// Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only OR MPL-2.0-no-copyleft-exception

#include "SectionEntryRecorder.h"
#include <NoDestroy.h>
#include <QtLogging>

namespace Spectator::Test
{

SectionEntryRecorder & SectionEntryRecorder::global()
{
    static NoDestroy<SectionEntryRecorder*> pInstance(new SectionEntryRecorder);
    static NoDestroyPtrDeleter<SectionEntryRecorder*> instanceDeleter(pInstance);
    auto * pGlobal = pInstance();
    if (pGlobal != nullptr) [[likely]]
        return *pGlobal;
    else [[unlikely]]
        qFatal("Global section entry recorder has already been destroyed.");
}

}
