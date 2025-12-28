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

#include "ClockTicker.h"
#include "UnixUtils.h"
#include "EpollEventNotifier.h"
#include <chrono>
#include <sys/timerfd.h>


namespace Kourier
{

ClockTicker::ClockTicker(std::chrono::milliseconds resolution) :
    EpollEventSource(EPOLLET | EPOLLIN, EpollEventNotifier::current()),
    m_resolution(resolution),
    m_timerFd(timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK))
{
    if (m_resolution.count() <= 0) [[unlikely]]
        qFatal("ClockTicker::ClockTicker failed. Resolution must be a positive value. Exiting.");
    if (m_timerFd < 0) [[unlikely]]
        qFatal("ClockTicker::ClockTicker failed. Failed to create a timer object. Exiting.");
    EpollEventSource::setEnabled(true);
}

ClockTicker::~ClockTicker()
{
    EpollEventSource::setEnabled(false);
    UnixUtils::safeClose(m_timerFd);
}

Signal ClockTicker::tick() KOURIER_SIGNAL(&ClockTicker::tick)

void ClockTicker::setEnabled(bool enabled)
{
    if (enabled != m_enabled)
    {
        m_enabled = enabled;
        m_enabled ? activateTicker() : deactivateTicker();
    }
}

void ClockTicker::onEvent(uint32_t epollEvents)
{
    if (epollEvents & EPOLLIN)
    {
        uint64_t expiryCount = 0;
        UnixUtils::safeRead(m_timerFd, (char*)&expiryCount, sizeof(expiryCount));
        for (auto i = 0; i < expiryCount; ++i)
        {
            m_currentTime += m_resolution;
            tick();
        }
    }
}

void ClockTicker::activateTicker()
{
    m_currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch());
    // Setting time
    struct itimerspec newValue{0,0,0,0};
    struct itimerspec oldValue{0,0,0,0};
    using SecType = decltype(newValue.it_value.tv_sec);
    using NSecType = decltype(newValue.it_value.tv_nsec);
    newValue.it_value.tv_sec = m_resolution.count()/static_cast<SecType>(1000);
    newValue.it_value.tv_nsec = (m_resolution.count()%static_cast<NSecType>(1000))*static_cast<NSecType>(1000000);
    newValue.it_interval.tv_sec = m_resolution.count()/static_cast<SecType>(1000);
    newValue.it_interval.tv_nsec = (m_resolution.count()%static_cast<NSecType>(1000))*static_cast<NSecType>(1000000);
    if (-1 == timerfd_settime(m_timerFd, 0, &newValue, &oldValue))
        qFatal("ClockTicker::activateTicker failed. Failed to set timer. Exiting.");
}

void ClockTicker::deactivateTicker()
{
    m_currentTime = std::chrono::milliseconds(0);
    uint64_t tmp = 0;
    UnixUtils::safeRead(m_timerFd, (char*)&tmp, sizeof(tmp));
    struct itimerspec newValue{0,0,0,0};
    struct itimerspec oldValue{0,0,0,0};
    if (-1 == timerfd_settime(m_timerFd, 0, &newValue, &oldValue))
        qFatal("ClockTicker::deactivateTicker failed. Failed to set timer. Exiting.");
}

}
