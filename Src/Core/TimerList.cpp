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

void TimerList::swap(TimerList &other)
{
    if (this != &other) [[likely]]
    {
        TimerList temp = *this;
        *this = other;
        other = temp;
    }
}

void TimerList::pushFront(TimerPrivate *pTimer)
{
    assert(pTimer);
    pTimer->m_listNode.pNext = m_pHead;
    pTimer->m_listNode.pPrevious = nullptr;
    if (!isEmpty()) [[likely]]
    {
        m_pHead->pPrevious = &pTimer->m_listNode;
        m_pHead = &pTimer->m_listNode;
    }
    else [[unlikely]]
    {
        m_pHead = &pTimer->m_listNode;
        m_pTail = m_pHead;
    }
    ++m_size;
}

void TimerList::pushFront(TimerList timers)
{
    if (this != &timers) [[likely]]
    {
        if (timers.isEmpty()) [[unlikely]]
            return;
        else if (isEmpty()) [[unlikely]]
            *this = timers;
        else [[likely]]
        {
            timers.m_pTail->pNext = m_pHead;
            m_pHead->pPrevious = timers.m_pTail;
            m_pHead = timers.m_pHead;
            m_size += timers.m_size;
        }
    }
}

void TimerList::remove(TimerPrivate *pTimer)
{
    assert(pTimer);
    --m_size;
    if (pTimer->m_listNode.pPrevious)
        pTimer->m_listNode.pPrevious->pNext = pTimer->m_listNode.pNext;
    if (pTimer->m_listNode.pNext)
        pTimer->m_listNode.pNext->pPrevious = pTimer->m_listNode.pPrevious;
    if (m_pHead == &pTimer->m_listNode)
        m_pHead = m_pHead->pNext;
    if (m_pTail == &pTimer->m_listNode)
        m_pTail = m_pTail->pPrevious;
    pTimer->m_listNode.pNext = nullptr;
    pTimer->m_listNode.pPrevious = nullptr;
}

TimerPrivate *TimerList::popFirst()
{
    TimerPrivate *pTimer = nullptr;
    if (m_size > 0) [[likely]]
    {
        pTimer = m_pHead->pTimer;
        m_pHead = m_pHead->pNext;
        pTimer->m_listNode.pNext = nullptr;
        pTimer->m_listNode.pPrevious = nullptr;
        --m_size;
    }
    return pTimer;
}

}
