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

#ifndef KOURIER_EPOLL_OBJECT_DELETER_H
#define KOURIER_EPOLL_OBJECT_DELETER_H

#include "EpollEventSource.h"


namespace Kourier
{

class Object;

class EpollObjectDeleter : public EpollEventSource
{
KOURIER_OBJECT(Kourier::EpollObjectDeleter)
public:
    explicit EpollObjectDeleter(EpollEventNotifier *pEventNotifier);
    EpollObjectDeleter();
    ~EpollObjectDeleter() override;
    int64_t fileDescriptor() const override {return m_eventFd;}
    void scheduleForDeletion(Object *pObject);

private:
    void set();
    void reset();
    void onEvent(uint32_t epollEvents) override;
    void deleteScheduledObjects();

private:
    const int m_eventFd = -1;
    std::vector<Object*> m_objectsToDelete;
    bool m_eventIsSet = false;
};

}

#endif // KOURIER_EPOLL_OBJECT_DELETER_H
