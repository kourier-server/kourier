// Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only OR MPL-2.0-no-copyleft-exception

#ifndef SPECTATOR_SECTION_ENTRY_RECORDER_H
#define SPECTATOR_SECTION_ENTRY_RECORDER_H

#include <QMap>
#include <QString>
#include <QtTypes>
#include <QtClassHelperMacros>

namespace Spectator::Test
{

class SectionEntryRecorder
{
    Q_DISABLE_COPY_MOVE(SectionEntryRecorder)
public:
    ~SectionEntryRecorder() = default;
    static SectionEntryRecorder & global();
    inline void recordEntry(QString sectionName) {++m_entryRecords[sectionName];}
    inline QMap<QString, qint64> recordedEntries() const {return m_entryRecords;}

private:
    SectionEntryRecorder() = default;

private:
    QMap<QString, qint64> m_entryRecords;
};

}

#endif // SPECTATOR_SECTION_ENTRY_RECORDER_H
