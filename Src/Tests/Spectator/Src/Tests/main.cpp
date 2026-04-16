// Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only OR MPL-2.0-no-copyleft-exception

#include "Resources/GeneratorDataRecorder.h"
#include <QCoreApplication>
#include <QtLogging>
#include <QThread>
#include <QList>
#include <QString>
#include <QProcess>
#include <QProcessEnvironment>
#include <QDir>
#include <QtLogging>
#include <QDebug>
#include <QDataStream>
#include <QByteArray>
#include <QStringList>
#include <QByteArrayList>
#include <QSet>
#include <QTextStream>
#include <QFileInfo>
#include <Qt>
#include <QMap>
#include <QtTypes>
#include <QRegularExpression>
#include <cstdio>
#include <utility>

using namespace Qt::StringLiterals;
using Spectator::Test::GeneratorData;

static QTextStream & qStdOut()
{
    static QTextStream textStream(stdout);
    return textStream;
}

static void spectatorFetchesSettingsFromCmdLine();
static void spectatorStoresScenarios();
static void spectatorSupportsInfoMessagesOutsideScenarioScope();
static void spectatorSupportsRequireOutsideScenarioScope();
static void spectatorVisitsAllLeafNodesOfScenarioPathWithoutGeneratorsOnce();
static void spectatorKeepsVisitingLeafNodeOfScenarioPathUntilExhaustingDataOfAllGeneratorsOnPath();
static void spectatorOutputsAllScenarioPathsRan();
static void spectatorRunsAllScenariosOnSingleThreadByDefault();
static void spectatorSupportsRunningScenariosOnMultipleThreads();
static void spectatorSupportsScenarioFiltering();
static void spectatorSupportsTestRepetition();

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    qStdOut() << u"Running Tests"_s << Qt::endl;
    spectatorFetchesSettingsFromCmdLine();
    spectatorStoresScenarios();
    spectatorSupportsInfoMessagesOutsideScenarioScope();
    spectatorSupportsRequireOutsideScenarioScope();
    spectatorVisitsAllLeafNodesOfScenarioPathWithoutGeneratorsOnce();
    spectatorKeepsVisitingLeafNodeOfScenarioPathUntilExhaustingDataOfAllGeneratorsOnPath();
    spectatorOutputsAllScenarioPathsRan();
    spectatorRunsAllScenariosOnSingleThreadByDefault();
    spectatorSupportsRunningScenariosOnMultipleThreads();
    spectatorSupportsScenarioFiltering();
    spectatorSupportsTestRepetition();
    return 0;
}

static void spectatorFetchesSettingsFromCmdLine()
{
    qStdOut() << u"Testing Settings from command line."_s << Qt::endl;
    const auto threadCounts = QStringList() << u""_s
                                            << u"-1"_s
                                            << u"1"_s
                                            << QString::number(QThread::idealThreadCount());
    const auto repetitionCounts = QStringList() << u""_s
                                                << u"1"_s
                                                << u"1024"_s;
    const auto filePathFilters = QStringList() << u""_s
                                               << u"file://somefile"_s
                                               << u"file://another_file/at_sub/directory"_s;
    const auto scenarioNameFilters = QStringList() << u""_s
                                                   << u"TcpSockets exchange data"_s
                                                   << u"RingBuffer wraps data inside buffer"_s;
    const auto tagsList = QList<QStringList>() << QStringList()
                                               << (QStringList() << u"Design Tests"_s  << u"Another Tag"_s)
                                               << (QStringList() << u"TLS Encryption"_s);
    for (const auto & threadCount : threadCounts)
    {
        for (const auto & repetitionCount : repetitionCounts)
        {
            for (const auto & filePathFilter : filePathFilters)
            {
                for (const auto & scenarioNameFilter : scenarioNameFilters)
                {
                    for (const auto & tags : tagsList)
                    {
                        // Create command line arguments
                        QStringList cmdLineArgs;
                        if (!threadCount.isEmpty())
                            cmdLineArgs << u"-j"_s << threadCount;
                        if (!repetitionCount.isEmpty())
                            cmdLineArgs << u"-r"_s << repetitionCount;
                        if (!filePathFilter.isEmpty())
                            cmdLineArgs << u"-f"_s << filePathFilter;
                        if (!scenarioNameFilter.isEmpty())
                            cmdLineArgs << u"-s"_s << scenarioNameFilter;
                        if (!tags.isEmpty())
                        {
                            for (const auto & tag : tags)
                                cmdLineArgs << u"-t"_s << tag;
                        }
                        // Run test app
                        QProcess testApp;
                        auto testsAppDir = QDir(QCoreApplication::applicationDirPath());
                        auto resourceTestAppFilePath = testsAppDir.absoluteFilePath("Resources/SettingsFromCmdLineTestApp/SettingsFromCmdLineTestApp");
                        testApp.start(resourceTestAppFilePath, cmdLineArgs);
                        if (!testApp.waitForFinished(5000))
                            qFatal("%s%s%s", "Failed to wait for ", qUtf8Printable(resourceTestAppFilePath), " test app to finish.");
                        const auto output = testApp.readAllStandardOutput();
                        // Check result
                        const auto buffer = QByteArray::fromBase64(output);
                        QDataStream dataStream(buffer);
                        qint32 parsedThreadCount = 0;
                        dataStream >> parsedThreadCount;
                        const qint32 expectedThreadCount = threadCount.isEmpty() ? 1 : (threadCount.toInt() == -1 ? QThread::idealThreadCount() : threadCount.toInt());
                        if (parsedThreadCount != expectedThreadCount)
                            qFatal() << QString(u"Thread count does not match with expected value (parsed value = %1; expected value = %2)."_s).arg(parsedThreadCount).arg(expectedThreadCount);
                        qint64 parsedRepetitionCount = 0;
                        dataStream >> parsedRepetitionCount;
                        const qint64 expectedRepetitionCount = repetitionCount.isEmpty() ? 0 : repetitionCount.toLongLong();
                        if (parsedRepetitionCount != expectedRepetitionCount)
                            qFatal() << QString(u"Repetition count does not match with expected value (parsed value = %1; expected value = %2)."_s).arg(parsedRepetitionCount).arg(expectedRepetitionCount);
                        QString parsedFilePathFilter;
                        dataStream >> parsedFilePathFilter;
                        if (parsedFilePathFilter != filePathFilter)
                            qFatal() << QString(u"File path filter does not match with expected value (parsed value = %1; expected value = %2)."_s).arg(parsedFilePathFilter).arg(filePathFilter);
                        QString parsedScenarioNameFilter;
                        dataStream >> parsedScenarioNameFilter;
                        const auto expectedScenarioNameFilter = scenarioNameFilter.isEmpty() ? u""_s : (u"Scenario: "_s + scenarioNameFilter);
                        if (parsedScenarioNameFilter != expectedScenarioNameFilter)
                            qFatal() << QString(u"Scenario name filter does not match with expected value (parsed value = %1; expected value = %2)."_s).arg(parsedScenarioNameFilter).arg(expectedScenarioNameFilter);
                        QStringList parsedTags;
                        dataStream >> parsedTags;
                        QSet<QString> orderedParsedTags;
                        for (const auto &tag : parsedTags)
                            orderedParsedTags.insert(tag);
                        QSet<QString> orderedExpectedTags;
                        for (const auto &tag : tags)
                            orderedExpectedTags.insert(tag);
                        if (orderedParsedTags != orderedExpectedTags)
                            qFatal("Parsed tags do not match expected tags.");
                    }
                }
            }
        }
    }
    qStdOut() << u"PASSED Settings from command line tests."_s << Qt::endl;
}

