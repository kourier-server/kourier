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

#ifndef KOURIER_TIMER_LIST_ITERATOR_H
#define KOURIER_TIMER_LIST_ITERATOR_H

#include "TimerListNode.h"

namespace Kourier
{

class TimerListIterator
{
public:
    TimerListIterator() = default;
    ~TimerListIterator() = default;
    TimerPrivate *timer()
    {
        if (m_pNode) [[likely]]
            return m_pNode->pTimer;
        else [[unlikely]]
            return nullptr;
    }
    inline bool operator==(const TimerListIterator &it) {return (m_pNode == it.m_pNode);}
    inline bool operator!=(const TimerListIterator &it) {return !(*this == it);}
    inline TimerListIterator& operator++()
    {
        if (m_pNode) [[likely]]
            m_pNode = m_pNode->pNext;
        return *this;
    }

private:
    inline constexpr TimerListIterator(TimerListNode *pNode) {m_pNode = pNode;}

private:
    TimerListNode *m_pNode = nullptr;
    friend class TimerList;
};

}

#endif // KOURIER_TIMER_LIST_ITERATOR_H
