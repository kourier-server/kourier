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

#include "TimerPrivate_epoll.h"
#include "EpollEventNotifier.h"
#include <QDateTime>


namespace Kourier
{

TimerPrivate::TimerPrivate() :
    m_pEventNotifier(EpollEventNotifier::current())
{
    m_listNode.pTimer = this;
}

void TimerPrivate::setInterval(std::chrono::milliseconds interval)
{
    switch (m_state)
    {
    case State::Active:
        activateTimer(interval);
        break;
    case State::Inactive:
        m_interval = interval;
        break;
    }
}

void TimerPrivate::activateTimer(std::chrono::milliseconds interval)
{
    m_interval = interval;
    m_pEventNotifier->registerTimer(this);
}

void TimerPrivate::deactivateTimer()
{
    m_pEventNotifier->unregisterTimer(this);
}

void TimerPrivate::processTimeout()
{
    Q_Q(Timer);
    m_state = State::Inactive;
    if (!isSingleShot())
        start();
    q->timeout();
}

}
