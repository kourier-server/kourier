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

#ifndef KOURIER_SPECTATOR_H
#define KOURIER_SPECTATOR_H

#include <QByteArray>
#include <QByteArrayView>
#include <QSet>
#include <QHash>
#include <QStack>
#include <QVector>
#include <QList>
#include <QSemaphore>
#include <QDeadlineTimer>
#include <QCoreApplication>
#include <QMutex>
#include <QAtomicInteger>
#include <QThreadStorage>
#include <functional>
#include <exception>
#include <initializer_list>
#if __has_include(<unistd.h>)
#include <csignal>
#endif


namespace Spectator
{

class GeneratorInfo
{
public:
    GeneratorInfo() = default;
    GeneratorInfo(QByteArrayView sourceFile, int sourceLine, qsizetype size) :
        m_sourceFile(sourceFile),
        m_sourceLine(sourceLine),
        m_size(size),
        m_generatorId(getId(m_sourceFile, m_sourceLine))
    {
        if(m_size <= 0)
            qFatal("Failed to create GenerateInfo. Size must be a positive integer.");
    }
    ~GeneratorInfo() = default;
    uint64_t id() const {return m_generatorId;}
    static quint64 getId(QByteArrayView sourceFile, int sourceLine)
    {
        static thread_local constinit std::atomic_uint64_t counter = 0;
        static thread_local std::map<QByteArrayView, std::map<int, uint64_t>> ids;
        auto &id = ids[sourceFile][sourceLine];
        if (id)
            return id;
        else
        {
            id = ++counter;
            return id;
        }
    }
    void reset() {m_currentIndex = 0;}
    bool canAdvance() const {return m_currentIndex < (m_size - 1);}
    void advance()
    {
        if (++m_currentIndex < m_size)
            return;
        else
            qFatal("GeneratorInfo::advance failed. Index is out of range.");
    }
    qsizetype index() const {return m_currentIndex;}

private:
    QByteArrayView m_sourceFile;
    int m_sourceLine = 0;
    uint64_t m_generatorId = 0;
    qsizetype m_currentIndex = 0;
    qsizetype m_size = 0;
};

class GeneratorInfoRegistrar
{
public:
    GeneratorInfoRegistrar() = default;
    ~GeneratorInfoRegistrar() = default;
    bool tryAdvance()
    {
        qsizetype generatorsToReset = 0;
        for (auto i = m_generators.rbegin(), rend = m_generators.rend(); i != rend; ++i)
        {
            if (i->canAdvance())
            {
                i->advance();
                for (auto idx = (m_generators.size() - generatorsToReset); idx < m_generators.size(); ++idx)
                    m_generators[idx].reset();
                return true;
            }
            else
                ++generatorsToReset;
        }
        for (auto idx = 0; idx < m_generators.size(); ++idx)
            m_generators[idx].reset();
        return false;
    }
    qsizetype currentGeneratorIndex(const GeneratorInfo &generatorInfo)
    {
        if (!m_registeredGenerators.contains(generatorInfo.id()))
        {
            m_registeredGenerators.insert(generatorInfo.id(), m_generators.size());
            m_generators.push(generatorInfo);
        }
        return m_generators[m_registeredGenerators[generatorInfo.id()]].index();
    }

private:
    QHash<uint64_t, qsizetype> m_registeredGenerators;
    QStack<GeneratorInfo> m_generators;
};

enum class SectionType {Scenario, Given, When, Then, AndWhen, AndThen};

class SectionInfo
{
public:
    SectionInfo() = default;
    SectionInfo(SectionType type,
                QByteArrayView sectionName,
                QByteArrayView sourceFile,
                int sourceLine) :
        m_type(type),
        m_sectionName(sectionName),
        m_sourceFile(sourceFile),
        m_sourceLine(sourceLine),
        m_sectionId(getId(m_sourceFile, m_sourceLine)) {}
    ~SectionInfo() = default;