static void spectatorStoresScenarios()
{
    qStdOut() << u"Testing Scenario Storing."_s << Qt::endl;
    // Run test app
    QProcess testApp;
    auto testsAppDir = QDir(QCoreApplication::applicationDirPath());
    auto resourceTestAppFilePath = testsAppDir.absoluteFilePath("Resources/SpectatorStoresScenariosTestApp/SpectatorStoresScenariosTestApp");
    testApp.start(resourceTestAppFilePath);
    if (!testApp.waitForFinished(5000))
        qFatal("%s%s%s", "Failed to wait for ", qUtf8Printable(resourceTestAppFilePath), " test app to finish.");
    const auto output = testApp.readAllStandardOutput();
    // Check result
    const auto buffer = QByteArray::fromBase64(output);
    QDataStream dataStream(buffer);
    qsizetype scenarioCount = 0;
    dataStream >> scenarioCount;
    if (scenarioCount != 7)
        qFatal("Spectator did not store the correct number of scenarios.");
    struct ScenarioData
    {
        QString sourceFile;
        qint32 sourceLine;
        QString scenarioName;
        QStringList scenarioTags;
        inline bool operator==(const ScenarioData & other) const
        {
            return sourceFile == other.sourceFile
                   && sourceLine == other.sourceLine
                   && scenarioName == other.scenarioName
                   && scenarioTags == other.scenarioTags;
        }
    };
    QList<ScenarioData> fetchedScenarios;
    for (auto i = 0; i < scenarioCount; ++i)
    {
        fetchedScenarios.append(ScenarioData());
        dataStream >> fetchedScenarios.back().sourceFile;
        dataStream >> fetchedScenarios.back().sourceLine;
        dataStream >> fetchedScenarios.back().scenarioName;
        dataStream >> fetchedScenarios.back().scenarioTags;
    }
    const QFileInfo thisFileInfo(__FILE__);
    QDir scenariosDir(thisFileInfo.canonicalPath());
    if (!scenariosDir.cd(u"Resources"_s) || !scenariosDir.cd(u"SpectatorStoresScenariosTestApp"_s))
        qFatal("Failed to navigate to directory containing scenario source files.");
    const auto expectedScenariosData = QList<ScenarioData>()
        << ScenarioData{.sourceFile=scenariosDir.absoluteFilePath(u"scenarios_1.cpp"_s), .sourceLine=6, .scenarioName=u"Scenario: A Scenario"_s}
        << ScenarioData{.sourceFile=scenariosDir.absoluteFilePath(u"scenarios_1.cpp"_s), .sourceLine=10, .scenarioName=u"Scenario: Another Scenario"_s}
        << ScenarioData{.sourceFile=scenariosDir.absoluteFilePath(u"scenarios_1.cpp"_s), .sourceLine=14, .scenarioName=u"Scenario: Yet Another Scenario"_s}
        << ScenarioData{.sourceFile=scenariosDir.absoluteFilePath(u"scenarios_1.cpp"_s), .sourceLine=18, .scenarioName=u"Scenario: A Scenario with one tag"_s, .scenarioTags=(QStringList() << u"a tag"_s)}
        << ScenarioData{.sourceFile=scenariosDir.absoluteFilePath(u"scenarios_1.cpp"_s), .sourceLine=22, .scenarioName=u"Scenario: A Scenario with multiple tags"_s, .scenarioTags=(QStringList() << u"tag 1"_s << u"tag 2"_s << u"tag 3"_s)}
        << ScenarioData{.sourceFile=scenariosDir.absoluteFilePath(u"scenarios_2.cpp"_s), .sourceLine=7, .scenarioName=u"Scenario: Spectator is a really fast test framework"_s}
        << ScenarioData{.sourceFile=scenariosDir.absoluteFilePath(u"scenarios_3.cpp"_s), .sourceLine=8, .scenarioName=u"Scenario: Kourier is a blazingly fast HTTP server"_s};
    for (const auto & expectedScenarioData : expectedScenariosData)
    {
        if (!fetchedScenarios.contains(expectedScenarioData))
            qFatal("Spectator failed to store scenario data.");
    }
    qStdOut() << u"PASSED Testing Scenario Storing."_s << Qt::endl;
}

