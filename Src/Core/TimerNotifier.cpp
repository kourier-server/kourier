//
// Copyright (C) 2025 Glauco Pacheco <glauco@kourier.io>
// SPDX-License-Identifier: AGPL-3.0-only
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, version 3 of the License.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#include "TimerNotifier.h"
#include "EpollEventNotifier.h"
#include "UnixUtils.h"
#include <bit>
#include <sys/eventfd.h>


namespace Kourier
{

TimerNotifier::TimerNotifier()
    : EpollEventSource(EPOLLET | EPOLLIN, EpollEventNotifier::current()),
      m_eventFd(eventfd(0, EFD_NONBLOCK)),
      m_lowResolutionTime(msSinceEpoch()),
      m_pLowResolutionClockTicker(std::make_shared<ClockTicker>(std::chrono::milliseconds(64))),
      m_pHighResolutionClockTicker(std::make_shared<ClockTicker>(std::chrono::milliseconds(1)))
{
    if (-1 == m_eventFd)
        qFatal("Failed to create event for epoll-based event dispatcher. Exiting.");
    if (!m_pLowResolutionClockTicker) [[unlikely]]
        qFatal("Failed to create timer notifier. Given low resolution clock ticker is null.");
    if (!m_pHighResolutionClockTicker) [[unlikely]]
        qFatal("Failed to create timer notifier. Given high resolution clock ticker is null.");
    Object::connect(m_pLowResolutionClockTicker.get(), &ClockTicker::tick, this, &TimerNotifier::onLowResolutionTick);
    Object::connect(m_pHighResolutionClockTicker.get(), &ClockTicker::tick, this, &TimerNotifier::onHighResolutionTick);
    m_pHighResolutionClockTicker->setEnabled(false);
    m_pLowResolutionClockTicker->setEnabled(false);
    m_pLowResolutionClockTicker->setEnabled(true);
}

TimerNotifier::TimerNotifier(std::shared_ptr<ClockTicker> pLowResolutionClockTicker,
                             std::shared_ptr<ClockTicker> pHighResolutionClockTicker)
    : EpollEventSource(EPOLLET | EPOLLIN, EpollEventNotifier::current()),
      m_eventFd(eventfd(0, EFD_NONBLOCK)),
      m_lowResolutionTime(msSinceEpoch()),
      m_pLowResolutionClockTicker(pLowResolutionClockTicker),
      m_pHighResolutionClockTicker(pHighResolutionClockTicker)

{
    if (-1 == m_eventFd)
        qFatal("Failed to create event for epoll-based event dispatcher. Exiting.");
    if (!m_pLowResolutionClockTicker) [[unlikely]]
        qFatal("Failed to create timer notifier. Given low resolution clock ticker is null.");
    if (!m_pHighResolutionClockTicker) [[unlikely]]
        qFatal("Failed to create timer notifier. Given high resolution clock ticker is null.");
    Object::connect(m_pLowResolutionClockTicker.get(), &ClockTicker::tick, this, &TimerNotifier::onLowResolutionTick);
    Object::connect(m_pHighResolutionClockTicker.get(), &ClockTicker::tick, this, &TimerNotifier::onHighResolutionTick);
    m_pHighResolutionClockTicker->setEnabled(false);
    m_pLowResolutionClockTicker->setEnabled(false);
    m_pLowResolutionClockTicker->setEnabled(true);
}

TimerNotifier::~TimerNotifier()
{
    disable();
}

void TimerNotifier::addTimer(TimerPrivate *pTimer)
{
    assert(pTimer);
    if (!m_isEnabled) [[unlikely]]
        return;
    const uint8_t wheelIdxMap[42] = {0,0,0,0,0,0,
                                     1,1,1,1,1,1,
                                     2,2,2,2,2,2,
                                     3,3,3,3,3,3,
                                     4,4,4,4,4,4,
                                     5,5,5,5,5,5,
                                     6,6,6,6,6,6};
    auto timeout = pTimer->interval();
    if (timeout.count() > 0) [[likely]]
    {
        switch(pTimer->timerType())
        {
            case Timer::TimerType::Coarse: [[likely]]
            case Timer::TimerType::VeryCoarse: [[unlikely]]
                if (timeout.count() >= 4096)
                  break;
            case Timer::TimerType::Precise: [[unlikely]]
                if (timeout >= std::chrono::milliseconds(64))
                {
                    const auto currentTime = msSinceEpoch();
                    const auto timeFix = (currentTime >= m_lowResolutionTime) ? (currentTime - m_lowResolutionTime) : std::chrono::milliseconds(0);
                    timeout += timeFix;
                }
                break;
        }
    }
    auto * const pTimerWheel = &(m_timerWheels[wheelIdxMap[std::countr_zero<uint64_t>(std::bit_floor<uint64_t>(timeout.count()))]]);
    const auto idxSlot = pTimerWheel->getSlotIdx(timeout);
    if (pTimer->m_state == TimerPrivate::State::Active)
    {
        if (timeout == pTimer->m_timeout
            && pTimerWheel == pTimer->m_pTimerWheel
            && idxSlot == pTimer->m_idxTimerWheelSlot)
            return;
        else
            removeTimer(pTimer);
    }
    pTimer->m_timeout = timeout;
    addAdjustedTimer(pTimer);
}

void TimerNotifier::removeTimer(TimerPrivate *pTimer)
{
    assert(pTimer);
    if (!m_isEnabled || pTimer->m_state != TimerPrivate::State::Active)
        return;
    pTimer->m_state = TimerPrivate::State::Inactive;
    if (pTimer->m_pTimerWheel) [[likely]]
    {
        pTimer->m_pTimerWheel->removeTimer(pTimer);
        setHighResolutionClockTickerEnabled(!m_timerWheels[0].isEmpty());
    }
    else
        m_timersToNotify.remove(pTimer);
    pTimer->m_pTimerWheel = nullptr;
    pTimer->m_idxTimerWheelSlot = 0;
}

void TimerNotifier::addAdjustedTimer(TimerPrivate *pTimer)
{
    assert(pTimer);
    const uint8_t wheelIdxMap[42] = {0,0,0,0,0,0,
                                     1,1,1,1,1,1,
                                     2,2,2,2,2,2,
                                     3,3,3,3,3,3,
                                     4,4,4,4,4,4,
                                     5,5,5,5,5,5,
                                     6,6,6,6,6,6};
    if (pTimer->timeout().count() == 0) [[unlikely]]
    {
        pTimer->m_state = TimerPrivate::State::Active;
        triggerTimerWhenControlReturnsToEventLoop(pTimer);
        return;
    }
    else if (pTimer->timeout().count() > maxTimeout) [[unlikely]]
        pTimer->timeout() = std::chrono::milliseconds(maxTimeout);
    pTimer->m_state = TimerPrivate::State::Active;
    m_timerWheels[wheelIdxMap[std::countr_zero<uint64_t>(std::bit_floor<uint64_t>(pTimer->timeout().count()))]].addTimer(pTimer);
    setHighResolutionClockTickerEnabled(!m_timerWheels[0].isEmpty());
}

void Kourier::TimerNotifier::onLowResolutionTick()
{
    m_lowResolutionTime += std::chrono::milliseconds(64);
    ++m_lowResolutionTickCounter;
    const uint8_t wheelIdxMap[42] = {0,0,0,0,0,0,
                                     1,1,1,1,1,1,
                                     2,2,2,2,2,2,
                                     3,3,3,3,3,3,
                                     4,4,4,4,4,4,
                                     5,5,5,5,5,5,
                                     6,6,6,6,6,6};
    const auto largestWheelToTick = wheelIdxMap[std::countr_zero<uint64_t>(std::bit_floor<uint64_t>(m_lowResolutionTickCounter << 6))];
    for (auto i = largestWheelToTick; i > 1; --i)
        processExpiredTimers(m_timerWheels[i].tick());
    auto expiredTimers = m_timerWheels[1].tick();
    TimerList timers;
    for (auto it = expiredTimers.begin(); it != expiredTimers.end(); ++it)
    {
        if (it.timer()->timeout().count() > 0) [[likely]]
            m_timerWheels[0].addTimer(it.timer());
        else [[unlikely]]
            timers.pushFront(it.timer());
    }
    setHighResolutionClockTickerEnabled(!m_timerWheels[0].isEmpty());
    if (!timers.isEmpty())
    {
        m_timersToNotify.pushFront(timers);
        notifyTimers();
    }
}

void TimerNotifier::onHighResolutionTick()
{
    auto expiredTimers = m_timerWheels[0].tick();
    setHighResolutionClockTickerEnabled(!m_timerWheels[0].isEmpty());
    if (!expiredTimers.isEmpty())
    {
        m_timersToNotify.pushFront(expiredTimers);
        notifyTimers();
    }
}

void TimerNotifier::set()
{
    if (m_eventIsSet)
        return;
    else
    {
        setEnabled(true);
        m_eventIsSet = true;
        const uint64_t value = 1;
        UnixUtils::safeWrite(m_eventFd, (const char*)&value, sizeof(value));
    }
}

void TimerNotifier::reset()
{
    if (!m_eventIsSet)
        return;
    else
    {
        m_eventIsSet = false;
        uint64_t value = 0;
        UnixUtils::safeRead(m_eventFd, (char*)&value, sizeof(value));
    }
}

void TimerNotifier::onEvent(uint32_t epollEvents)
{
    if (EPOLLIN == (epollEvents & EPOLLIN))
    {
        reset();
        notifyTimers();
    }
}

void TimerNotifier::notifyTimers()
{
    if (!m_isNotifyingTimers) [[likely]]
        m_isNotifyingTimers = true;
    else [[unlikely]]
        return;
    while (!m_timersToNotify.isEmpty())
    {
        auto *pTimer = m_timersToNotify.popFirst();
        pTimer->processTimeout();
    }
    m_isNotifyingTimers = false;
}

void TimerNotifier::disable()
{
    if (m_isEnabled)
    {
        m_isEnabled = false;
        setEnabled(false);
        UnixUtils::safeClose(m_eventFd);
        m_pLowResolutionClockTicker->setEnabled(false);
        Object::disconnect(m_pLowResolutionClockTicker.get(), &ClockTicker::tick, this, &TimerNotifier::onLowResolutionTick);
        m_pHighResolutionClockTicker->setEnabled(false);
        Object::disconnect(m_pHighResolutionClockTicker.get(), &ClockTicker::tick, this, &TimerNotifier::onHighResolutionTick);
    }
}

}
