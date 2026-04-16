// Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only OR MPL-2.0-no-copyleft-exception

#include "Settings.h"
#include <QCommandLineParser>
#include <QThread>
#include <QDebug>
#include <QString>
#include <QTextStream>
#include <cstdio>
#include <cstdlib>

using namespace Qt::StringLiterals;

namespace Spectator
{

Settings Settings::fromCmdLine()
{
    QCommandLineParser parser;
    if (!parser.addOptions({
        {{u"h"_s, u"help"_s}, u"Shows this message."_s},
        {{u"v"_s, u"version"_s}, u"Shows version."_s},
        {u"j"_s, u"Sets the number of threads to use for running the tests."_s, u"<thread count>"_s, u"1"_s},
        {u"r"_s, u"Sets how many times the tests should be repeated."_s, u"<repetition count>"_s, u"0"_s},
        {u"f"_s, u"Sets the source file for filtering scenarios. Only scenarios belonging to the given source file are run."_s, u"<scenario filename>"_s, u""_s},
        {u"s"_s, u"Sets the scenario name for filtering scenarios. Only scenarios matching the given name are run."_s, u"<scenario name>"_s, u""_s},
        {u"t"_s, u"Adds the scenario tag for filtering scenarios. Only scenarios tagged with the given tag are run."_s, u"<scenario tag>"_s, u""_s}}))
        {
            qFatal() << u"Failed to fetch options from commmand line. "
                         "Failed to create QCommandLineParser. "
                         "Invalid command line options given to command line parser."_s;
        }
    if (QCoreApplication::instance() == nullptr) [[unlikely]]
        qFatal() << u"Failed to fetch settings from command line. "
                     "Current QCoreApplication instance is null. "
                     "Please, create a QCoreInstance before trying to run the scenarios."_s;
    else if (!parser.parse(QCoreApplication::arguments())) [[unlikely]]
    {
        qFatal() << u"Failed to fetch settings from command line. "
                     "Failed to parse command line arguments %1."_s
                     .arg(QCoreApplication::arguments().join(' '));
    }
    Settings settings;
    if (parser.isSet("h") || parser.isSet("help"))
        showHelpAndExit();
    if (parser.isSet("v") || parser.isSet("version"))
        showVersionAndExit();
    if (parser.isSet("j"))
    {
        const auto values = parser.values("j");
        if (values.size() != 1) [[unlikely]]
            qFatal("Failed to parse command line options. Thread count (-j option) must be specified only once.");
        else [[likely]]
        {
            bool ok = false;
            auto const threadCount = values[0].toInt(&ok);
            if (!ok || threadCount < -1 || threadCount == 0 || threadCount > QThread::idealThreadCount())
                qFatal("Failed to parse command line options. Invalid argument value. "
                        "The option -j must have as values either a positive integer equal to or "
                        "lesser than the value returned by QThread::idealThreadCount or -1, in "
                        "which case, QThread::idealThreadCount() threads will be used for running the filtered scenarios.");
            settings.m_threadCount = threadCount > 0 ? threadCount : QThread::idealThreadCount();
        }
    }
    if (parser.isSet("r"))
    {
        const auto values = parser.values("r");
        if (values.size() != 1) [[unlikely]]
            qFatal("Failed to parse command line options. Repetition count (-r option) must be specified only once.");
        else [[likely]]
        {
            bool ok = false;
            auto const repetitionCount = values[0].toLongLong(&ok);
            if (!ok || repetitionCount <= 0)
                qFatal("Failed to parse command line options. Invalid argument value. "
                       "The option -r must have as value a positive integer.");
            settings.m_repetitionCount = repetitionCount;
        }
    }
    if (parser.isSet("f"))
    {
        const auto values = parser.values("f");
        if (values.size() != 1) [[unlikely]]
            qFatal("Failed to parse command line options. Only one file path at a time can be used for filtering scenarios (-f can only be specified once).");
        else [[likely]]
            settings.m_filePathFilter = values[0];
    }
    if (parser.isSet("s"))
    {
        const auto values = parser.values("s");
        if (values.size() != 1) [[unlikely]]
            qFatal("Failed to parse command line options. Only one scenario name at a time can be used for filtering scenarios (-s can only be specified once).");
        else [[likely]]
            settings.m_scenarioNameFilter = QString(values[0]).prepend(QStringLiteral(u"Scenario: "));
    }
    if (parser.isSet("t"))
        settings.m_scenarioTagsFilter = parser.values("t");
    return settings;
}

void Settings::showHelpAndExit()
{
    static const QString helpText = u"(Usage: test-app [INFORMATION OPTION]\n"
"or:  test-app [TEST OPTIONS]\n\n"
"Options are of two types: information or test. "
"Information options must not be combined with other information or test options. Test options can be combined freely.\n\n"
"Information options:\n"
"  -h, --help               Show this message and exit.\n"
"  -v, --version            Show version and exit.\n\n"
"Test options:\n"
"  -j <thread count>        Use <thread count> threads to run tests.\n"
"  -u <N>                   Repeat tests <N> more times.\n"
"  -f <file>                Filter scenarios by <file>. Only scenarios belonging to <file> are run.\n"
"  -s <name>                Filter scenarios by <name>. Only the scenario with <name> is run.\n"
"  -t <tag>                 Filter scenarios by <tag>. Only scenarios tagged with <tag> are run.\n"_s;
    QTextStream textStream(stdout);
    textStream << helpText;
    textStream.flush();
    std::exit(0);
}

#define XSTR(x) STR(x)
#define STR(x) #x

void Settings::showVersionAndExit()
{
    QTextStream textStream(stdout);
    textStream << XSTR(CONFIGURATION_VERSION_STRING) << Qt::endl;
    textStream.flush();
    std::exit(0);
}

}