static void spectatorSupportsInfoMessagesOutsideScenarioScope()
{
    qStdOut() << u"Testing INFO messages outside scenario scope."_s << Qt::endl;
    // Run test app
    const auto options = QStringList() << u"IN_CONSTRUCTOR"_s << u"IN_DESTRUCTOR"_s;
    for (const auto & option : options)
    {
        QProcess testApp;
        QProcessEnvironment processEnvironment = QProcessEnvironment::systemEnvironment();
        processEnvironment.insert(u"INFO_LOCATION"_s, option);
        testApp.setProcessEnvironment(processEnvironment);
        auto testsAppDir = QDir(QCoreApplication::applicationDirPath());
        auto resourceTestAppFilePath = testsAppDir.absoluteFilePath("Resources/SpectatorSupportsInfoMessagesOutsideScenarioScopeTestApp/SpectatorSupportsInfoMessagesOutsideScenarioScopeTestApp");
        testApp.start(resourceTestAppFilePath);
        if (!testApp.waitForStarted(5000))
            qFatal("%s%s%s", "Failed to wait for ", qUtf8Printable(resourceTestAppFilePath), " test app to start.");
        if (!testApp.waitForFinished(5000))
            qFatal("%s%s%s", "Failed to wait for ", qUtf8Printable(resourceTestAppFilePath), " test app to finish.");
        const auto output = testApp.readAllStandardOutput();
        if (output.isEmpty())
            qFatal().noquote() << "INFO logging outside scenario scope test failed: " << testApp.readAllStandardError() << Qt::endl;
        else
        {
            const auto expectedMessagesInOutput = QByteArrayList()
                << "Buffer Contents: Global Scope"
                << "Assertions: 0"
                << ((option == u"IN_CONSTRUCTOR"_s) ? "INFO: This is the info message in constructor." : "INFO: This is the info message in destructor.");
            for (const auto & expectedMessage : expectedMessagesInOutput)
            {
                if (!output.contains(expectedMessage))
                {
                    qFatal().noquote() << "INFO logging outside scenario scope test failed: "
                                       << "Expected message \"" << expectedMessage << "\" was not found in process output."
                                       << Qt::endl << "Process output: " << output
                                       << Qt::endl;
                }
            }
        }
    }
    qStdOut() << u"PASSED Testing INFO messages outside scenario scope."_s << Qt::endl;
}

