//
// Copyright (C) 2023 Glauco Pacheco <glauco@kourier.io>
// SPDX-License-Identifier: AGPL-3.0-only
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, version 3 of the License.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#include "Spectator.h"
#include <QCoreApplication>
#include <QTimer>
#include <QRunnable>
#include <QEventLoop>
#include <QThreadPool>
#include <QScopedPointer>
#include <QAtomicInt>
#include <QList>
#include <QByteArrayList>
#include <QMap>
#include <QTextStream>
#include <QElapsedTimer>
#include <QRandomGenerator>
#include <QCommandLineParser>
#include <QMetaObject>

#if defined(_WIN32)
#include <Windows.h>
#else
#include <sys/mman.h>
#include <fcntl.h>
#endif

namespace Spectator
{

class ScenarioRegistrar
{
public:
    static void registerScenario(Scenario *pScenario);
    static const QList<Scenario*> &registeredScenarios();
};

class Settings
{
public:
    Settings() = default;
    ~Settings() = default;
    int threadCount() const {return m_threadCount;}
    quint64 repetitionCount() const {return m_repetitionCount;}
    QByteArray filePathFilter() const {return m_filePathFilter;}
    QByteArray scenarioNameFilter() const {return m_scenarioNameFilter;}
    static Settings fromCmdLine()
    {
        QCommandLineParser parser;
        parser.addOptions({{"j", "Sets the number of threads to use for running the tests.", "<thread count>", "1"},
                           {"r", "Sets how many times the tests must be repeated.", "<repetition count>", "1"},
                           {"f", "Sets the source file name to run. Only scenarios belonging to this file are run.", "<scenario filename>", ""},
                           {"s", "Sets the scenario to run. Only scenarios matching the given name are run.", "<scenario name>", ""}});
        if (!parser.parse(QCoreApplication::arguments()))
            qFatal("%s%s%s", "Failed to parser command line arguments", qUtf8Printable(QCoreApplication::arguments().join(' ')), ".");
        Settings settings;
        if (parser.isSet("j"))
        {
            bool ok = false;
            auto const threadCount = parser.value("j").toInt(&ok);
            if (!ok || threadCount < 0 || threadCount > QThread::idealThreadCount())
                qFatal("Invalid argument value. "
                       "The option -j must have as value a positive integer equal or "
                       "lesser to the value returned by QThread::idealThreadCount.");
            settings.m_threadCount = threadCount;
        }
        if (parser.isSet("r"))
        {
            bool ok = false;
            auto const repetitionCount = parser.value("r").toInt(&ok);
            if (!ok || repetitionCount < 0)
                qFatal("Invalid argument value. "
                       "The option -r must have as value a positive integer.");
            settings.m_repetitionCount = repetitionCount;
        }
        if (parser.isSet("f"))
            settings.m_filePathFilter = parser.value("f").toUtf8();
        if (parser.isSet("s"))
            settings.m_scenarioNameFilter = parser.value("s").toUtf8().prepend("Scenario: ");
        return settings;
    }

private:
    int m_threadCount = 1;
    int m_repetitionCount = 1;
    QByteArray m_filePathFilter;
    QByteArray m_scenarioNameFilter;
};

class TestFilter
{
public:
    TestFilter(Settings settings) :
        m_sourceFileToRun(settings.filePathFilter()),
        m_scenarioNameToRun(settings.scenarioNameFilter()) {}
    ~TestFilter() = default;
    bool hasToRunScenario(Scenario *pScenario) const
    {
        return (m_sourceFileToRun.isEmpty() || pScenario->sourceFileName() == m_sourceFileToRun)
                && (m_scenarioNameToRun.isEmpty() || m_scenarioNameToRun == pScenario->scenarioName());
    }

private:
    QByteArray m_sourceFileToRun;
    QByteArray m_scenarioNameToRun;
};

static quint64 repetitionCount = 1;
static quint64 currentRepetitionCount = 0;
static void runTestsPrivate();
Q_GLOBAL_STATIC(QThreadPool, testsThreadPool);

void processFinishedRunningTests()
{
    if (QThread::currentThread() != QCoreApplication::instance()->thread())
        qFatal("The function processFinishedRunningTests must be executed by the main thread only.");
    if (!testsThreadPool()->waitForDone())
        qFatal("Failed to shutdown thread pool responsible for running scenarios.");
    if (++currentRepetitionCount == repetitionCount)
        QCoreApplication::quit();
    else
    {
        TestRegistrar::reset();
        runTestsPrivate();
    }
}

class TestRunnable : public QRunnable
{
public:
    explicit TestRunnable(Scenario *pScenario) : m_pScenario(pScenario) {};
    ~TestRunnable() override = default;
    void run() override
    {
        m_eventLoop.reset(new QEventLoop);
        m_ctxObject.reset(new QObject);
        QTimer::singleShot(0, m_ctxObject.data(), [this](){m_pScenario->runScenario(); m_eventLoop->exit();});
        m_eventLoop->exec();
        TestRegistrar::incrementGlobalCounters();
        if (hasFinishedRunningTests())
        {
            if (!QMetaObject::invokeMethod(QCoreApplication::instance(), processFinishedRunningTests, Qt::QueuedConnection))
                qFatal("Failed to process finished tests.");
        }
    }

