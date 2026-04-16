// Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only OR MPL-2.0-no-copyleft-exception

#include <Spectator.h>

SCENARIO("A Scenario")
{
}

SCENARIO("Another Scenario")
{
}

SCENARIO("Yet Another Scenario")
{
}

SCENARIO("A Scenario with one tag", TAG("a tag"))
{
}

SCENARIO("A Scenario with multiple tags", TAG("tag 1"), TAG("tag 2"), TAG("tag 3"))
{
}