static void spectatorSupportsRequireOutsideScenarioScope()
{
    qStdOut() << u"Testing REQUIRE outside scenario scope."_s << Qt::endl;
    // Run test app
    const auto options = QStringList() << u"SUCCEED_IN_CONSTRUCTOR"_s
                                       << u"FAIL_IN_CONSTRUCTOR"_s
                                       << u"SUCCEED_IN_DESTRUCTOR"_s
                                       << u"FAIL_IN_DESTRUCTOR"_s;
    for (const auto & option : options)
    {
        QProcess testApp;
        QProcessEnvironment processEnvironment = QProcessEnvironment::systemEnvironment();
        processEnvironment.insert(u"REQUIRE_TYPE"_s, option);
        testApp.setProcessEnvironment(processEnvironment);
        auto testsAppDir = QDir(QCoreApplication::applicationDirPath());
        auto resourceTestAppFilePath = testsAppDir.absoluteFilePath("Resources/SpectatorSupportsRequireOutsideScenarioScopeTestApp/SpectatorSupportsRequireOutsideScenarioScopeTestApp");
        testApp.start(resourceTestAppFilePath);
        if (!testApp.waitForStarted(5000))
            qFatal("%s%s%s", "Failed to wait for ", qUtf8Printable(resourceTestAppFilePath), " test app to start.");
        if (!testApp.waitForFinished(5000))
            qFatal("%s%s%s", "Failed to wait for ", qUtf8Printable(resourceTestAppFilePath), " test app to finish.");
        const QFileInfo thisFileInfo(__FILE__);
        QDir scenariosDir(thisFileInfo.canonicalPath());
        if (!scenariosDir.cd(u"Resources"_s) || !scenariosDir.cd(u"SpectatorSupportsRequireOutsideScenarioScopeTestApp"_s))
            qFatal("Failed to navigate to directory containing scenario source files.");
        const auto mainFilePath = scenariosDir.absoluteFilePath(u"main.cpp"_s);
        QByteArrayList expectedMessages;
        if (option == u"SUCCEED_IN_CONSTRUCTOR"_s)
        {
            expectedMessages = QByteArrayList() << "Global Scope" << "Assertions: 1";
        }
        else if (option == u"FAIL_IN_CONSTRUCTOR"_s)
        {
            expectedMessages = QByteArrayList() << QByteArray("REQUIRE failed at file://").append(mainFilePath.toUtf8()).append(":27.");
        }
        else if (option == u"SUCCEED_IN_DESTRUCTOR"_s)
        {
            expectedMessages = QByteArrayList() << "Global Scope" << "Assertions: 0";
        }
        else if (option == u"FAIL_IN_DESTRUCTOR"_s)
        {
            expectedMessages = QByteArrayList() << "Global Scope" << "Assertions: 0" << QByteArray("REQUIRE failed at file://").append(mainFilePath.toUtf8()).append(":39.");
        }
        else
            qFatal().noquote() << "Require outside scenario scope test failed: Invalid option value of: " << option << Qt::endl;
        const auto output = testApp.readAllStandardOutput() + testApp.readAllStandardError();
        if (output.isEmpty())
            QTextStream(stdout) << "Require outside scenario scope test failed: test app did not generate neither of standard/error output." << Qt::endl;
        else
        {
            for (const auto & expectedMessage : expectedMessages)
            {
                if (!output.contains(expectedMessage))
                {
                    qFatal().noquote() << "REQUIRE outside scenario scope test failed: "
                                       << "Expected message \"" << expectedMessage << "\" was not found in process output."
                                       << Qt::endl << "Process output: " << output
                                       << Qt::endl;
                }
            }
        }
    }
    qStdOut() << u"PASSED Testing Require outside scenario scope."_s << Qt::endl;
}

static void spectatorVisitsAllLeafNodesOfScenarioPathWithoutGeneratorsOnce()
{
    qStdOut() << u"Testing Spectator Visits All Leaf Nodes Of Scenario Path Without Generators Once."_s << Qt::endl;
    // Run test app
    QProcess testApp;
    auto testsAppDir = QDir(QCoreApplication::applicationDirPath());
    auto resourceTestAppFilePath = testsAppDir.absoluteFilePath("Resources/SpectatorVisitsAllLeafNodesOfScenarioPathWithoutGeneratorsOnceTestApp/SpectatorVisitsAllLeafNodesOfScenarioPathWithoutGeneratorsOnceTestApp");
    testApp.start(resourceTestAppFilePath);
    if (!testApp.waitForFinished(5000))
        qFatal("%s%s%s", "Failed to wait for ", qUtf8Printable(resourceTestAppFilePath), " test app to finish.");
    if (testApp.exitCode() != 0 || testApp.exitStatus() != QProcess::NormalExit)
    {
        qFatal().noquote() << "FAILED Spectator Visits All Leaf Nodes Of Scenario Path Without Generators Once!"
                           << Qt::endl
                           << "Standard error: "
                           << Qt::endl
                           << testApp.readAllStandardError()
                           << Qt::endl;
    }
    const auto output = testApp.readAllStandardOutput();
    const auto buffer = QByteArray::fromBase64(output);
    QDataStream dataStream(buffer);
    QMap<QString, qint64> results;
    dataStream >> results;
    const auto expectedResults = QMap<QString, qint64>({{u"Scenario"_s, 5},
                                                        {u"1"_s, 4},
                                                        {u"1.1"_s, 2},
                                                        {u"1.1.1"_s, 1},
                                                        {u"1.1.2"_s, 1},
                                                        {u"1.2"_s, 2},
                                                        {u"1.2.1"_s, 1},
                                                        {u"1.2.2"_s, 1},
                                                        {u"1.2.2.1"_s, 1},
                                                        {u"1.2.2.1.1"_s, 1},
                                                        {u"1.2.2.1.1.1"_s, 1},
                                                        {u"a"_s, 1},
                                                        {u"a.1"_s, 1},
                                                        {u"a.2"_s, 1},
                                                        {u"a.3"_s, 1},
                                                        {u"a.4"_s, 1},
                                                        {u"a.5"_s, 1},
                                                        {u"a.6"_s, 1},
                                                        {u"a.7"_s, 1}});
    if (results != expectedResults)
    {
        qFatal() << "FAILED Spectator Visits All Leaf Nodes Of Scenario Path Without Generators Once!"
                 << Qt::endl
                 << "Results did not match expected results."
                 << Qt::endl;
    }
    qStdOut() << u"PASSED Testing Spectator Visits All Leaf Nodes Of Scenario Path Without Generators Once."_s << Qt::endl;
}

