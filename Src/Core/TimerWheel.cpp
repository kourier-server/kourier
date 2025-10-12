//
// Copyright (C) 2023 Glauco Pacheco <glauco@kourier.io>
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

#include "TimerWheel.h"
#include "TimerPrivate_epoll.h"
#include "UnixUtils.h"
#include <sys/timerfd.h>


namespace Kourier
{

TimerWheel::TimerWheel(std::chrono::nanoseconds resolution) :
    EpollEventSource(EPOLLET | EPOLLIN, EpollEventNotifier::current()),
    m_wheelTimerFd(timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK)),
    m_resolution(resolution > std::chrono::nanoseconds(0) ? resolution : std::chrono::nanoseconds(1))
{
    if (m_wheelTimerFd < 0)
        qFatal("Failed to create a timer object. Exiting.");
}

TimerWheel::~TimerWheel()
{
    setEnabled(false);
    UnixUtils::safeClose(m_wheelTimerFd);
}

void TimerWheel::onEvent(const uint32_t epollEvents)
{
    if (epollEvents & EPOLLIN)
    {
        uint64_t slicesSinceLastTimeout = 0;
        UnixUtils::safeRead(fileDescriptor(), (char*)&slicesSinceLastTimeout, sizeof(slicesSinceLastTimeout));
        processExpiredTimers(slicesSinceLastTimeout);
    }
}

bool TimerWheel::add(TimerPrivate *pTimer)
{
    return false;
    // assert(pTimer);
    // if (m_activeTimersCount == 0)
    //     activateInternalTimer();
    // const auto timeoutInSlices = (pTimer->interval() >> 9) + 1 + m_nextTimeoutInSlices;
    // if (pTimer->m_state == TimerPrivate::State::Active)
    // {
    //     if (timeoutInSlices == pTimer->timeoutInSlices())
    //         return;
    //     else
    //         remove(pTimer);
    // }
    // ++m_activeTimersCount;
    // pTimer->m_state = TimerPrivate::State::Active;
    // pTimer->setTimeoutInSlices(timeoutInSlices);
    // auto *&pHead = m_activeTimersPerSlot[timeoutInSlices & 127];
    // pTimer->m_pNext = pHead;
    // pTimer->m_pPrevious = nullptr;
    // if (pHead)
    //     pHead->m_pPrevious = pTimer;
    // pHead = pTimer;
}

bool TimerWheel::remove(TimerPrivate *pTimer)
{
    return false;
    // assert(pTimer);
    // if (pTimer->m_state != TimerPrivate::State::Active)
    //     return;
    // --m_activeTimersCount;
    // pTimer->m_state = TimerPrivate::State::Inactive;
    // if (pTimer->m_pPrevious)
    //     pTimer->m_pPrevious->m_pNext = pTimer->m_pNext;
    // if (pTimer->m_pNext)
    //     pTimer->m_pNext->m_pPrevious = pTimer->m_pPrevious;
    // auto *&pHead = m_activeTimersPerSlot[pTimer->timeoutInSlices() & 127];
    // if (pHead == pTimer)
    //     pHead = pHead->m_pNext;
    // if (m_pNextTimerToExpire == pTimer)
    //     m_pNextTimerToExpire = m_pNextTimerToExpire->m_pNext;
}

void TimerWheel::activateInternalTimer()
{
    if (m_isInternalTimerActive)
        return;
    m_isInternalTimerActive = true;
    //m_nextTimeoutInSlices = 1;
    // Setting time
    struct itimerspec newValue{0,0,0,0};
    struct itimerspec oldValue{0,0,0,0};
    newValue.it_value.tv_nsec = 512000000;
    newValue.it_interval.tv_nsec = 512000000;
    if (-1 == timerfd_settime(m_wheelTimerFd, 0, &newValue, &oldValue))
        qFatal("Failed to set timer. Exiting.");
}

void TimerWheel::deactivateInternalTimer()
{
    if (!m_isInternalTimerActive)
        return;
    m_isInternalTimerActive = false;
    uint64_t tmp = 0;
    UnixUtils::safeRead(fileDescriptor(), (char*)&tmp, sizeof(tmp));
    struct itimerspec newValue{0,0,0,0};
    struct itimerspec oldValue{0,0,0,0};
    if (-1 == timerfd_settime(m_wheelTimerFd, 0, &newValue, &oldValue))
        qFatal("Failed to set timer. Exiting.");
}

void TimerWheel::processExpiredTimers(uint64_t slicesSinceLastTimeout)
{
    // const auto lastTimeoutInSlices = m_nextTimeoutInSlices;
    // m_nextTimeoutInSlices += slicesSinceLastTimeout;
    // for (auto timeoutInSlices = lastTimeoutInSlices; timeoutInSlices < m_nextTimeoutInSlices; ++timeoutInSlices)
    // {
    //     auto *pTimer = m_activeTimersPerSlot[timeoutInSlices & 127];
    //     while (pTimer != nullptr)
    //     {
    //         if (pTimer->timeoutInSlices() > timeoutInSlices)
    //         {
    //             pTimer = pTimer->m_pNext;
    //             continue;
    //         }
    //         else
    //         {
    //             m_pNextTimerToExpire = pTimer;
    //             remove(pTimer);
    //             pTimer->processTimeout();
    //             pTimer = m_pNextTimerToExpire;
    //             m_pNextTimerToExpire = nullptr;
    //         }
    //     }
    // }
    // if (m_activeTimersCount == 0)
    //     deactivateInternalTimer();
}

}
