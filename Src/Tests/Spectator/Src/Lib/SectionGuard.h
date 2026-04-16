// Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only OR MPL-2.0-no-copyleft-exception

#ifndef SPECTATOR_SECTION_GUARD_H
#define SPECTATOR_SECTION_GUARD_H

#include "SpectatorGlobals.h"
#include <QtClassHelperMacros>

namespace Spectator
{

class Section;
class Scenario;

class SPECTATOR_EXPORT SectionGuard
{
    Q_DISABLE_COPY_MOVE(SectionGuard)
public:
    SectionGuard(Section const * const section, Scenario * pScenario);
    ~SectionGuard();
    inline bool hasEntered() const {return m_hasEntered;}

private:
    Section const * const m_pSection;
    Scenario * m_pScenario = nullptr;
    const bool m_hasEntered;
};

}

#endif // SPECTATOR_SECTION_GUARD_H
