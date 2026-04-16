// Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only OR MPL-2.0-no-copyleft-exception

#include "Section.h"

namespace Spectator
{

Section::Section(QStringView sourceFile, qint32 sourceLine, QStringView sectionName) :
    m_sourceFile(sourceFile),
    m_sourceLine(sourceLine),
    m_sectionName(sectionName)
{
}

}
