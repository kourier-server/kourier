// Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only OR MPL-2.0-no-copyleft-exception

#ifndef SPECTATOR_SPECTATOR_EXCEPTION_H
#define SPECTATOR_SPECTATOR_EXCEPTION_H

#include "SpectatorGlobals.h"
#include <QString>
#include <QStringView>
#include <QtTypes>

namespace Spectator
{

class SPECTATOR_EXPORT SpectatorException
{
public:
    SpectatorException(QString message) : m_message(message) {}
    SpectatorException(QString message, QStringView sourceFile, qint32 sourceLine) :
        m_message(QString(message).append(u" thrown at file://")
                                  .append(sourceFile).append(':')
                                  .append(QString::number(sourceLine).append('.')))
    {
    }
    ~SpectatorException() = default;
    QString message() const noexcept {return m_message;}

private:
    QString m_message;
};

}

#endif // SPECTATOR_SPECTATOR_EXCEPTION_H
