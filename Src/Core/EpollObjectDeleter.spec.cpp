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

#include "EpollObjectDeleter.h"
#include "Object.h"
#include <QCoreApplication>
#include <Spectator.h>


using Kourier::EpollObjectDeleter;
using Kourier::Object;
using Kourier::EpollEventNotifier;


class EpollObjectDeleterTest : public Object
{
KOURIER_OBJECT(EpollObjectDeleterTest);
public:
    EpollObjectDeleterTest() = default;
    ~EpollObjectDeleterTest() override {++m_deletedObjects; deletedObject();}
    static int deletedObjects() {return m_deletedObjects;}
    static void resetDeletedObjectsCount() {m_deletedObjects = 0;}
    Kourier::Signal deletedObject();

private:
    static int m_deletedObjects;
};

Kourier::Signal EpollObjectDeleterTest::deletedObject() KOURIER_SIGNAL(&EpollObjectDeleterTest::deletedObject)
int EpollObjectDeleterTest::m_deletedObjects = 0;


SCENARIO("EpollObjectDeleter deletes added objects")
{
    GIVEN("an epoll object deleter")
    {
        EpollObjectDeleter epollObjectDeleter;

        WHEN("objects are scheduled for deletion")
        {
            const auto objectsToDelete = GENERATE(AS(size_t), 0, 1, 3, 8);
            EpollObjectDeleterTest::resetDeletedObjectsCount();
            REQUIRE(EpollObjectDeleterTest::deletedObjects() == 0);
            for (auto i = 0; i < objectsToDelete; ++i)
                epollObjectDeleter.scheduleForDeletion(new EpollObjectDeleterTest);

            THEN("objects are deleted when control returns to the event loop")
            {
                REQUIRE(EpollObjectDeleterTest::deletedObjects() == 0);
                QCoreApplication::processEvents();
                REQUIRE(EpollObjectDeleterTest::deletedObjects() == objectsToDelete);

                AND_WHEN("control returns one more time to the event loop")
                {
                    QCoreApplication::processEvents();

                    THEN("deleted object count remains as before")
                    {
                        REQUIRE(EpollObjectDeleterTest::deletedObjects() == objectsToDelete);
                    }
                }
            }
        }
    }
}


SCENARIO("EpollObjectDeleter supports multiple additions of the same object")
{
    GIVEN("an epoll object deleter")
    {
        EpollObjectDeleter epollObjectDeleter;

        WHEN("an object is scheduled for deletion many times")
        {
            const auto repCount = GENERATE(AS(size_t), 1, 3, 8);
            EpollObjectDeleterTest::resetDeletedObjectsCount();
            REQUIRE(EpollObjectDeleterTest::deletedObjects() == 0);
            auto * const pObject = new EpollObjectDeleterTest;
            for (auto i = 0; i < repCount; ++i)
                epollObjectDeleter.scheduleForDeletion(pObject);

            THEN("object is deleted when control returns to the event loop")
            {
                REQUIRE(EpollObjectDeleterTest::deletedObjects() == 0);
                QCoreApplication::processEvents();
                REQUIRE(EpollObjectDeleterTest::deletedObjects() == 1);

                AND_WHEN("control returns one more time to the event loop")
                {
                    QCoreApplication::processEvents();

                    THEN("deleted object count remains as before")
                    {
                        REQUIRE(EpollObjectDeleterTest::deletedObjects() == 1);
                    }
                }
            }
        }
    }
}


SCENARIO("EpollObjectDeleter supports objects to be scheduled by objects being destroyed")
{
    GIVEN("an epoll object deleter")
    {
        EpollObjectDeleter epollObjectDeleter;

        WHEN("one of the added objects schedules other objects for deletion")
        {
            const auto objectCount = GENERATE(AS(size_t), 0, 1, 3, 8);
            EpollObjectDeleterTest::resetDeletedObjectsCount();
            REQUIRE(EpollObjectDeleterTest::deletedObjects() == 0);
            auto *pEpollEventSourceTest = new EpollObjectDeleterTest;
            Object::connect(pEpollEventSourceTest, &EpollObjectDeleterTest::deletedObject, [&epollObjectDeleter, objectCount]()
            {
                for (auto i = 0; i < objectCount; ++i)
                    epollObjectDeleter.scheduleForDeletion(new EpollObjectDeleterTest);
            });
            epollObjectDeleter.scheduleForDeletion(pEpollEventSourceTest);

            THEN("all objects are deleted when control returns to the event loop")
            {
                REQUIRE(EpollObjectDeleterTest::deletedObjects() == 0);
                QCoreApplication::processEvents();
                REQUIRE(EpollObjectDeleterTest::deletedObjects() == (objectCount + 1));

                AND_WHEN("control returns one more time to the event loop")
                {
                    QCoreApplication::processEvents();

                    THEN("deleted object count remains as before")
                    {
                        REQUIRE(EpollObjectDeleterTest::deletedObjects() == (objectCount + 1));
                    }
                }
            }
        }
    }
}


SCENARIO("EpollEventNotifier deletes added objects")
{
    GIVEN("a number of objects to delete")
    {
        const auto objectsToDelete = GENERATE(AS(size_t), 0, 1, 3, 8);

        WHEN("objects are scheduled for deletion through current epoll event notifier")
        {
            EpollObjectDeleterTest::resetDeletedObjectsCount();
            REQUIRE(EpollObjectDeleterTest::deletedObjects() == 0);
            for (auto i = 0; i < objectsToDelete; ++i)
                (new EpollObjectDeleterTest)->scheduleForDeletion();

            THEN("objects are deleted when control returns to the event loop")
            {
                REQUIRE(EpollObjectDeleterTest::deletedObjects() == 0);
                QCoreApplication::processEvents();
                REQUIRE(EpollObjectDeleterTest::deletedObjects() == objectsToDelete);

                AND_WHEN("control returns one more time to the event loop")
                {
                    QCoreApplication::processEvents();

                    THEN("deleted object count remains as before")
                    {
                        REQUIRE(EpollObjectDeleterTest::deletedObjects() == objectsToDelete);
                    }
                }
            }
        }
    }
}
