// Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only OR MPL-2.0-no-copyleft-exception

#include <Spectator.h>
#include "../SectionEntryRecorder.h"
#include <QString>

using namespace Qt::StringLiterals;
using Spectator::Test::SectionEntryRecorder;

SCENARIO("Scenario")
{
    SectionEntryRecorder::global().recordEntry(u"Scenario"_s);

    GIVEN("1")
    {
        SectionEntryRecorder::global().recordEntry(u"1"_s);

        WHEN("1.1")
        {
            SectionEntryRecorder::global().recordEntry(u"1.1"_s);

            THEN("1.1.1")
            {
                SectionEntryRecorder::global().recordEntry(u"1.1.1"_s);
            }

            THEN("1.1.2")
            {
                SectionEntryRecorder::global().recordEntry(u"1.1.2"_s);
            }
        }

        WHEN("1.2")
        {
            SectionEntryRecorder::global().recordEntry(u"1.2"_s);

            THEN("1.2.1")
            {
                SectionEntryRecorder::global().recordEntry(u"1.2.1"_s);
            }

            THEN("1.2.2")
            {
                SectionEntryRecorder::global().recordEntry(u"1.2.2"_s);

                AND_WHEN("1.2.2.1")
                {
                    SectionEntryRecorder::global().recordEntry(u"1.2.2.1"_s);

                    THEN("1.2.2.1.1")
                    {
                        SectionEntryRecorder::global().recordEntry(u"1.2.2.1.1"_s);

                        AND_THEN("1.2.2.1.1.1")
                        {
                            SectionEntryRecorder::global().recordEntry(u"1.2.2.1.1.1"_s);
                        }
                    }
                }
            }
        }
    }

    GIVEN("a")
    {
        SectionEntryRecorder::global().recordEntry(u"a"_s);

        WHEN("a.1")
        {
            SectionEntryRecorder::global().recordEntry(u"a.1"_s);

            THEN("a.2")
            {
                SectionEntryRecorder::global().recordEntry(u"a.2"_s);

                AND_WHEN("a.3")
                {
                    SectionEntryRecorder::global().recordEntry(u"a.3"_s);

                    THEN("a.4")
                    {
                        SectionEntryRecorder::global().recordEntry(u"a.4"_s);

                        AND_WHEN("a.5")
                        {
                            SectionEntryRecorder::global().recordEntry(u"a.5"_s);

                            THEN("a.6")
                            {
                                SectionEntryRecorder::global().recordEntry(u"a.6"_s);

                                AND_THEN("a.7")
                                {
                                    SectionEntryRecorder::global().recordEntry(u"a.7"_s);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