    static void setTestCount(int testCount)
    {
        if (testCount <= 0)
            qFatal("Test count must be a positive integer.");
        m_sharedTestCounter = testCount;
    }

    static bool hasFinishedRunningTests() {return 0 == --m_sharedTestCounter;}

private:
    Scenario *m_pScenario = nullptr;
    QScopedPointer<QEventLoop> m_eventLoop;
    QScopedPointer<QObject> m_ctxObject;
    static QAtomicInt m_sharedTestCounter;
};

QAtomicInt TestRunnable::m_sharedTestCounter;
Q_GLOBAL_STATIC(QList<Scenario*>, scenariosToRun);

static void runTestsPrivate()
{
    TestRunnable::setTestCount(scenariosToRun()->size());
    QList<Scenario*> randomlyOrderedScenarios;
    auto filteredScenarios = *scenariosToRun();
    while (!filteredScenarios.isEmpty())
        randomlyOrderedScenarios.append(filteredScenarios.takeAt(QRandomGenerator::global()->bounded(0, filteredScenarios.size())));
    for (auto scenario : randomlyOrderedScenarios)
        testsThreadPool()->start(new TestRunnable(scenario));
}

static void runTests()
{
    const auto settings = Settings::fromCmdLine();
    repetitionCount = settings.repetitionCount();
    if (repetitionCount > 1)
        QTextStream(stdout) << "Repeating tests " << repetitionCount << " times." << Qt::endl;
    const TestFilter testFilter(settings);
    testsThreadPool()->setMaxThreadCount(qMax(1, qMin(QThread::idealThreadCount(), settings.threadCount())));
    scenariosToRun()->reserve(4096);
    for (auto registeredScenario : ScenarioRegistrar::registeredScenarios())
    {
        if (testFilter.hasToRunScenario(registeredScenario))
            scenariosToRun()->append(registeredScenario);
    }
    if (scenariosToRun()->isEmpty())
        qFatal("There are no scenarios to run.");
    runTestsPrivate();
}

QList<Scenario*> &scenarios()
{
    static QList<Scenario*> data;
    return data;
}

QMap<QByteArrayView, Scenario*> &scenarioNamePointerMap()
{
    static QMap<QByteArrayView, Scenario*> data;
    return data;
}

QMutex TestRegistrar::m_lock;
QByteArrayList TestRegistrar::m_registeredSuccessfulScenarioMessages;
QByteArrayList TestRegistrar::m_registeredScenarioFailureMessages;

constinit std::atomic_uint64_t TestRegistrarData::m_passedScenarios = 0;
constinit std::atomic_uint64_t TestRegistrarData::m_failedScenarios = 0;
constinit std::atomic_uint64_t TestRegistrarData::m_passedTests = 0;
constinit std::atomic_uint64_t TestRegistrarData::m_failedTests = 0;
constinit std::atomic_uint64_t TestRegistrarData::m_passedAssertions = 0;
constinit std::atomic_uint64_t TestRegistrarData::m_failedAssertions = 0;


thread_local constinit TestRegistrarDataCounter TestRegistrar::m_passedScenarios(TestRegistrarData::m_passedScenarios);
thread_local constinit TestRegistrarDataCounter TestRegistrar::m_failedScenarios(TestRegistrarData::m_failedScenarios);
thread_local constinit TestRegistrarDataCounter TestRegistrar::m_passedTests(TestRegistrarData::m_passedTests);
thread_local constinit TestRegistrarDataCounter TestRegistrar::m_failedTests(TestRegistrarData::m_failedTests);
thread_local constinit TestRegistrarDataCounter TestRegistrar::m_passedAssertions(TestRegistrarData::m_passedAssertions);
thread_local constinit TestRegistrarDataCounter TestRegistrar::m_failedAssertions(TestRegistrarData::m_failedAssertions);

void ScenarioRegistrar::registerScenario(Scenario *pScenario)
{
    if (pScenario)
    {
        if (!scenarioNamePointerMap().contains(pScenario->scenarioName()))
        {
            if (!scenarios().contains(pScenario))
            {
                scenarios().append(pScenario);
                scenarioNamePointerMap().insert(pScenario->scenarioName(), pScenario);
            }
            else
                qFatal("Failed to register scenario. Scenario has already been registered.");
        }
        else
        {
            Scenario *pAlreadyRegisteredScenarioWithSameName = scenarioNamePointerMap()[pScenario->scenarioName()];
            qFatal("%s%s%s%s%s%d%s%s%s%d%s",
                   "Failed to register scenario \"", pScenario->scenarioName().constData(),
                   "\" at ", pScenario->sourceFile().constData(), ":", pScenario->sourceLine(),
                   ". A scenario with same name has already been registered at ",
                   pAlreadyRegisteredScenarioWithSameName->sourceFile().constData(),
                   ":", pAlreadyRegisteredScenarioWithSameName->sourceLine(), ".");
        }
    }
    else
        qFatal("Failed to register scenario. Scenario is null.");
}

const QList<Scenario *> &ScenarioRegistrar::registeredScenarios()
{
    return scenarios();
}

Scenario::Scenario(QByteArrayView sourceFile, int sourceLine, QByteArrayView scenarioName, F f) :
    m_sourceFile(sourceFile),
    m_sourceLine(sourceLine),
    m_scenarioName(scenarioName),
    m_testFcn(f)
{
    if (f)
    {
        const auto lastIndex = qMax(m_sourceFile.lastIndexOf('/'), m_sourceFile.lastIndexOf('\\'));
        if (lastIndex != -1)
        {
            m_sourceFileName = m_sourceFile.sliced(lastIndex + 1);
            ScenarioRegistrar::registerScenario(this);
        }
        else
            qFatal("Source file name could not be fetched for scenario.");
    }
    else
        qFatal("Failed to create scenario. Given test method function is null.");
}

void Scenario::runScenario()
{
    SectionInfo scenarioInfo(SectionType::Scenario, m_scenarioName, m_sourceFile, m_sourceLine);
    SectionRegistrar sectionRegistrar(scenarioInfo);
    struct ThreadLocalSectionRegistrarKeeper
    {
        ThreadLocalSectionRegistrarKeeper(SectionRegistrar *pSectionRegistrar)
        {
            ThreadLocalSectionRegistrar::setSectionRegistrarForCurrentThread(pSectionRegistrar);
        }

        ~ThreadLocalSectionRegistrarKeeper()
        {
            ThreadLocalSectionRegistrar::setSectionRegistrarForCurrentThread(nullptr);
        }
    };
    const ThreadLocalSectionRegistrarKeeper threadLocalSectionRegistrarKeeper(&sectionRegistrar);
    do
    {
        try
        {
            sectionRegistrar.setEnteredScenario();
            m_testFcn();
            sectionRegistrar.setLeavedScenario();
            TestRegistrar::registerSuccessfulTest();
        }
        catch (SpectatorException ex)
        {
            sectionRegistrar.registerFailureMessage(ex.what());
            TestRegistrar::registerScenarioFailureMessages(sectionRegistrar.scenarioMessages());
            TestRegistrar::registerTestFailure();
            TestRegistrar::registerScenarioFailure();
            return;
        }
        catch (std::exception ex)
        {
            sectionRegistrar.registerFailureMessage(QByteArray("Test code threw an unhandled stdandard exception: ").append(ex.what()));
            TestRegistrar::registerScenarioFailureMessages(sectionRegistrar.scenarioMessages());
            TestRegistrar::registerTestFailure();
            TestRegistrar::registerScenarioFailure();
            return;
        }
        catch (...)
        {
            sectionRegistrar.registerFailureMessage(QByteArray("Test code threw an unhandled exception."));
            TestRegistrar::registerScenarioFailureMessages(sectionRegistrar.scenarioMessages());
            TestRegistrar::registerTestFailure();
            TestRegistrar::registerScenarioFailure();
            return;
        }
    } while (!sectionRegistrar.hasRunAllSections());
    TestRegistrar::registerSuccessfulScenarioMessages(sectionRegistrar.scenarioMessages());
    TestRegistrar::registerSuccessfulScenario();
}

QThreadStorage<SectionRegistrar*> ThreadLocalSectionRegistrar::m_threadLocalSectionRegistrar;

#if defined(_WIN32)
bool isBadPtr(volatile void *ptr) {return 0 != IsBadReadPtr(const_cast<const void*>(ptr), 1);}
#else
bool isBadPtr(volatile void *ptr)
{
    if (ptr)
    {
        const auto fileDescriptor = ::memfd_create("Spectator_IsBadPtr_File", 0);
        if (fileDescriptor >= 0)
        {
            ssize_t result = 0;
            do
            {
                result = ::write(fileDescriptor, const_cast<const void*>(ptr), 1);
            }
            while (result == -1 && errno == EINTR);
            const bool isPointerInvalid = (result == -1 && errno == EFAULT);
            ::close(fileDescriptor);
            return isPointerInvalid;
        }
        else
            qFatal("Failed to open file descriptor for testing if pointer refers to valid memory location.");
    }
    else
        return false;
}
#endif

}

