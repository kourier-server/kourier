// Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include <Spectator>
#include <QString>
#include <map>

using namespace Qt::StringLiterals;

SCENARIO("Scenario with generator")
{
    GIVEN("given executed once")
    {
        WHEN("when executed once")
        {
            THEN("then executed once")
            {
            }
        }
    }

    // From now on, everything on the scenario paths will be run 2 times.
    const auto scenarioData = GENERATE(AS(QString), u"Hello"_s, u"World!"_s);

    GIVEN("a given with generate range")
    {
        // From now on, everything on the scenario paths will be run 2*3 times (givenData iterates on {1,2,3})
        const auto givenData = GENERATE_RANGE(AS(int), 1, 3);
        REQUIRE(givenData == 1 || givenData == 2 || givenData == 3);

        WHEN("when with generate range with step")
        {
            // From now on, everything on the scenario paths will be run 2*3*4 times (whenData iterates on {1,3,5,7})
            const auto whenData = GENERATE_RANGE_WITH_STEP(AS(int64_t), 1, 7, 2);
            REQUIRE(whenData == 1 || whenData == 3 || whenData == 5 || whenData == 7);

            THEN("then with generate on maps")
            {
                // From now on, everything on the scenario path will be run 2*3*4*3 times.
                const auto map = GENERATE(AS(std::map<int, QByteArray>), {{1,"one"}},
                                                                         {{1,"one"}, {2,"two"}},
                                                                         {{1,"one"}, {2,"two"}, {3,"three"}});
            }
        }
    }
    // Let's generate some data at the scenario end just to test if Spectator
    // correctly repeats the whole scenario even without entering any section.
    const auto scenarioEndData = GENERATE(AS(QString), u"a"_s, u"b"_s);
}
