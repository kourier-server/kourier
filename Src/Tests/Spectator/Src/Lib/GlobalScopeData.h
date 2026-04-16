// Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only OR MPL-2.0-no-copyleft-exception

#ifndef SPECTATOR_GLOBAL_SCOPE_DATA_H
#define SPECTATOR_GLOBAL_SCOPE_DATA_H

#include <QString>

namespace Spectator
{

struct GlobalScopeData
{
    static void incrementSuccessfulRequireCounter();
    static void recordInfoMessage(QString message);
    static void printGlobalStats(QString & buffer);
};

}

#endif // SPECTATOR_GLOBAL_SCOPE_DATA_H