static void spectatorKeepsVisitingLeafNodeOfScenarioPathUntilExhaustingDataOfAllGeneratorsOnPath()
{
    qStdOut() << u"Testing Spectator Keeps Visiting Leaf Node Of Scenario Path Until Exhausting Data Of All Generators On Path."_s << Qt::endl;
    // Run test app
    QProcess testApp;
    auto testsAppDir = QDir(QCoreApplication::applicationDirPath());
    auto resourceTestAppFilePath = testsAppDir.absoluteFilePath("Resources/SpectatorKeepsVisitingLeafNodeOfScenarioPathUntilExhaustingDataOfAllGeneratorsOnPathTestApp/SpectatorKeepsVisitingLeafNodeOfScenarioPathUntilExhaustingDataOfAllGeneratorsOnPathTestApp");
    testApp.start(resourceTestAppFilePath);
    if (!testApp.waitForFinished(5000))
        qFatal("%s%s%s", "Failed to wait for ", qUtf8Printable(resourceTestAppFilePath), " test app to finish.");
    if (testApp.exitCode() != 0 || testApp.exitStatus() != QProcess::NormalExit)
    {
        qFatal().noquote() << "FAILED Spectator Keeps Visiting Leaf Node Of Scenario Path Until Exhausting Data Of All Generators On Path!"
                           << Qt::endl
                           << "Standard error: "
                           << Qt::endl
                           << testApp.readAllStandardError()
                           << Qt::endl;
    }
    const auto output = testApp.readAllStandardOutput();
    const auto buffer = QByteArray::fromBase64(output);
    QDataStream dataStream(buffer);
    QMap<QString, qint64> results;
    dataStream >> results;
    const auto expectedResults = QMap<QString, qint64>({{u"Scenario"_s, 90},
                                                        {u"1"_s, 84},
                                                        {u"1.1"_s, 72},
                                                        {u"1.1.1"_s, 48},
                                                        {u"1.1.2"_s, 24},
                                                        {u"1.2"_s, 12},
                                                        {u"1.2.1"_s, 6},
                                                        {u"1.2.2"_s, 6},
                                                        {u"1.2.2.1"_s, 6},
                                                        {u"1.2.2.1.1"_s, 6},
                                                        {u"1.2.2.1.1.1"_s, 6},
                                                        {u"a"_s, 2},
                                                        {u"a.1"_s, 2},
                                                        {u"a.2"_s, 2},
                                                        {u"a.3"_s, 2},
                                                        {u"a.4"_s, 2},
                                                        {u"a.5"_s, 2},
                                                        {u"a.6"_s, 2},
                                                        {u"a.7"_s, 2}});
    if (results != expectedResults)
    {
        qFatal() << "FAILED Spectator Keeps Visiting Leaf Node Of Scenario Path Until Exhausting Data Of All Generators On Path!"
                 << Qt::endl
                 << "Results did not match expected results."
                 << Qt::endl;
    }
    qStdOut() << u"PASSED Spectator Keeps Visiting Leaf Node Of Scenario Path Until Exhausting Data Of All Generators On Path."_s << Qt::endl;
}

static constexpr auto checkStringsInText = [](QStringList strings, QString text) -> bool
{
    for (const auto &str : strings)
    {
        if (!text.contains(str))
            return false;
    }
    return true;
};

static void spectatorOutputsAllScenarioPathsRan()
{
    qStdOut() << u"Testing Spectator Outputs All Scenario Paths Ran."_s << Qt::endl;
    // Run test app
    QProcess testApp;
    auto testsAppDir = QDir(QCoreApplication::applicationDirPath());
    auto resourceTestAppFilePath = testsAppDir.absoluteFilePath("Resources/SpectatorOutputsAllScenarioPathsRanTestApp/SpectatorOutputsAllScenarioPathsRanTestApp");
    testApp.start(resourceTestAppFilePath);
    if (!testApp.waitForFinished(5000))
        qFatal("%s%s%s", "Failed to wait for ", qUtf8Printable(resourceTestAppFilePath), " test app to finish.");
    if (testApp.exitCode() != 0 || testApp.exitStatus() != QProcess::NormalExit)
    {
        qFatal().noquote() << "FAILED Spectator Outputs All Scenario Paths Ran!"
                           << Qt::endl
                           << "Standard error: "
                           << Qt::endl
                           << testApp.readAllStandardError()
                           << Qt::endl;
    }
    const auto output = QString::fromUtf8(testApp.readAllStandardOutput());
    if (output.count(u"Time:"_s) != 8)
    {
        qFatal().noquote() << "FAILED Spectator Outputs All Scenario Paths Ran!"
                        << Qt::endl
                        << "Output must have exactly eight occurrences of \"Time:\""
                        << Qt::endl;
    }
    auto filteredOutput = output;
    filteredOutput.remove(QRegularExpression("Time:.*ms"));
    const auto expectedInitalText = uR"(
------------------------------------------
Passed scenario paths
------------------------------------------)"_s;
    const auto expectedScenarioOutputs = QStringList()
        << u"Scenario: A Scenario without require\nGiven: given\nWhen: when\nThen: then\nAnd When: and when\nThen: then in and when\nStats: [; Run count: 1; Require count: 0]"_s
        << u"Scenario: A Scenario without require\nGiven: given\nWhen: when\nThen: then\nAnd Then: and then\nStats: [; Run count: 1; Require count: 0]"_s
        << u"Scenario: A Scenario without require\nGiven: given\nWhen: when 2\nThen: then 2\nStats: [; Run count: 1; Require count: 0]"_s
        << u"Scenario: A Scenario with a single require\nGiven: given\nWhen: when\nThen: then\nStats: [; Run count: 1; Require count: 1]"_s
        << u"Scenario: A Scenario with two requires\nGiven: given\nWhen: when\nThen: then\nStats: [; Run count: 1; Require count: 2]"_s
        << u"Scenario: A Scenario with one tag\nGiven: given\nWhen: when\nThen: then\nStats: [; Run count: 1; Require count: 2]"_s
        << u"Scenario: A Scenario with multiple tags\nGiven: given\nWhen: when\nThen: then\nStats: [; Run count: 1; Require count: 4]"_s;
    const auto expectedStatsText = uR"(------------------------------------------