    QByteArrayView sectionName() const {return m_sectionName;}
    uint64_t sectionId() const {return m_sectionId;}
    static quint64 getId(QByteArrayView sourceFile, int sourceLine)
    {
        static thread_local constinit std::atomic_uint64_t counter = 0;
        static thread_local std::map<QByteArrayView, std::map<int, uint64_t>> ids;
        auto &id = ids[sourceFile][sourceLine];
        if (id)
            return id;
        else
        {
            id = ++counter;
            return id;
        }
    }
    void setEnteredSection() {}
    void setLeavedSection()
    {
        m_currentSubSection = 0;
        m_hasFetchedSubSections = true;
    }
    bool hasFinished()
    {
        if (m_hasFetchedSubSections
            && (m_subSections == m_subSectionsToSkip))
        {
            m_subSectionsToSkip.clear();
            return !m_generatorsRegistrar.tryAdvance();
        }
        else
            return false;
    }
    bool canEnterSubSection(uint64_t id)
    {
        m_subSections.insert(id);
        if (m_subSectionsToSkip.contains(id))
            return false;
        else
        {
            if (!m_currentSubSection)
            {
                m_currentSubSection = id;
                return true;
            }
            else
                return false;
        }
    }
    void leavingSubSection(bool hasFinished)
    {
        if (hasFinished)
        {
            m_subSectionsToSkip.insert(m_currentSubSection);
        }
    }
    qsizetype currentGeneratorIndex(const GeneratorInfo &generatorInfo)
    {
        return m_generatorsRegistrar.currentGeneratorIndex(generatorInfo);
    }

private:
    SectionType m_type = SectionType::Scenario;
    QByteArrayView m_sectionName;
    QByteArrayView m_sourceFile;
    int m_sourceLine = 0;
    uint64_t m_sectionId = 0;
    uint64_t m_currentSubSection = 0;
    QSet<uint64_t> m_subSections;
    QSet<uint64_t> m_subSectionsToSkip;
    GeneratorInfoRegistrar m_generatorsRegistrar;
    bool m_hasAlreadyEnteredSubSection = false;
    bool m_hasFetchedSubSections = false;
};

struct TestRegistrarData
{
    static constinit std::atomic_uint64_t m_passedScenarios;
    static constinit std::atomic_uint64_t m_failedScenarios;
    static constinit std::atomic_uint64_t m_passedTests;
    static constinit std::atomic_uint64_t m_failedTests;
    static constinit std::atomic_uint64_t m_passedAssertions;
    static constinit std::atomic_uint64_t m_failedAssertions;
};

class TestRegistrarDataCounter
{
public:
    constexpr TestRegistrarDataCounter(std::atomic_uint64_t &globalCounter) : m_globalCounter(globalCounter) {}
    ~TestRegistrarDataCounter() = default;
    inline void incrementGlobalCounter() {m_globalCounter.fetch_add(m_counter, std::memory_order_relaxed); m_counter = 0;}
    inline uint64_t operator++() {return ++m_counter;}
    inline uint64_t &operator=(const uint64_t &other) {m_counter = other; return m_counter;}
    inline uint64_t value() const {return m_counter;}

private:
    uint64_t m_counter = 0;
    std::atomic_uint64_t &m_globalCounter;
};

class TestRegistrar
{
public:
    static void registerSuccessfulScenarioMessages(const QByteArray &scenarioMessages)
    {
        QMutexLocker locker(&m_lock);
        m_registeredSuccessfulScenarioMessages.append(scenarioMessages);
    }
    static void registerScenarioFailureMessages(const QByteArray &scenarioMessages)
    {
        QMutexLocker locker(&m_lock);
        m_registeredScenarioFailureMessages.append(scenarioMessages);
    }
    static QByteArray successfulScenariosMessages()
    {
        QMutexLocker locker(&m_lock);
        return m_registeredSuccessfulScenarioMessages.join("\n");
    }
    static QByteArray failedScenariosMessages()
    {
        QMutexLocker locker(&m_lock);
        return m_registeredScenarioFailureMessages.join("\n");
    }
    inline static void registerSuccessfulScenario() {++m_passedScenarios;}
    inline static void registerScenarioFailure() {++m_failedScenarios;}
    inline static void registerSuccessfulTest() {++m_passedTests;}
    inline static void registerTestFailure() {++m_failedTests;}
    inline static void registerSuccessfulAssertion() {++m_passedAssertions;}
    inline static void registerAssertionFailure() {++m_failedAssertions;}
    inline static quint64 passedScenarios() {return TestRegistrarData::m_passedScenarios.load() + m_passedScenarios.value();}
    inline static quint64 failedScenarios() {return TestRegistrarData::m_failedScenarios.load() + m_failedScenarios.value();}
    inline static quint64 totalScenarios() {return passedScenarios() + failedScenarios();}
    inline static quint64 passedTests() {return TestRegistrarData::m_passedTests.load() + m_passedTests.value();}
    inline static quint64 failedTests() {return TestRegistrarData::m_failedTests.load() + m_failedTests.value();}
    inline static quint64 totalTests() {return passedTests() + failedTests();}
    inline static quint64 passedAssertions() {return TestRegistrarData::m_passedAssertions.load() + m_passedAssertions.value();}
    inline static quint64 failedAssertions() {return TestRegistrarData::m_failedAssertions.load() + m_failedAssertions.value();}
    inline static quint64 totalAssertions() {return passedAssertions() + failedAssertions();}
    inline static void incrementGlobalCounters()
    {
        m_passedScenarios.incrementGlobalCounter();
        m_failedScenarios.incrementGlobalCounter();
        m_passedTests.incrementGlobalCounter();
        m_failedTests.incrementGlobalCounter();
        m_passedAssertions.incrementGlobalCounter();
        m_failedAssertions.incrementGlobalCounter();
    }
    static void reset()
    {
        m_registeredSuccessfulScenarioMessages.clear();
        m_registeredScenarioFailureMessages.clear();
        m_passedScenarios = 0;
        m_failedScenarios = 0;
        m_passedTests = 0;
        m_failedTests = 0;
        m_passedAssertions = 0;
        m_failedAssertions = 0;
    }

private:
    static QMutex m_lock;
    static QByteArrayList m_registeredSuccessfulScenarioMessages;
    static QByteArrayList m_registeredScenarioFailureMessages;
    static thread_local constinit TestRegistrarDataCounter m_passedScenarios;
    static thread_local constinit TestRegistrarDataCounter m_failedScenarios;
    static thread_local constinit TestRegistrarDataCounter m_passedTests;
    static thread_local constinit TestRegistrarDataCounter m_failedTests;
    static thread_local constinit TestRegistrarDataCounter m_passedAssertions;
    static thread_local constinit TestRegistrarDataCounter m_failedAssertions;
};

class SectionRegistrar
{
public:
    SectionRegistrar(SectionInfo scenario) :
        m_scenarioId(scenario.sectionId()),
        m_scenarioName(scenario.sectionName())
    {
        registerSection(scenario);
        m_sectionQueue.push(m_scenarioId);
    }
    ~SectionRegistrar() = default;
    bool hasRunAllSections()
    {
        return m_registeredSections[m_scenarioId].hasFinished();
    }
    SectionInfo &top()
    {
        if (m_sectionQueue.isEmpty())
            qFatal("Catcher internal error! Failed to fetch section info. Section queue is empty.");
        return m_registeredSections[m_sectionQueue.top()];
    }
    void setEnteredScenario()
    {
        m_currentTestMessages.append(m_scenarioName).append('\n');
        m_registeredSections[m_scenarioId].setEnteredSection();
    }
    void setLeavedScenario()
    {
        if (!m_testMessages.contains(m_currentTestMessages))
            m_testMessages.append(m_currentTestMessages);
        m_currentTestMessages.clear();
        m_registeredSections[m_scenarioId].setLeavedSection();
    }
    QByteArray scenarioMessages()
    {
        if (!m_currentTestMessages.isEmpty() && !m_testMessages.contains(m_currentTestMessages))
            m_testMessages.append(m_currentTestMessages);
        return m_testMessages.join("\n");
    }
    bool tryPush(const SectionInfo &sectionInfo)
    {
        registerSection(sectionInfo);
        auto &registeredSectionInfo = m_registeredSections[sectionInfo.sectionId()];
        if (top().canEnterSubSection(registeredSectionInfo.sectionId()))
        {
            registeredSectionInfo.setEnteredSection();
            m_sectionQueue.push(registeredSectionInfo.sectionId());
            m_currentTestMessages.append(QByteArray(qMax(0, 2 * (m_sectionQueue.size() - 1)), ' ')).append(sectionInfo.sectionName()).append('\n');
            return true;
        }
        else
            return false;
    }
    void registerMessage(const QByteArray &message)
    {
        m_currentTestMessages.append(QByteArray(2 * m_sectionQueue.size(), ' ')).append("Message: ").append(message).append('\n');
    }
    void registerFailureMessage(const QByteArray &message)
    {
        qsizetype index = m_currentTestMessages.lastIndexOf('\n', -2) + 1;
        qsizetype failureMessageOffset = 0;
        while (index < m_currentTestMessages.size() && m_currentTestMessages[index++] == ' ')
            ++failureMessageOffset;
        failureMessageOffset = qMax(2, failureMessageOffset);
        m_currentTestMessages.append(QByteArray(failureMessageOffset, ' ')).append("Fail: ").append(message).append('\n');
    }
    void pop()
    {
        if (m_sectionQueue.isEmpty())
            qFatal("Catcher internal error! Failed to pop section info. Section queue is empty.");
        auto &previousSectionInfo = m_registeredSections[m_sectionQueue.pop()];
        previousSectionInfo.setLeavedSection();
        if (m_sectionQueue.isEmpty())
            qFatal("Catcher internal error! Failed to pop section info. Section queue is empty.");
        m_registeredSections[m_sectionQueue.top()].leavingSubSection(previousSectionInfo.hasFinished());
    }

private:
    void registerSection(const SectionInfo &sectionInfo)
    {
        if (!m_registeredSections.contains(sectionInfo.sectionId()))
            m_registeredSections.insert(sectionInfo.sectionId(), sectionInfo);
    }

private:
    const uint64_t m_scenarioId = 0;
    const QByteArrayView m_scenarioName;
    QHash<uint64_t, SectionInfo> m_registeredSections;
    QStack<uint64_t> m_sectionQueue;
    QByteArray m_currentTestMessages;
    QByteArrayList m_testMessages;
};

struct ThreadLocalSectionRegistrar
{
    static SectionRegistrar &currentSectionRegistrar()
    {
        if (!m_threadLocalSectionRegistrar.hasLocalData())
            qFatal("Spectator failed. There is no section registrar on the current thread.");
        else
            return *m_threadLocalSectionRegistrar.localData();
    }

