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

#include "EpollEventSource.h"
#include "UnixUtils.h"
#include <QCoreApplication>
#include <Spectator.h>
#include <sys/eventfd.h>
#include <memory.h>

using Kourier::EpollEventSource;
using Kourier::EpollEventNotifier;
using Kourier::UnixUtils;


class EpollEventSourceTest : public EpollEventSource
{
public:
    EpollEventSourceTest(size_t &eventCount) :
        EpollEventSource(0),
        m_fileDescriptor(::eventfd(0, EFD_NONBLOCK)),
        m_eventCount(eventCount)
    {
        REQUIRE(m_fileDescriptor >= 0);
    }

    EpollEventSourceTest(uint32_t events,
                         size_t &eventCount) :
        EpollEventSource(events),
        m_fileDescriptor(::eventfd(0, EFD_NONBLOCK)),
        m_eventCount(eventCount)
    {
        REQUIRE(m_fileDescriptor >= 0);
        setEnabled(true);
    }
    ~EpollEventSourceTest() override
    {
        setEnabled(false);
        if (!m_keepFileDescriptor)
            UnixUtils::safeClose(m_fileDescriptor);
    }
    inline void setKeepFileDescriptor(bool keep) {m_keepFileDescriptor = keep;}
    int64_t fileDescriptor() const override {return m_fileDescriptor;}
    inline size_t eventCount() const {return m_eventCount;}
    inline uint32_t epollEvents() const {return m_epollEvents;}
    inline void setEventToBeDeleted(std::unique_ptr<EpollEventSourceTest> *pEvent) {m_pOtherEventSourceToBeDeleted = pEvent;}

private:
    void onEvent(uint32_t epollEvents) override
    {
        uint64_t buffer = 0;
        REQUIRE(sizeof(buffer) == UnixUtils::safeRead(fileDescriptor(), (char*)&buffer, sizeof(buffer)));
        ++m_eventCount;
        m_epollEvents = epollEvents;
        if (m_pOtherEventSourceToBeDeleted)
        {
            m_pOtherEventSourceToBeDeleted->reset(nullptr);
            m_pOtherEventSourceToBeDeleted = nullptr;
        }
    }

private:
    bool m_keepFileDescriptor = false;
    qint64 m_fileDescriptor = -1;
    size_t &m_eventCount;
    uint32_t m_epollEvents = 0;
    std::unique_ptr<EpollEventSourceTest> *m_pOtherEventSourceToBeDeleted = nullptr;
};


SCENARIO("EpollEventSource is informed when specified event type occurs")
{
    GIVEN("an epoll event source for a file descriptor-based event that is set to be informed when file descriptor is available for read operations")
    {
        const uint32_t eventTypes = EPOLLIN;
        size_t eventCount = 0;
        std::unique_ptr<EpollEventSourceTest> pEpollEventSource(new EpollEventSourceTest(eventTypes, eventCount));
        REQUIRE(pEpollEventSource != nullptr);
        REQUIRE(pEpollEventSource->eventCount() == 0);
        REQUIRE(pEpollEventSource->epollEvents() == 0);
        REQUIRE(pEpollEventSource->eventTypes() == eventTypes);

        WHEN("event is set")
        {
            const uint64_t counter = 1;
            REQUIRE(sizeof(counter) == UnixUtils::safeWrite(pEpollEventSource->fileDescriptor(), (char*)&counter, sizeof(counter)));

            THEN("onEvent method gets called")
            {
                QCoreApplication::processEvents();
                REQUIRE(pEpollEventSource->eventCount() == 1);
                REQUIRE(EPOLLIN == (pEpollEventSource->epollEvents() | EPOLLIN));
                REQUIRE(pEpollEventSource->eventTypes() == eventTypes);
            }
        }
    }
}


SCENARIO("EpollEventSource is informed when enabled in edge-triggered operation with ready descriptor")
{
    GIVEN("an epoll event source for a file descriptor-based event that is set")
    {
        const uint32_t eventTypes = EPOLLIN | EPOLLET;
        size_t eventCount = 0;
        std::unique_ptr<EpollEventSourceTest> pEpollEventSource(new EpollEventSourceTest(eventCount));
        const uint64_t counter = 1;
        REQUIRE(sizeof(counter) == UnixUtils::safeWrite(pEpollEventSource->fileDescriptor(), (char*)&counter, sizeof(counter)));
        REQUIRE(pEpollEventSource != nullptr);
        REQUIRE(pEpollEventSource->eventCount() == 0);
        REQUIRE(pEpollEventSource->epollEvents() == 0);
        REQUIRE(pEpollEventSource->eventTypes() == 0);
        pEpollEventSource->setEventTypes(eventTypes);
        REQUIRE(pEpollEventSource->eventTypes() == eventTypes);
        REQUIRE(!pEpollEventSource->isEnabled());

        WHEN("event source is enabled")
        {
            pEpollEventSource->setEnabled(true);

            THEN("onEvent method gets called once")
            {
                QCoreApplication::processEvents();
                REQUIRE(pEpollEventSource->eventCount() == 1);
                REQUIRE(EPOLLIN == (pEpollEventSource->epollEvents() | EPOLLIN));
                REQUIRE(pEpollEventSource->eventTypes() == eventTypes);

                AND_THEN("onEvent method does not get called twice")
                {
                    QCoreApplication::processEvents();
                    REQUIRE(pEpollEventSource->eventCount() == 1);
                    REQUIRE(EPOLLIN == (pEpollEventSource->epollEvents() | EPOLLIN));
                    REQUIRE(pEpollEventSource->eventTypes() == eventTypes);
                }
            }
        }
    }
}