Scenario paths stats
------------------------------------------
Total scenarios paths ran: 7
Successful scenarios paths ran: 7
Unsuccessful scenarios paths ran: 0
Total requires in scenarios paths: 9
Successful requires in scenarios paths: 9
Unsuccessful requires in scenarios paths: 0


All 7 scenarios paths passed.)"_s;
    if (!filteredOutput.startsWith(expectedInitalText)
       || !filteredOutput.contains(expectedStatsText)
       || !checkStringsInText(expectedScenarioOutputs, filteredOutput))
    {
        qFatal() << "FAILED Spectator Outputs All Scenario Paths Ran!"
                 << Qt::endl
                 << "Results did not match expected results."
                 << Qt::endl;
    }
    qStdOut() << u"PASSED Spectator Outputs All Scenario Paths Ran."_s << Qt::endl;
}

static void spectatorRunsAllScenariosOnSingleThreadByDefault()
{
    qStdOut() << u"Testing Spectator Runs All Scenarios On Single Thread By Default."_s << Qt::endl;
    // Run test app
    QProcess testApp;
    auto testsAppDir = QDir(QCoreApplication::applicationDirPath());
    auto resourceTestAppFilePath = testsAppDir.absoluteFilePath("Resources/SpectatorRunsAllScenariosOnSingleThreadByDefaultTestApp/SpectatorRunsAllScenariosOnSingleThreadByDefaultTestApp");
    testApp.start(resourceTestAppFilePath);
    if (!testApp.waitForFinished(15000))
        qFatal("%s%s%s", "Failed to wait for ", qUtf8Printable(resourceTestAppFilePath), " test app to finish.");
    if (testApp.exitCode() != 0 || testApp.exitStatus() != QProcess::NormalExit)
    {
        qFatal().noquote() << "FAILED Spectator Runs All Scenarios On Single Thread By Default!"
                           << Qt::endl
                           << "Standard error: "
                           << Qt::endl
                           << testApp.readAllStandardError()
                           << Qt::endl;
    }
    const auto output = QString::fromUtf8(testApp.readAllStandardOutput());
    if (output.count(u"Time:"_s) != 3)
    {
        qFatal().noquote() << "FAILED Spectator Runs All Scenarios On Single Thread By Default!"
                        << Qt::endl
                        << "Output must have exactly three occurrences of \"Time:\""
                        << Qt::endl;
    }
    auto filteredOutput = output;
    filteredOutput.remove(QRegularExpression("Time:.*ms"));
    const auto expectedInitalText = uR"(
------------------------------------------
Passed scenario paths
------------------------------------------)"_s;
    const auto expectedScenarioOutputs = QStringList()
        << u"Scenario: Scenario 1\nStats: [; Run count: 1; Require count: 0]"_s
        << u"Scenario: Scenario 2\nStats: [; Run count: 1; Require count: 0]"_s;
    const auto expectedStatsText = uR"(------------------------------------------
Scenario paths stats
------------------------------------------
Total scenarios paths ran: 2
Successful scenarios paths ran: 2
Unsuccessful scenarios paths ran: 0
Total requires in scenarios paths: 0
Successful requires in scenarios paths: 0
Unsuccessful requires in scenarios paths: 0


All 2 scenarios paths passed.)"_s;
    if (!filteredOutput.startsWith(expectedInitalText)
       || !filteredOutput.contains(expectedStatsText)
       || !checkStringsInText(expectedScenarioOutputs, filteredOutput))
    {
        qFatal() << "FAILED Spectator Runs All Scenarios On Single Thread By Default!"
                 << Qt::endl
                 << "Results did not match expected results."
                 << Qt::endl;
    }
    qStdOut() << u"PASSED Spectator Runs All Scenarios On Single Thread By Default."_s << Qt::endl;
}

