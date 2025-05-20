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

#include "EpollReadyEventSourceRegistrar.h"
#include <QCoreApplication>
#include <Spectator.h>
#include <utility>
#include <set>
#include <vector>


using Kourier::EpollReadyEventSourceRegistrar;
using Kourier::EpollEventSource;
using Kourier::EpollEventNotifier;
using Kourier::Object;

namespace Test::EpollReadyEventSourceRegistrar
{

class EpollReadyEventSourceRegistrarTest : public EpollEventSource
{
KOURIER_OBJECT(Test::EpollReadyEventSourceRegistrar::EpollReadyEventSourceRegistrarTest)
public:
    EpollReadyEventSourceRegistrarTest(uint32_t events) : EpollEventSource(events) {}
    ~EpollReadyEventSourceRegistrarTest() override = default;
    int64_t fileDescriptor() const override {FAIL("This code is supposed to be unreachable.");}
    Kourier::Signal triggeredEvent(EpollReadyEventSourceRegistrarTest *pEvent, uint32_t epollEvents);

private:
    void onEvent(uint32_t epollEvents) override
    {
        triggeredEvent(this, epollEvents);
    }
};

Kourier::Signal EpollReadyEventSourceRegistrarTest::triggeredEvent(EpollReadyEventSourceRegistrarTest *pEvent, uint32_t epollEvents) KOURIER_SIGNAL(&EpollReadyEventSourceRegistrarTest::triggeredEvent, pEvent, epollEvents)

}

using namespace Test::EpollReadyEventSourceRegistrar;


SCENARIO("EpollReadyEventSourceRegistrar calls onEvent on added epoll event sources when control returns to event loop")
{
    GIVEN("a EpollReadyEventSourceRegistrar instance")
    {
        const auto useEventNotifier = GENERATE(AS(bool), true, false);
        EpollReadyEventSourceRegistrar instance;

        WHEN("events are added to instance/posted to event notifier")
        {
            const auto eventSourceCount = GENERATE(AS(size_t), 0, 1, 3, 8);
            std::set<std::pair<EpollReadyEventSourceRegistrarTest*, uint32_t>> results;
            std::set<std::pair<EpollReadyEventSourceRegistrarTest*, uint32_t>> expectedResults;
            std::vector<std::shared_ptr<EpollReadyEventSourceRegistrarTest>> createdTestEvents;
            for (auto i = 0; i < eventSourceCount; ++i)
            {
                createdTestEvents.push_back(std::make_shared<EpollReadyEventSourceRegistrarTest>(i));
                Object::connect(createdTestEvents.back().get(),
                                &EpollReadyEventSourceRegistrarTest::triggeredEvent,
                                [&results](EpollReadyEventSourceRegistrarTest *pEvent, uint32_t events)
                                {
                                    results.insert({pEvent, events});
                                });
                expectedResults.insert({createdTestEvents.back().get(), i});
                if (useEventNotifier)
                    createdTestEvents.back().get()->eventNotifier()->postEvent(createdTestEvents.back().get(), i);
                else
                    instance.addReadyEvent(createdTestEvents.back().get(), i);
            }

            THEN("events are triggered when control returns to the event loop")
            {
                REQUIRE(results.empty());
                QCoreApplication::processEvents();
                REQUIRE(results == expectedResults);

                AND_WHEN("control returns one more time to the event loop")
                {
                    QCoreApplication::processEvents();

                    THEN("triggered event count remains as before")
                    {
                        REQUIRE(results == expectedResults);
                    }
                }
            }
        }
    }
}