SCENARIO("EpollEventSource event types can be changed")
{
    GIVEN("an epoll event source for a file descriptor-based event that is set to be informed when file descriptor is available for read operations")
    {
        const uint32_t eventTypes = EPOLLIN;
        size_t eventCount = 0;
        std::unique_ptr<EpollEventSourceTest> pEpollEventSource(new EpollEventSourceTest(eventTypes, eventCount));
        REQUIRE(pEpollEventSource != nullptr);
        REQUIRE(pEpollEventSource->eventCount() == 0);
        REQUIRE(pEpollEventSource->epollEvents() == 0);
        REQUIRE(pEpollEventSource->eventTypes() == eventTypes);

        WHEN("event is set")
        {
            const uint64_t counter = 1;
            REQUIRE(sizeof(counter) == UnixUtils::safeWrite(pEpollEventSource->fileDescriptor(), (const char*)&counter, sizeof(counter)));

            THEN("onEvent method gets called")
            {
                QCoreApplication::processEvents();
                REQUIRE(pEpollEventSource->eventCount() == 1);
                REQUIRE(EPOLLIN == (pEpollEventSource->epollEvents() | EPOLLIN));
                REQUIRE(pEpollEventSource->eventTypes() == eventTypes);

                AND_WHEN("event is set after epoll event source is set to not be informed when file descriptor is available for read operations")
                {
                    pEpollEventSource->setEventTypes(0);
                    REQUIRE(sizeof(counter) == UnixUtils::safeWrite(pEpollEventSource->fileDescriptor(), (const char*)&counter, sizeof(counter)));

                    THEN("onEvent method does not get called")
                    {
                        QCoreApplication::processEvents();
                        REQUIRE(pEpollEventSource->eventCount() == 1);
                        REQUIRE(pEpollEventSource->eventTypes() == 0);
                    }
                }
            }
        }
    }
}


SCENARIO("EpollEventSource can be deleted")
{
    GIVEN("an epoll event source for a file descriptor-based event that is set to be informed when file descriptor is available for read operations")
    {
        const uint32_t eventTypes = EPOLLIN;
        size_t eventCount = 0;
        std::unique_ptr<EpollEventSourceTest> pEpollEventSource(new EpollEventSourceTest(eventTypes, eventCount));
        REQUIRE(pEpollEventSource != nullptr);
        REQUIRE(pEpollEventSource->eventCount() == 0);
        REQUIRE(pEpollEventSource->epollEvents() == 0);
        REQUIRE(pEpollEventSource->eventTypes() == eventTypes);

        WHEN("event is set after epoll event source is deleted")
        {
            const auto fileDescriptor = pEpollEventSource->fileDescriptor();
            pEpollEventSource->setKeepFileDescriptor(true);
            pEpollEventSource.reset(nullptr);
            const uint64_t counter = 1;
            REQUIRE(sizeof(counter) == UnixUtils::safeWrite(fileDescriptor, (const char*)&counter, sizeof(counter)));

            THEN("epoll event source is destroyed and onEvent method does not get called")
            {
                QCoreApplication::processEvents();
                REQUIRE(eventCount == 0);
                UnixUtils::safeClose(fileDescriptor);
            }
        }
    }
}


SCENARIO("EpollEventSource can be discarded while events are still pending to be delivered")
{
    GIVEN("two epoll event sources that discard both sources upon receiving an event")
    {
        std::unique_ptr<EpollEventSourceTest> events[2];
        const uint32_t eventTypes = EPOLLIN;
        size_t eventCount = 0;
        for (auto i = 0; i < 2; ++i)
        {
            events[i].reset(new EpollEventSourceTest(eventTypes, eventCount));
            REQUIRE(events[i]->eventCount() == 0);
            REQUIRE(events[i]->epollEvents() == 0);
            REQUIRE(events[i]->eventTypes() == eventTypes);
        }
        events[0]->setEventToBeDeleted(&(events[1]));
        events[1]->setEventToBeDeleted(&events[0]);

        WHEN("both events are set")
        {
            const uint64_t counter = 1;
            REQUIRE(sizeof(counter) == UnixUtils::safeWrite(events[0]->fileDescriptor(), (const char*)&counter, sizeof(counter)));
            REQUIRE(sizeof(counter) == UnixUtils::safeWrite(events[1]->fileDescriptor(), (const char*)&counter, sizeof(counter)));

            THEN("the event source that receives the first onEvent discards the other that does not have its onEvent method called")
            {
                REQUIRE(eventCount == 0);
                QCoreApplication::processEvents();
                REQUIRE(eventCount == 1);
            }
        }
    }
}
