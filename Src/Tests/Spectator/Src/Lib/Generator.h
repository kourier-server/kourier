// Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only OR MPL-2.0-no-copyleft-exception

#ifndef SPECTATOR_GENERATOR_H
#define SPECTATOR_GENERATOR_H

#include "SpectatorGlobals.h"
#include <QStringView>
#include <QtClassHelperMacros>
#include <QtTypes>
#include <limits>

namespace Spectator
{

template <class T>
struct GeneratorTypeHolder
{
    using Type = T;
};

class Scenario;

class SPECTATOR_EXPORT Generator
{
    Q_DISABLE_COPY_MOVE(Generator)
public:
    Generator(qsizetype size, QStringView sourceFile, qint32 sourceLine);
    ~Generator() = default;
    template <class T>
    const T & currentValue(T const * const pData, Scenario * pScenario) const
    {
        return pData[getCurrentIndex(pScenario, this)];
    }

    template <class T>
    T currentRangeValue(T minVal, T maxVal, T stepVal, Scenario * pScenario) const
    {
        const auto currentValue = minVal + stepVal * getCurrentIndex(pScenario, this);
        return currentValue;
    }
    inline qsizetype size() const {return m_size;}
    inline QStringView sourceFile() const {return m_sourceFile;}
    inline qint32 sourceLine() const {return m_sourceLine;}

private:
    static qsizetype getCurrentIndex(Scenario *pScenario, Generator const * const pGenerator);

private:
    const qsizetype m_size;
    const QStringView m_sourceFile;
    const qint32 m_sourceLine;
};

}

#define AS(...) (::Spectator::GeneratorTypeHolder<__VA_ARGS__>)
#define GET_TYPE(...) __VA_ARGS__::Type
#define GENERATE(TypeHolder, ...) \
    [this]() -> const GET_TYPE TypeHolder & { \
        const static GET_TYPE TypeHolder data[] = {__VA_ARGS__}; \
        assert(sizeof(data)/sizeof(GET_TYPE TypeHolder) > 0); \
        const static ::Spectator::Generator generator(sizeof(data)/sizeof(GET_TYPE TypeHolder), _SPECTATOR_TO_UTF_16_STRING_LITERAL(__FILE__), __LINE__); \
        /* The explicit cast below on the this pointer is permitted per C++20 Standard 11.9.2 (4.4) */ \
        return generator.currentValue<GET_TYPE TypeHolder>(data, (::Spectator::Scenario*)this); \
    }()
#define GENERATE_RANGE(TypeHolder, MIN_VAL, MAX_VAL) \
    [this]() -> GET_TYPE TypeHolder { \
        assert(std::numeric_limits<GET_TYPE TypeHolder>::is_integer && MAX_VAL > MIN_VAL); \
        static ::Spectator::Generator generator(MAX_VAL - MIN_VAL + 1, _SPECTATOR_TO_UTF_16_STRING_LITERAL(__FILE__), __LINE__); \
        /* The explicit cast below on the this pointer is permitted per C++20 Standard 11.9.2 (4.4) */ \
        return generator.currentRangeValue<GET_TYPE TypeHolder>(MIN_VAL, MAX_VAL, 1, (::Spectator::Scenario*)this); \
    }()
#define GENERATE_RANGE_WITH_STEP(TypeHolder, MIN_VAL, MAX_VAL, STEP_VAL) \
    [this]() -> GET_TYPE TypeHolder { \
        assert(std::numeric_limits<GET_TYPE TypeHolder>::is_integer && MAX_VAL > MIN_VAL && STEP_VAL > 0); \
        static ::Spectator::Generator generator((MAX_VAL - MIN_VAL)/STEP_VAL + 1, _SPECTATOR_TO_UTF_16_STRING_LITERAL(__FILE__), __LINE__); \
        /* The explicit cast below on the this pointer is permitted per C++20 Standard 11.9.2 (4.4) */ \
        return generator.currentRangeValue<GET_TYPE TypeHolder>(MIN_VAL, MAX_VAL, STEP_VAL, (::Spectator::Scenario*)this); \
    }()

#endif // SPECTATOR_GENERATOR_H
