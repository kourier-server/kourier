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

#include "TimerPrivate_epoll.h"
#include <cstdint>


namespace Kourier
{

class TimerWheel
{
public:
    TimerWheel(uint64_t slotInterval);
    ~TimerWheel() = default;
    inline void add(TimerPrivate *pTimer) {m_slots[(uint64_t)pTimer->m_timeout.count() & m_slotFinderMask].addTimer(pTimer);}
    TimerList tick();

private:
    const uint64_t m_slotInterval;
    uint64_t m_slotFinderMask = 0;
    uint64_t m_idxNextTimersToExpire = 0;
    TimerList m_slots[64];
};

}

#endif // KOURIER_TIMER_WHEEL_H
