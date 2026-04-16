// Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only OR MPL-2.0-no-copyleft-exception

#include <Spectator.h>
#include <Generator.h>
#include "../SectionEntryRecorder.h"
#include "../GeneratorDataRecorder.h"
#include <QString>
#include <QByteArray>
#include <cstdint>

using namespace Qt::StringLiterals;
using Spectator::Test::GeneratorData;
using Spectator::Test::GeneratorDataRecorder;
using Spectator::Test::SectionEntryRecorder;

SCENARIO("Scenario")
{
    SectionEntryRecorder::global().recordEntry(u"Scenario"_s);
    const auto scenarioData = GENERATE(AS(QString), u"Hello"_s, u"World!"_s);

    GIVEN("1")
    {
        SectionEntryRecorder::global().recordEntry(u"1"_s);
        const auto given1Data = GENERATE_RANGE(AS(int), 1, 3);

        WHEN("1.1")
        {
            SectionEntryRecorder::global().recordEntry(u"1.1"_s);
            const auto when1_1Data = GENERATE_RANGE_WITH_STEP(AS(int64_t), 1, 7, 2);

            THEN("1.1.1")
            {
                SectionEntryRecorder::global().recordEntry(u"1.1.1"_s);
                const auto then1_1_1Data = GENERATE(AS(QByteArray), "First Data", "Second Data");
                GeneratorDataRecorder::global().recordData(GeneratorData{.string=scenarioData, .intValue = given1Data, .int64Value = when1_1Data, .byteArray = then1_1_1Data});
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

    const auto scenarioEndData = GENERATE(AS(QString), u"a"_s, u"b"_s);
}
