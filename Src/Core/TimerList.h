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

#ifndef KOURIER_TIMER_LIST_H
#define KOURIER_TIMER_LIST_H

#include "TimerPrivate_epoll.h"


namespace Kourier
{

class TimerList
{
public:
    TimerList();
    ~TimerList() = default;
    void swap(TimerList &other);
    void addTimer(TimerPrivate *pTimer);
    void removeTimer(TimerPrivate *pTimer);
    TimerPrivate *popFirst();
    inline bool isEmpty() const {return m_size == 0;}
    inline uint64_t size() const {return m_size;}

private:
    TimerListNode m_head;
    TimerListNode m_tail;
    uint64_t m_size = 0;
};

}

#endif // KOURIER_TIMER_LIST_H