    static void setSectionRegistrarForCurrentThread(SectionRegistrar *pSectionRegistrar)
    {
        m_threadLocalSectionRegistrar.localData() = pSectionRegistrar;
    }

private:
    static QThreadStorage<SectionRegistrar*> m_threadLocalSectionRegistrar;
};


class Section
{
public:
    Section(SectionType type,
            QByteArrayView sectionName,
            QByteArrayView sourceFile,
            int sourceLine) :
        m_sectionInfo(type, sectionName, sourceFile, sourceLine),
        m_sectionRegistrar(ThreadLocalSectionRegistrar::currentSectionRegistrar()),
        m_hasEnteredSection(m_sectionRegistrar.tryPush(m_sectionInfo)) {}
    ~Section()
    {
        if (m_hasEnteredSection)
            m_sectionRegistrar.pop();
    }
    bool canEnterSection() {return m_hasEnteredSection;}

private:
    SectionInfo m_sectionInfo;
    SectionRegistrar &m_sectionRegistrar;
    const bool m_hasEnteredSection;
};

template <typename T>
class Generator
{
public:
    T currentValue(std::initializer_list<T> initList, QByteArrayView sourceFile, int sourceLine)
    {
        return QVector<T>(initList)[ThreadLocalSectionRegistrar::currentSectionRegistrar().top().currentGeneratorIndex({sourceFile, sourceLine, static_cast<qsizetype>(initList.size())})];
    }

