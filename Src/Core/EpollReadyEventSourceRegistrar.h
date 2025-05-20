//
// Copyright (C) 2024 Glauco Pacheco <glauco@kourier.io>
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

#ifndef KOURIER_EPOLL_READY_EVENT_SOURCE_REGISTRAR_H
#define KOURIER_EPOLL_READY_EVENT_SOURCE_REGISTRAR_H

#include "EpollEventSource.h"


namespace Kourier
{

class EpollReadyEventSourceRegistrar : public EpollEventSource
{
KOURIER_OBJECT(Kourier::EpollReadyEventSourceRegistrar)
public:
    explicit EpollReadyEventSourceRegistrar(EpollEventNotifier *pEventNotifier);
    EpollReadyEventSourceRegistrar();
    ~EpollReadyEventSourceRegistrar() override;
    int64_t fileDescriptor() const override {return m_eventFd;}
    void addReadyEvent(EpollEventSource *pEpollEventSource, uint32_t events);
    void removeReadyEvent(EpollEventSource *pEpollEventSource);

private:
    void set();
    void reset();
    void onEvent(uint32_t epollEvents) override;

private:
    const int m_eventFd = -1;
    EpollEventSource *m_pReadyEvents = nullptr;
    EpollEventSource *m_pEventsBeingTriggered = nullptr;
    bool m_eventIsSet = false;
};

}

#endif // KOURIER_EPOLL_READY_EVENT_SOURCE_REGISTRAR_H