static void spectatorSupportsRunningScenariosOnMultipleThreads()
{
    qStdOut() << u"Testing Spectator Supports Running Scenarios On Multiple Threads."_s << Qt::endl;
    // Run test app
    QProcess testApp;
    auto testsAppDir = QDir(QCoreApplication::applicationDirPath());
    auto resourceTestAppFilePath = testsAppDir.absoluteFilePath("Resources/SpectatorSupportsRunningScenariosOnMultipleThreadsTestApp/SpectatorSupportsRunningScenariosOnMultipleThreadsTestApp");
    testApp.start(resourceTestAppFilePath, QStringList() << "-j" << "-1");
    if (!testApp.waitForFinished(10000))
        qFatal("%s%s%s", "Failed to wait for ", qUtf8Printable(resourceTestAppFilePath), " test app to finish.");
    if (testApp.exitCode() != 0 || testApp.exitStatus() != QProcess::NormalExit)
    {
        qFatal().noquote() << "FAILED Spectator Supports Running Scenarios On Multiple Threads!"
                           << Qt::endl
                           << "Standard error: "
                           << Qt::endl
                           << testApp.readAllStandardError()
                           << Qt::endl;
    }
    const auto output = QString::fromUtf8(testApp.readAllStandardOutput());
    if (output.count(u"Time:"_s) != 3)
    {
        qFatal().noquote() << "FAILED Spectator Supports Running Scenarios On Multiple Threads!"
                        << Qt::endl
                        << "Output must have exactly three occurrences of \"Time:\""
                        << Qt::endl;
    }
    auto filteredOutput = output;
    filteredOutput.remove(QRegularExpression("Time:.*ms"));
    const auto expectedInitalText = uR"(
------------------------------------------
Passed scenario paths
------------------------------------------)"_s;
    const auto expectedScenarioOutputs = QStringList()
        << u"Scenario: Scenario 1\nStats: [; Run count: 1; Require count: 0]"_s
        << u"Scenario: Scenario 2\nStats: [; Run count: 1; Require count: 0]"_s;
    const auto expectedStatsText = uR"(------------------------------------------
Scenario paths stats
------------------------------------------
Total scenarios paths ran: 2
Successful scenarios paths ran: 2
Unsuccessful scenarios paths ran: 0
Total requires in scenarios paths: 0
Successful requires in scenarios paths: 0
Unsuccessful requires in scenarios paths: 0


All 2 scenarios paths passed.)"_s;
    if (!filteredOutput.startsWith(expectedInitalText)
       || !filteredOutput.contains(expectedStatsText)
       || !checkStringsInText(expectedScenarioOutputs, filteredOutput))
    {
        qFatal() << "FAILED Spectator Supports Running Scenarios On Multiple Threads!"
                 << Qt::endl
                 << "Results did not match expected results."
                 << Qt::endl;
    }
    qStdOut() << u"PASSED Spectator Supports Running Scenarios On Multiple Threads."_s << Qt::endl;
}

static void spectatorSupportsScenarioFiltering()
{
    qStdOut() << u"Spectator Supports Scenario Filtering."_s << Qt::endl;
    // Run test app
    QProcess testApp;
    auto testsAppDir = QDir(QCoreApplication::applicationDirPath());
    auto resourceTestAppFilePath = testsAppDir.absoluteFilePath("Resources/SpectatorSupportsScenarioFilteringTestApp/SpectatorSupportsScenarioFilteringTestApp");
    const auto testData = []() -> QVector<std::pair<QStringList, QStringList>>
    {
        QVector<std::pair<QStringList, QStringList>> data(3);
        data[0].first = QStringList() << u"-t"_s << u"a tag"_s;
        data[0].second = QStringList() << u"Scenario: Scenario with a tag\nStats: [; Run count: 1; Require count: 0]"_s
                                       << u"Total scenarios paths ran: 1"_s
                                       << u"Successful scenarios paths ran: 1"_s
                                       << u"Unsuccessful scenarios paths ran: 0"_s
                                       << u"All 1 scenarios paths passed."_s;
        data[1].first = QStringList() << u"-s"_s << u"Scenario with no tag"_s;
        data[1].second = QStringList() << u"Scenario: Scenario with no tag\nStats: [; Run count: 1; Require count: 0]"_s
                                       << u"Total scenarios paths ran: 1"_s
                                       << u"Successful scenarios paths ran: 1"_s
                                       << u"Unsuccessful scenarios paths ran: 0"_s
                                       << u"All 1 scenarios paths passed."_s;
        const QFileInfo thisFileInfo(__FILE__);
        QDir scenariosDir(thisFileInfo.canonicalPath());
        if (!scenariosDir.cd(u"Resources"_s) || !scenariosDir.cd(u"SpectatorSupportsScenarioFilteringTestApp"_s))
            qFatal("Failed to navigate to directory containing scenario source files.");
        data[2].first = QStringList() << u"-f"_s << scenariosDir.absoluteFilePath(u"scenario_on_another_file.cpp"_s);
        data[2].second = QStringList() << u"Scenario: Scenario on another file\nStats: [; Run count: 1; Require count: 0]"_s
                                       << u"Total scenarios paths ran: 1"_s
                                       << u"Successful scenarios paths ran: 1"_s
                                       << u"Unsuccessful scenarios paths ran: 0"_s
                                       << u"All 1 scenarios paths passed."_s;
        return data;
    }();
    for (const auto &currentTestData : testData)
    {
        testApp.start(resourceTestAppFilePath, currentTestData.first);
        if (!testApp.waitForFinished(5000))
            qFatal("%s%s%s", "Failed to wait for ", qUtf8Printable(resourceTestAppFilePath), " test app to finish.");
        if (testApp.exitCode() != 0 || testApp.exitStatus() != QProcess::NormalExit)
        {
            qFatal().noquote() << "FAILED Spectator Supports Scenario Filtering!"
                            << Qt::endl
                            << "Standard error: "
                            << Qt::endl
                            << testApp.readAllStandardError()
                            << Qt::endl;
        }
        const auto output = QString::fromUtf8(testApp.readAllStandardOutput());
        if (output.count(u"Time:"_s) != 2)
        {
            qFatal().noquote() << "FAILED Spectator Supports Scenario Filtering!"
                            << Qt::endl
                            << "Output must have exactly two occurrences of \"Time:\""
                            << Qt::endl;
        }
        auto filteredOutput = output;
        filteredOutput.remove(QRegularExpression("Time:.*ms"));
        
        if (!checkStringsInText(currentTestData.second, filteredOutput))
        {
            qFatal() << "FAILED Spectator Supports Scenario Filtering!"
                    << Qt::endl
                    << "Results did not match expected results."
                    << Qt::endl;
        }
    }
    qStdOut() << u"PASSED Spectator Supports Scenario Filtering."_s << Qt::endl;
}