    T currentRangeValue(T minVal, T maxVal, T stepVal, QByteArrayView sourceFile, int sourceLine)
    {
        const size_t size = (maxVal - minVal + 1)/stepVal;
        const size_t currentIndex = ThreadLocalSectionRegistrar::currentSectionRegistrar().top().currentGeneratorIndex({sourceFile, sourceLine, size});
        const auto currentValue = minVal + stepVal * currentIndex;
        return currentValue;
    }
};

class Scenario
{
public:
    using F = void(*)();
    Scenario(QByteArrayView sourceFile, int sourceLine, QByteArrayView scenarioName, F f);
    ~Scenario() = default;
    QByteArrayView sourceFile() const {return m_sourceFile;}
    QByteArrayView sourceFileName() const {return m_sourceFileName;}
    int sourceLine() const {return m_sourceLine;}
    QByteArrayView scenarioName() const {return m_scenarioName;}
    void runScenario();

private:
    QByteArrayView m_sourceFile;
    QByteArrayView m_sourceFileName;
    int m_sourceLine;
    QByteArrayView m_scenarioName;
    std::function<void()> m_testFcn;
};

class SemaphoreAwaiter
{
public:
    static bool signalSlotAwareWait(QSemaphore &semaphore, int timeoutInSecs)
    {
        if (timeoutInSecs <= 0)
            qFatal("Failed to wait for semaphore. The timeoutInSecs must be a positive integer.");
        QDeadlineTimer deadlineTimer;
        deadlineTimer.setRemainingTime(timeoutInSecs * 1000, Qt::PreciseTimer);
        do
        {
            processEvents();
            if (semaphore.tryAcquire())
            {
                processEvents();
                return true;
            }
            else
                QCoreApplication::processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMoreEvents, 1);
        } while (!deadlineTimer.hasExpired());
        processEvents();
        return semaphore.tryAcquire();
    }

