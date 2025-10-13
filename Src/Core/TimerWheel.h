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

#ifndef KOURIER_TIMER_WHEEL_H
#define KOURIER_TIMER_WHEEL_H

#include "EpollEventSource.h"
#include <chrono>
#include <list>


namespace Kourier
{

class TimerPrivate;

class TimerWheel : public EpollEventSource
{
KOURIER_OBJECT(Kourier::TimerWheel);
public:
    TimerWheel(std::chrono::nanoseconds resolution);
    ~TimerWheel() override;
    int64_t fileDescriptor() const override {return m_wheelTimerFd;}
    bool add(TimerPrivate *pTimer);
    bool remove(TimerPrivate *pTimer);
    inline std::chrono::nanoseconds resolution() const {return m_resolution;}
    inline std::chrono::nanoseconds period() const {return std::chrono::nanoseconds(m_resolution.count() << 6);}
    Signal timersExpired(std::list<TimerPrivate*> expiredTimers);

private:
    void onEvent(uint32_t epollEvents) override;

private:
    void activateInternalTimer();
    void deactivateInternalTimer();
    void processExpiredTimers(uint64_t slicesSinceLastTimeout);

private:
    const qint64 m_wheelTimerFd = -1;
    const std::chrono::nanoseconds m_resolution;
    size_t m_activeTimersCount = 0;
    std::list<TimerPrivate*> m_wheel[64];
    size_t m_idxNextTimersToExpire = 0;
    bool m_isInternalTimerActive = false;
};

}

#endif // KOURIER_TIMER_WHEEL_H