QByteArrayList identNumbers(const QList<quint64> &numbers)
{
    QByteArrayList result;
    result.reserve(numbers.size());
    qsizetype maxVal = 0;
    for (const auto &number : numbers)
    {
        result.append(QByteArray::number(number));
        maxVal = qMax(maxVal, result.last().size());
    }
    for (auto &number : result)
    {
        if (const auto size = (maxVal - number.size()); size > 0)
            number.prepend(QByteArray(size, ' '));
    }
    return result;
}

int main(int argc, char ** argv)
{
    QCoreApplication app(argc, argv);
    QElapsedTimer elapsedTimer;
    elapsedTimer.start();
    QTimer::singleShot(0, &app, ::Spectator::runTests);
    const auto returnValue = app.exec();
    const auto timeTakenInMSecs = elapsedTimer.elapsed();
    QTextStream outputStream(stdout);
    outputStream << "\n------------------------------------------\n";
    outputStream << "           SUCCESSFUL TESTS (" << Spectator::TestRegistrar::passedTests() << ")\n";
    outputStream << "------------------------------------------\n\n";
    outputStream << ::Spectator::TestRegistrar::successfulScenariosMessages();
    outputStream << "\n------------------------------------------\n";
    outputStream << "           FAILED TESTS (" << Spectator::TestRegistrar::failedTests() << ")\n";
    outputStream << "------------------------------------------\n\n";
    outputStream << ::Spectator::TestRegistrar::failedScenariosMessages();
    const auto totals = identNumbers(QList<quint64>() << ::Spectator::TestRegistrar::totalScenarios()
                                                      << ::Spectator::TestRegistrar::totalTests()
                                                      << ::Spectator::TestRegistrar::totalAssertions()
                                                      << timeTakenInMSecs);
    const QByteArray totalScenarios = totals[0];
    const QByteArray totalTests = totals[1];
    const QByteArray totalAssertions = totals[2];
    const QByteArray totalTimeTaken = totals[3];
    const auto passed = identNumbers(QList<quint64>() << ::Spectator::TestRegistrar::passedScenarios()
                                                      << ::Spectator::TestRegistrar::passedTests()
                                                      << ::Spectator::TestRegistrar::passedAssertions());
    const QByteArray passedScenarios = passed[0];
    const QByteArray passedTests = passed[1];
    const QByteArray passedAssertions = passed[2];
    const auto failed = identNumbers(QList<quint64>() << ::Spectator::TestRegistrar::failedScenarios()
                                                      << ::Spectator::TestRegistrar::failedTests()
                                                      << ::Spectator::TestRegistrar::failedAssertions());
    const QByteArray failedScenarios = failed[0];
    const QByteArray failedTests = failed[1];
    const QByteArray failedAssertions = failed[2];
    outputStream << "\nTotal Scenarios : " << totalScenarios
                 << " (" << passedScenarios << " Passed, "
                 << failedScenarios << " Failed)\n";
    outputStream << "Total Tests     : " << totalTests
                 << " (" << passedTests << " Passed, "
                 << failedTests << " Failed)\n";
    outputStream << "Total Assertions: " << totalAssertions
                 << " (" << passedAssertions << " Passed, "
                 << failedAssertions << " Failed)\n";
    outputStream << "Time taken (ms) : " << totalTimeTaken << "\n\n";
    outputStream.flush();
    return returnValue;
}