    static bool signalSlotAwareWait(QSemaphore &semaphore, int counter, int timeoutInSecs)
    {
        if (timeoutInSecs <= 0)
            qFatal("Failed to wait for semaphore. The timeoutInSecs must be a positive integer.");
        if (counter <= 0)
            qFatal("Failed to wait for semaphore. counter must be a positive integer.");
        QDeadlineTimer deadlineTimer;
        deadlineTimer.setRemainingTime(timeoutInSecs * 1000, Qt::PreciseTimer);
        do
        {
            QCoreApplication::sendPostedEvents();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            QCoreApplication::processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMoreEvents, qMax(0, deadlineTimer.remainingTime()));
            if (semaphore.tryAcquire(counter))
            {
                QCoreApplication::sendPostedEvents();
                QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
                return true;
            }
        } while (!deadlineTimer.hasExpired());
        QCoreApplication::sendPostedEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        return semaphore.tryAcquire();
    }

private:
    static void processEvents()
    {
        QCoreApplication::sendPostedEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents();
        QCoreApplication::sendPostedEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
};

class SpectatorException : public std::exception
{
public:
    SpectatorException(QByteArray message) : m_message(message) {}
    SpectatorException(QByteArray message, QByteArray sourceFile, int sourceLine) :
        m_message(message)
    {
        m_message.append(" failed at file://")
                .append(sourceFile)
                .append(':')
                .append(QByteArray::number(sourceLine))
                .append('.');
    };
    ~SpectatorException() override = default;
    const char* what() const noexcept override {return m_message.constData();}

private:
    QByteArray m_message;
};

