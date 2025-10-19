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
      m_lowResolutionTime(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())),
      m_pLowResolutionClockTicker(std::make_shared<ClockTicker>(std::chrono::milliseconds(64))),
      m_pHighResolutionClockTicker(std::make_shared<ClockTicker>(std::chrono::milliseconds(1)))
{
    if (-1 == m_eventFd)
        qFatal("Failed to create event for epoll-based event dispatcher. Exiting.");
    if (!m_pLowResolutionClockTicker) [[unlikely]]
        qFatal("Failed to create timer wheels. Given low resolution clock ticker is null.");
    if (!m_pHighResolutionClockTicker) [[unlikely]]
        qFatal("Failed to create timer wheels. Given high resolution clock ticker is null.");
    Object::connect(m_pLowResolutionClockTicker.get(), &ClockTicker::tick, this, &TimerNotifier::onLowResolutionTick);
    Object::connect(m_pHighResolutionClockTicker.get(), &ClockTicker::tick, this, &TimerNotifier::onHighResolutionTick);
    m_pLowResolutionClockTicker->setEnabled(true);
}

TimerNotifier::TimerNotifier(std::shared_ptr<ClockTicker> pLowResolutionClockTicker,
                             std::shared_ptr<ClockTicker> pHighResolutionClockTicker)
    : EpollEventSource(EPOLLET | EPOLLIN, EpollEventNotifier::current()),
      m_eventFd(eventfd(0, EFD_NONBLOCK)),
      m_lowResolutionTime(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())),
      m_pLowResolutionClockTicker(pLowResolutionClockTicker),
      m_pHighResolutionClockTicker(pHighResolutionClockTicker)

{
    if (-1 == m_eventFd)
        qFatal("Failed to create event for epoll-based event dispatcher. Exiting.");
    if (!m_pLowResolutionClockTicker) [[unlikely]]
        qFatal("Failed to create timer wheels. Given low resolution clock ticker is null.");
    if (!m_pHighResolutionClockTicker) [[unlikely]]
        qFatal("Failed to create timer wheels. Given high resolution clock ticker is null.");
    Object::connect(m_pLowResolutionClockTicker.get(), &ClockTicker::tick, this, &TimerNotifier::onLowResolutionTick);
    Object::connect(m_pHighResolutionClockTicker.get(), &ClockTicker::tick, this, &TimerNotifier::onHighResolutionTick);
    m_pLowResolutionClockTicker->setEnabled(true);
}

TimerNotifier::~TimerNotifier()
{
    setEnabled(false);
    UnixUtils::safeClose(m_eventFd);
}

void TimerNotifier::addTimer(TimerPrivate *pTimer)
{
    assert(pTimer);
    const uint8_t idxFinder[42] = {0,0,0,0,0,0,
                                   1,1,1,1,1,1,
                                   2,2,2,2,2,2,
                                   3,3,3,3,3,3,
                                   4,4,4,4,4,4,
                                   5,5,5,5,5,5,
                                   6,6,6,6,6,6};
    if (pTimer->interval().count() == 0) [[unlikely]]
    {
        triggerTimerWhenControlReturnsToEventLoop(pTimer);
        return;
    }
    else if (pTimer->timeout().count() > maxTimeout) [[unlikely]]
        pTimer->timeout() = std::chrono::milliseconds(maxTimeout);
    m_timerWheels[idxFinder[std::countr_zero<uint64_t>(std::bit_floor<uint64_t>(pTimer->timeout().count()))]].addTimer(pTimer);
    setHighResolutionClockTickerEnabled(!m_timerWheels[0].isEmpty());
}

void TimerNotifier::removeTimer(TimerPrivate *pTimer)
{
    assert(pTimer);
    if (pTimer->m_pTimerWheel) [[likely]]
    {
        pTimer->m_pTimerWheel->removeTimer(pTimer);
        setHighResolutionClockTickerEnabled(!m_timerWheels[0].isEmpty());
    }
    else
        m_timersToNotify.remove(pTimer);
}

void Kourier::TimerNotifier::onLowResolutionTick()
{
    m_lowResolutionTime += std::chrono::milliseconds(64);
    ++m_lowResolutionTickCounter;
    uint8_t largestWheelToTick = 1;
    if (isMultipleOfPowerOfTwo(m_lowResolutionTickCounter, 30))
        largestWheelToTick = 6;
    else if (isMultipleOfPowerOfTwo(m_lowResolutionTickCounter, 24))
        largestWheelToTick = 5;
    else if (isMultipleOfPowerOfTwo(m_lowResolutionTickCounter, 18))
        largestWheelToTick = 4;
    else if (isMultipleOfPowerOfTwo(m_lowResolutionTickCounter, 12))
        largestWheelToTick = 3;
    else if (isMultipleOfPowerOfTwo(m_lowResolutionTickCounter, 6))
        largestWheelToTick = 2;
    else
        largestWheelToTick = 1;
    for (auto i = largestWheelToTick; i > 1; --i)
        processExpiredTimers(m_timerWheels[i].tick());
    auto expiredTimers = m_timerWheels[1].tick();
    TimerList timers;
    for (auto it = expiredTimers.begin(); it != expiredTimers.end(); ++it)
    {
        if (it.timer()->timeout().count() > 0)
            m_timerWheels[0].addTimer(it.timer());
        else
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

}
