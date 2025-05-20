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

#ifndef KOURIER_EPOLL_TIMER_REGISTRAR_H
#define KOURIER_EPOLL_TIMER_REGISTRAR_H

#include "EpollEventSource.h"
#include <QVector>


namespace Kourier
{

class TimerPrivate;

class EpollTimerRegistrar : public EpollEventSource
{
KOURIER_OBJECT(Kourier::EpollTimerRegistrar);
public:
    EpollTimerRegistrar(EpollEventNotifier *pEventNotifier);
    EpollTimerRegistrar();
    ~EpollTimerRegistrar() override;
    int64_t fileDescriptor() const override {return m_mainTimerFd;}
    void add(TimerPrivate *pTimer);
    void remove(TimerPrivate *pTimer);

private:
    void onEvent(uint32_t epollEvents) override;

private:
    void activateInternalTimer();
    void deactivateInternalTimer();
    void processActiveTimers(uint64_t slicesSinceLastTimeout);

private:
    const qint64 m_mainTimerFd = -1;
    size_t m_activeTimersCount = 0;
    TimerPrivate *m_activeTimersPerSlot[128] = {0};
    TimerPrivate *m_pNextTimerToExpire = nullptr;
    qint64 m_nextTimeoutInSlices = 0;
    bool m_isInternalTimerActive = false;
};

}

#endif // KOURIER_EPOLL_TIMER_REGISTRAR_H