#if __has_include(<unistd.h>)
static void signalHandler(int signum) {signal(SIGTRAP, SIG_DFL);}
static void debugBreak()
{
    static constinit bool firstRun = true;
    if (firstRun)
        signal(SIGTRAP, signalHandler);
    firstRun = false;
    raise(SIGTRAP);
}
#elif defined(_MSC_VER)
static void debugBreak() {__debugbreak();}
#else
static void debugBreak() {}
#endif


inline void require(bool expr, QByteArrayView exprAsString, QByteArrayView sourceFile, int sourceLine)
{
    if (expr) [[likely]]
        TestRegistrar::registerSuccessfulAssertion();
    else
    {
        TestRegistrar::registerAssertionFailure();
        QByteArray message = QByteArray().append("REQUIRE(")
                                 .append(exprAsString).append(") failed at file://")
                                 .append(sourceFile).append(':').append(QByteArray::number(sourceLine))
                                 .append('.');
        debugBreak();
        throw SpectatorException(message);
    }
}

bool isBadPtr(volatile void *ptr);

}

#define _SPECTATOR_CONCATENATE_IMPL_(a, b) a ## b
#define _SPECTATOR_CONCATENATE_(a, b) _SPECTATOR_CONCATENATE_IMPL_(a, b)

#define SCENARIO(SCENARIO_NAME) static void _SPECTATOR_CONCATENATE_(_spectator_scenario_fcn_, __LINE__)(); \
static ::Spectator::Scenario _SPECTATOR_CONCATENATE_(_spectator_scenario, __LINE__)(__FILE__, __LINE__, "Scenario: " SCENARIO_NAME, _SPECTATOR_CONCATENATE_(_spectator_scenario_fcn_, __LINE__)); \
    static void _SPECTATOR_CONCATENATE_(_spectator_scenario_fcn_, __LINE__)()

#define REQUIRE(...) ::Spectator::require(__VA_ARGS__, #__VA_ARGS__, __FILE__, __LINE__);
#define SECTION(TYPE, NAME) if (::Spectator::Section _SPECTATOR_CONCATENATE_(_spectator_section, __LINE__)(TYPE, NAME, __FILE__, __LINE__); _SPECTATOR_CONCATENATE_(_spectator_section, __LINE__).canEnterSection())
#define GIVEN(NAME) SECTION(::Spectator::SectionType::Given, "Given: " NAME)
#define WHEN(NAME) SECTION(::Spectator::SectionType::When, "When: " NAME)
#define THEN(NAME) SECTION(::Spectator::SectionType::Then, "Then: " NAME)
#define AND_WHEN(NAME) SECTION(::Spectator::SectionType::AndWhen, "And When: " NAME)
#define AND_THEN(NAME) SECTION(::Spectator::SectionType::AndThen, "And Then: " NAME)
#define AS(...) (::Spectator::Generator<__VA_ARGS__>())
#define GENERATE(INSTANCE, ...) INSTANCE.currentValue({__VA_ARGS__}, __FILE__, __LINE__)
#define GENERATE_RANGE(INSTANCE, MIN_VAL, MAX_VAL) INSTANCE.currentRangeValue(MIN_VAL, MAX_VAL, 1, __FILE__, __LINE__)
#define GENERATE_RANGE_WITH_STEP(INSTANCE, MIN_VAL, MAX_VAL, STEP_VAL) INSTANCE.currentRangeValue(MIN_VAL, MAX_VAL, STEP_VAL, __FILE__, __LINE__)
#define WARN(MSG) ::Spectator::ThreadLocalSectionRegistrar::currentSectionRegistrar().registerMessage(MSG)
#define FAIL(MSG) throw ::Spectator::SpectatorException(MSG, __FILE__, __LINE__)

#endif // KOURIER_SPECTATOR_H
