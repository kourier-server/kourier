//
// Copyright (C) 2025 Glauco Pacheco <glaucopacheco@gmail.com>
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
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#ifndef KOURIER_TIMER_LIST_H
#define KOURIER_TIMER_LIST_H

#include "TimerPrivate_epoll.h"
#include "TimerListIterator.h"


namespace Kourier
{

class TimerList
{
public:
    TimerList() = default;
    TimerList(const TimerList&) = delete;
    ~TimerList() {clear();}
    TimerList &operator=(const TimerList&) = delete;
    void swap(TimerList &other);
    void pushFront(TimerPrivate *pTimer);
    void pushFront(TimerList &timers);
    void remove(TimerPrivate *pTimer);
    TimerPrivate *popFirst();
    void clear() {while (!isEmpty()) {popFirst();}}
    inline bool isEmpty() const {return m_size == 0;}
    inline uint64_t size() const {return m_size;}
    inline TimerListIterator begin() {return {m_pHead};}
    inline TimerListIterator end() {return {nullptr};}

private:
    TimerListNode *m_pHead = nullptr;
    TimerListNode *m_pTail = nullptr;
    uint64_t m_size = 0;
};

}

#endif // KOURIER_TIMER_LIST_H
