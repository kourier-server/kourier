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
#include <Spectator.h>
#include <set>

using Kourier::TimerList;
using Kourier::TimerPrivate;


SCENARIO("TimerList stores added timers")
{
    GIVEN("three timers")
    {
        TimerPrivate timers[3];
        const std::set<TimerPrivate*> timersToAdd = {&timers[0], &timers[1], &timers[2]};

        WHEN("timers are added to the list")
        {
            TimerList timerList;
            REQUIRE(timerList.size() == 0);
            REQUIRE(timerList.isEmpty());
            size_t counter = 0;
            for (auto &pTimer : timersToAdd)
            {
                timerList.addTimer(pTimer);
                REQUIRE(timerList.size() == ++counter);
                REQUIRE(!timerList.isEmpty());
            }

            THEN("list stores added timers")
            {
                std::set<TimerPrivate*> addedTimers;
                while (!timerList.isEmpty())
                {
                    const auto previousSize = timerList.size();
                    addedTimers.insert(timerList.popFirst());
                    REQUIRE(previousSize == (timerList.size() + 1));
                }
                REQUIRE(addedTimers == timersToAdd);
            }
        }
    }
}


SCENARIO("TimerList allows lists to be swapped")
{
    GIVEN("two timer lists")
    {
        const auto firstTimersCount = GENERATE(AS(size_t), 0, 1, 2, 3);
        TimerPrivate firstTimers[3];
        TimerList firstTimersList;
        std::set<TimerPrivate*> firstTimersToAdd;
        for (auto i = 0; i < firstTimersCount; ++i)
        {
            firstTimersList.addTimer(&firstTimers[i]);
            firstTimersToAdd.insert(&firstTimers[i]);
        }
        const auto secondTimersCount = GENERATE(AS(size_t), 0, 1, 2, 3);
        TimerPrivate secondTimers[3];
        TimerList secondTimersList;
        std::set<TimerPrivate*> secondTimersToAdd;
        for (auto i = 0; i < secondTimersCount; ++i)
        {
            secondTimersList.addTimer(&secondTimers[i]);
            secondTimersToAdd.insert(&secondTimers[i]);
        }

        WHEN("lists are swapped")
        {
            firstTimersList.swap(secondTimersList);
            std::set<TimerPrivate*> addedFirstTimers;
            while (!secondTimersList.isEmpty())
                addedFirstTimers.insert(secondTimersList.popFirst());
            std::set<TimerPrivate*> addedSecondTimers;
            while (!firstTimersList.isEmpty())
                addedSecondTimers.insert(firstTimersList.popFirst());

            THEN("timers are swapped between lists")
            {
                REQUIRE(addedFirstTimers == firstTimersToAdd);
                REQUIRE(addedSecondTimers == secondTimersToAdd);
            }
        }
    }
}
