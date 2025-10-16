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

#include "TimerList.h"


namespace Kourier
{

TimerList::TimerList()
{
    m_head.pNext = &m_tail;
    m_tail.pPrevious = &m_head;
}

void TimerList::swap(TimerList &other)
{
    if (this != &other) [[likely]]
    {
        TimerList temp = *this;
        *this = other;
        other = temp;
        if (!isEmpty())
        {
            m_head.pNext->pPrevious = &m_head;
            m_tail.pPrevious->pNext = &m_tail;
        }
        else
        {
            m_head.pNext = &m_tail;
            m_tail.pPrevious = &m_head;
        }
        if (!other.isEmpty())
        {
            other.m_head.pNext->pPrevious = &other.m_head;
            other.m_tail.pPrevious->pNext = &other.m_tail;
        }
        else
        {
            other.m_head.pNext = &other.m_tail;
            other.m_tail.pPrevious = &other.m_head;
        }
    }
}

void TimerList::pushFront(TimerPrivate *pTimer)
{
    assert(pTimer);
    ++m_size;
    pTimer->m_listNode.pNext = m_head.pNext;
    pTimer->m_listNode.pPrevious = &m_head;
    m_head.pNext = &pTimer->m_listNode;
    pTimer->m_listNode.pNext->pPrevious = &pTimer->m_listNode;
}

void TimerList::remove(TimerPrivate *pTimer)
{
    assert(pTimer);
    --m_size;
    pTimer->m_listNode.pPrevious->pNext = pTimer->m_listNode.pNext;
    pTimer->m_listNode.pNext->pPrevious = pTimer->m_listNode.pPrevious;
}

TimerPrivate *TimerList::popFirst()
{
    TimerPrivate *pTimer = nullptr;
    if (m_size > 0) [[likely]]
    {
        pTimer = m_head.pNext->pTimer;
        remove(pTimer);
    }
    return pTimer;
}

}