static void spectatorSupportsTestRepetition()
{
    qStdOut() << u"Testing Spectator Supports Test Repetition."_s << Qt::endl;
    // Run test app
    QProcess testApp;
    auto testsAppDir = QDir(QCoreApplication::applicationDirPath());
    auto resourceTestAppFilePath = testsAppDir.absoluteFilePath("Resources/SpectatorOutputsAllScenarioPathsRanTestApp/SpectatorOutputsAllScenarioPathsRanTestApp");
    testApp.start(resourceTestAppFilePath, QStringList() << "-r" << "10");
    if (!testApp.waitForFinished(5000))
        qFatal("%s%s%s", "Failed to wait for ", qUtf8Printable(resourceTestAppFilePath), " test app to finish.");
    if (testApp.exitCode() != 0 || testApp.exitStatus() != QProcess::NormalExit)
    {
        qFatal().noquote() << "FAILED Spectator Supports Test Repetition!"
                           << Qt::endl
                           << "Standard error: "
                           << Qt::endl
                           << testApp.readAllStandardError()
                           << Qt::endl;
    }
    const auto output = QString::fromUtf8(testApp.readAllStandardOutput());
    if (output.count(u"Time:"_s) != 8)
    {
        qFatal().noquote() << "FAILED Spectator Supports Test Repetition!"
                        << Qt::endl
                        << "Output must have exactly eight occurrences of \"Time:\""
                        << Qt::endl;
    }
    auto filteredOutput = output;
    filteredOutput.remove(QRegularExpression("Time:.*ms"));
    const auto expectedInitalText = uR"(Repeating tests 10 times.
Repeated tests 10 times.

------------------------------------------
Passed scenario paths
------------------------------------------)"_s;
    const auto expectedScenarioOutputs = QStringList()
        << u"Scenario: A Scenario without require\nGiven: given\nWhen: when\nThen: then\nAnd When: and when\nThen: then in and when\nStats: [; Run count: 10; Require count: 0]"_s
        << u"Scenario: A Scenario without require\nGiven: given\nWhen: when\nThen: then\nAnd Then: and then\nStats: [; Run count: 10; Require count: 0]"_s
        << u"Scenario: A Scenario without require\nGiven: given\nWhen: when 2\nThen: then 2\nStats: [; Run count: 10; Require count: 0]"_s
        << u"Scenario: A Scenario with a single require\nGiven: given\nWhen: when\nThen: then\nStats: [; Run count: 10; Require count: 10]"_s
        << u"Scenario: A Scenario with two requires\nGiven: given\nWhen: when\nThen: then\nStats: [; Run count: 10; Require count: 20]"_s
        << u"Scenario: A Scenario with one tag\nGiven: given\nWhen: when\nThen: then\nStats: [; Run count: 10; Require count: 20]"_s
        << u"Scenario: A Scenario with multiple tags\nGiven: given\nWhen: when\nThen: then\nStats: [; Run count: 10; Require count: 40]"_s;
    const auto expectedStatsText = uR"(------------------------------------------
Scenario paths stats
------------------------------------------
Total scenarios paths ran: 70
Successful scenarios paths ran: 70
Unsuccessful scenarios paths ran: 0
Total requires in scenarios paths: 90
Successful requires in scenarios paths: 90
Unsuccessful requires in scenarios paths: 0


All 70 scenarios paths passed.)"_s;
    if (!filteredOutput.startsWith(expectedInitalText)
       || !filteredOutput.contains(expectedStatsText)
       || !checkStringsInText(expectedScenarioOutputs, filteredOutput))
    {
        qFatal() << "FAILED Spectator Supports Test Repetition!"
                 << Qt::endl
                 << "Results did not match expected results."
                 << Qt::endl;
    }
    qStdOut() << u"PASSED Spectator Supports Test Repetition."_s << Qt::endl;
}
