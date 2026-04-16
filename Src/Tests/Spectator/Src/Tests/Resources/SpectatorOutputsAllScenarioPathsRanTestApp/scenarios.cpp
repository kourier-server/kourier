// Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only OR MPL-2.0-no-copyleft-exception

#include <Spectator.h>

SCENARIO("A Scenario without require")
{
    GIVEN("given")
    {   
        WHEN("when")
        {
            THEN("then")
            {
                AND_WHEN("and when")
                {
                    THEN("then in and when")
                    {
                    }
                }

                AND_THEN("and then")
                {
                }
            }
        }

        WHEN("when 2")
        {
            THEN("then 2")
            {
            }
        }
    }
}

SCENARIO("A Scenario with a single require")
{
    GIVEN("given")
    {   
        WHEN("when")
        {
            THEN("then")
            {
                REQUIRE(true);
            }
        }
    }
}

SCENARIO("A Scenario with two requires")
{
    GIVEN("given")
    {
        REQUIRE(true);
        
        WHEN("when")
        {
            THEN("then")
            {
                REQUIRE(true);
            }
        }
    }
}

SCENARIO("A Scenario with one tag", TAG("a tag"))
{
    GIVEN("given")
    {
        WHEN("when")
        {
            REQUIRE(true);

            THEN("then")
            {
                REQUIRE(true);
            }
        }
    }
}

SCENARIO("A Scenario with multiple tags", TAG("tag 1"), TAG("tag 2"), TAG("tag 3"))
{
    REQUIRE(true);

    GIVEN("given")
    {
        REQUIRE(true);

        WHEN("when")
        {
            REQUIRE(true);

            THEN("then")
            {
                REQUIRE(true);
            }
        }
    }
}
