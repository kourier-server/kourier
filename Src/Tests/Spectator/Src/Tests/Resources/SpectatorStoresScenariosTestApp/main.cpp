// Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only OR MPL-2.0-no-copyleft-exception

#include <ScenarioRepository.h>
#include <Spectator.h>
#include <QCoreApplication>
#include <QTextStream>
#include <QDataStream>
#include <QByteArray>
#include <QString>
#include <cstdio>

using namespace Spectator;

int main(int argc, char ** argv)
{
    QCoreApplication app(argc, argv);
    QByteArray buffer;
    buffer.reserve(1024);
    QDataStream dataStream(&buffer, QIODeviceBase::WriteOnly);
    auto & scenarioRepository = ScenarioRepository::global();
    dataStream << scenarioRepository.size();
    for (auto i = 0; i < scenarioRepository.size(); ++i)
    {
        const auto & scenario = scenarioRepository[i];
        dataStream << QString(scenario.sourceFile());
        dataStream << scenario.sourceLine();
        dataStream << QString(scenario.name());
        QStringList tags;
        for (auto i = 0; i < scenario.tagCount(); ++i)
            tags.append(QString(scenario.tagAt(i)));
        dataStream << tags;
    }
    QTextStream(stdout) << buffer.toBase64();
    return 0;
}