SCENARIO("EpollReadyEventSourceRegistrar supports event removal while triggering events")
{
    GIVEN("an EpollReadyEventSourceRegistrar instance with some ready events added to it/posted to event notifier")
    {
        EpollReadyEventSourceRegistrar instance;
        const auto useEventNotifier = GENERATE(AS(bool), true, false);
        const size_t eventSourceCount = 4;
        const auto eventsToRemove = GENERATE(AS(size_t), 1, 2, 3);
        std::set<std::pair<EpollReadyEventSourceRegistrarTest*, uint32_t>> results;
        std::set<std::pair<EpollReadyEventSourceRegistrarTest*, uint32_t>> expectedResults;
        std::vector<std::shared_ptr<EpollReadyEventSourceRegistrarTest>> createdTestEvents;
        for (auto i = 0; i < eventSourceCount; ++i)
        {
            createdTestEvents.push_back(std::make_shared<EpollReadyEventSourceRegistrarTest>(i));
            if (useEventNotifier)
                createdTestEvents.back().get()->eventNotifier()->postEvent(createdTestEvents.back().get(), i);
            else
                instance.addReadyEvent(createdTestEvents.back().get(), i);
        }

        WHEN("first ready event processed removes other events")
        {
            size_t triggeredEventsCount = 0;
            for (auto &pEvent : createdTestEvents)
            {
                Object::connect(pEvent.get(),
                                &EpollReadyEventSourceRegistrarTest::triggeredEvent,
                                [&](EpollReadyEventSourceRegistrarTest *pEvent, uint32_t events)
                                {
                                    if (++triggeredEventsCount == 1)
                                    {
                                        expectedResults.insert({pEvent, events});
                                        size_t counter = 0;
                                        for (auto &event : createdTestEvents)
                                        {
                                            if (event.get() == pEvent)
                                                continue;
                                            else if (++counter <= eventsToRemove)
                                            {
                                                if (useEventNotifier)
                                                    event.get()->eventNotifier()->removePostedEvents(event.get());
                                                else
                                                    instance.removeReadyEvent(event.get());
                                            }
                                            else
                                                expectedResults.insert({event.get(), event->eventTypes()});
                                        }
                                    }
                                    results.insert({pEvent, events});
                                });
            }

            THEN("removed events are not triggered")
            {
                REQUIRE(triggeredEventsCount == 0);
                QCoreApplication::processEvents();
                REQUIRE(triggeredEventsCount == (createdTestEvents.size() - eventsToRemove));
                REQUIRE(results == expectedResults);

                AND_WHEN("control returns one more time to the event loop")
                {
                    QCoreApplication::processEvents();

                    THEN("triggered event count remains as before")
                    {
                        REQUIRE(triggeredEventsCount == (createdTestEvents.size() - eventsToRemove));
                        REQUIRE(results == expectedResults);
                    }
                }
            }
        }
    }
}


SCENARIO("EpollReadyEventSourceRegistrar supports event addition while triggering events")
{
    GIVEN("an EpollReadyEventSourceRegistrar instance with some ready events added to it/posted to event notifier")
    {
        EpollReadyEventSourceRegistrar instance;
        const auto useEventNotifier = GENERATE(AS(bool), true, false);
        const size_t eventSourceCount = 4;
        const auto eventsToAdd = GENERATE(AS(size_t), 1, 2, 3);
        std::set<std::pair<EpollReadyEventSourceRegistrarTest*, uint32_t>> results;
        std::set<std::pair<EpollReadyEventSourceRegistrarTest*, uint32_t>> expectedResults;
        std::vector<std::shared_ptr<EpollReadyEventSourceRegistrarTest>> createdTestEvents;
        for (auto i = 0; i < eventSourceCount; ++i)
        {
            createdTestEvents.push_back(std::make_shared<EpollReadyEventSourceRegistrarTest>(i));
            expectedResults.insert({createdTestEvents.back().get(), i});
            if (useEventNotifier)
                createdTestEvents.back().get()->eventNotifier()->postEvent(createdTestEvents.back().get(), i);
            else
                instance.addReadyEvent(createdTestEvents.back().get(), i);
        }

        WHEN("first ready event processed adds other events")
        {
            size_t triggeredEventsCount = 0;
            for (auto &pEvent : createdTestEvents)
            {
                Object::connect(pEvent.get(),
                                &EpollReadyEventSourceRegistrarTest::triggeredEvent,
                                [&](EpollReadyEventSourceRegistrarTest *pEvent, uint32_t events)
                                {
                                    if (++triggeredEventsCount == 1)
                                    {
                                        for (auto i = 0; i < eventsToAdd; ++i)
                                        {
                                            const uint32_t events = i + 125;
                                            createdTestEvents.push_back(std::make_shared<EpollReadyEventSourceRegistrarTest>(events));
                                            expectedResults.insert({createdTestEvents.back().get(), events});
                                            if (useEventNotifier)
                                                createdTestEvents.back().get()->eventNotifier()->postEvent(createdTestEvents.back().get(), events);
                                            else
                                                instance.addReadyEvent(createdTestEvents.back().get(), events);
                                            Object::connect(createdTestEvents.back().get(),
                                                            &EpollReadyEventSourceRegistrarTest::triggeredEvent,
                                                            [&](EpollReadyEventSourceRegistrarTest *pEvent, uint32_t events)
                                                            {
                                                                ++triggeredEventsCount;
                                                                results.insert({pEvent, events});
                                                            });
                                        }
                                    }
                                    results.insert({pEvent, events});
                                });
            }

            THEN("events added initially are triggered")
            {
                REQUIRE(triggeredEventsCount == 0);
                QCoreApplication::processEvents();
                REQUIRE(triggeredEventsCount == eventSourceCount);

                AND_WHEN("control returns to event loop again")
                {
                    QCoreApplication::processEvents();

                    THEN("added events are triggered")
                    {
                        REQUIRE(triggeredEventsCount == (eventSourceCount + eventsToAdd));
                        REQUIRE(results == expectedResults);

                        AND_WHEN("control returns one more time to the event loop")
                        {
                            QCoreApplication::processEvents();

                            THEN("triggered event count remains as before")
                            {
                                REQUIRE(triggeredEventsCount == (eventSourceCount + eventsToAdd));
                                REQUIRE(results == expectedResults);
                            }
                        }
                    }
                }
            }
        }
    }
}
