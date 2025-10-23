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
#include "TimerNotifier.h"
#include "NoDestroy.h"
#include <QDateTime>


namespace Kourier
{

class TimerNotifierDisabler
{
public:
    TimerNotifierDisabler(TimerNotifier *pTimerNotifier) : m_pTimerNotifier(pTimerNotifier)
    {
        if (!m_pTimerNotifier)
            qFatal("Failed to setup time notifier disabler. Given timer notifier is null.");
    }
    ~TimerNotifierDisabler() {m_pTimerNotifier->disable();}

private:
    TimerNotifier * const m_pTimerNotifier;
};

static TimerNotifier* currentTimerNotifier()
{
    static thread_local NoDestroy<TimerNotifier> timerNotifier;
    static thread_local TimerNotifierDisabler timerNotifierDisabler(&timerNotifier());
    return &(timerNotifier());
}

TimerPrivate::TimerPrivate() :
    m_pTimerNotifier(currentTimerNotifier())
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
        m_timeout = m_interval;
        break;
    }
}

void TimerPrivate::activateTimer(std::chrono::milliseconds interval)
{
    m_interval = interval;
    m_timeout = m_interval;
    m_pTimerNotifier->addTimer(this);
}

void TimerPrivate::deactivateTimer()
{
    m_pTimerNotifier->removeTimer(this);
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
