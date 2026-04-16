// Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only OR MPL-2.0-no-copyleft-exception

#ifndef SPECTATOR_GENERATOR_DATA_RECORDER_H
#define SPECTATOR_GENERATOR_DATA_RECORDER_H

#include <QString>
#include <QtTypes>
#include <QtClassHelperMacros>
#include <QByteArray>
#include <QDataStream>
#include <QList>
#include <cstdint>

namespace Spectator::Test
{

struct GeneratorData
{
    QString string;
    int intValue = 0;
    int64_t int64Value = 0;
    QByteArray byteArray;

    friend inline QDataStream &operator<<(QDataStream &stream, const GeneratorData &data)
    {
        stream << data.string;
        stream << data.intValue;
        stream << qint64(data.int64Value);
        stream << data.byteArray;
        return stream;
    }

    friend inline QDataStream &operator>>(QDataStream &stream, GeneratorData & data)
    {
        stream >> data.string;
        stream >> data.intValue;
        qint64 val = 0;
        stream >> val;
        data.int64Value = val;
        stream >> data.byteArray;
        return stream;
    }
};

class GeneratorDataRecorder
{
    Q_DISABLE_COPY_MOVE(GeneratorDataRecorder)
public:
    ~GeneratorDataRecorder() = default;
    static GeneratorDataRecorder & global();
    inline void recordData(const GeneratorData & data) {m_recordedData.append(data);}
    inline QList<GeneratorData> recordedData() const {return m_recordedData;}

private:
    GeneratorDataRecorder() = default;

private:
    QList<GeneratorData> m_recordedData;
};

}

#endif // SPECTATOR_GENERATOR_DATA_RECORDER_H
