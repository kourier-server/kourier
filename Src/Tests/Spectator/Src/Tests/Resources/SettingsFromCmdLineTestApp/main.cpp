// Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only OR MPL-2.0-no-copyleft-exception

#include <Settings.h>
#include <QCoreApplication>
#include <QTextStream>
#include <QDataStream>
#include <QByteArray>
#include <cstdio>

using namespace Spectator;

int main(int argc, char ** argv)
{
    QCoreApplication app(argc, argv);
    const auto settings = Settings::fromCmdLine();
    QByteArray buffer;
    buffer.reserve(1024);
    QDataStream dataStream(&buffer, QIODeviceBase::WriteOnly);
    dataStream << settings.threadCount();
    dataStream << settings.repetitionCount();
    dataStream << settings.filePathFilter();
    dataStream << settings.scenarioNameFilter();
    dataStream << settings.scenarioTagsFilter();
    QTextStream(stdout) << buffer.toBase64();
    return 0;
}
