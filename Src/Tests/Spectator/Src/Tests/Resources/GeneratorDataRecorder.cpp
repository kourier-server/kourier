// Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only OR MPL-2.0-no-copyleft-exception

#include "GeneratorDataRecorder.h"
#include <NoDestroy.h>
#include <QtLogging>

namespace Spectator::Test
{

GeneratorDataRecorder & GeneratorDataRecorder::global()
{
    static NoDestroy<GeneratorDataRecorder*> pInstance(new GeneratorDataRecorder);
    static NoDestroyPtrDeleter<GeneratorDataRecorder*> instanceDeleter(pInstance);
    auto * pGlobal = pInstance();
    if (pGlobal != nullptr) [[likely]]
        return *pGlobal;
    else [[unlikely]]
        qFatal("Global generator data recorder has already been destroyed.");
}

}
