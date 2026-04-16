// Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only OR MPL-2.0-no-copyleft-exception

#include <ScenarioRepository.h>
#include "../SectionEntryRecorder.h"
#include "../GeneratorDataRecorder.h"
#include <ScenarioRunner.h>
#include <Spectator.h>
#include <QCoreApplication>
#include <QtLogging>
#include <QByteArray>
#include <QDataStream>
#include <QTextStream>
#include <cstdio>

using namespace Spectator;
using namespace Spectator::Test;

int main(int argc, char ** argv)
{
    QCoreApplication app(argc, argv);
    auto & scenarioRepository = ScenarioRepository::global();
    if (scenarioRepository.size() != 1)
        qFatal("Failed to store scenario.");
    auto recordedEntries = SectionEntryRecorder::global().recordedEntries();
    if (!recordedEntries.isEmpty())
        qFatal("Recorded entries is expected to be empty.");
    ScenarioRunner::runScenario(scenarioRepository.global()[0]);
    recordedEntries = SectionEntryRecorder::global().recordedEntries();
    if (recordedEntries.isEmpty())
        qFatal("Recorded entries is expected to be non-empty.");
    QByteArray buffer;
    buffer.reserve(1024);
    QDataStream dataStream(&buffer, QIODeviceBase::WriteOnly);
    dataStream << SectionEntryRecorder::global().recordedEntries();
    dataStream << GeneratorDataRecorder::global().recordedData();
    QTextStream(stdout) << buffer.toBase64();
    return 0;
}
