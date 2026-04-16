// Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only OR MPL-2.0-no-copyleft-exception

#ifndef SPECTATOR_SPECTATOR_GLOBALS_H
#define SPECTATOR_SPECTATOR_GLOBALS_H

#include <QtCore/QtGlobal>

#if defined(SPECTATOR_LIBRARY)
    #define SPECTATOR_EXPORT Q_DECL_EXPORT
 #else
    #define SPECTATOR_EXPORT Q_DECL_IMPORT
 #endif

namespace Spectator
{



}

#endif // SPECTATOR_SPECTATOR_GLOBALS_H
