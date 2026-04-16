// Copyright (C) 2026 Glauco Pacheco <glaucopacheco@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only OR MPL-2.0-no-copyleft-exception

#include <GlobalScopeData.h>
#include <Spectator.h>
#include <QProcessEnvironment>
#include <QString>
#include <QtLogging>
#include <QDebug>
#include <Qt>
#include <QTextStream>
#include <cstdio>

using namespace Qt::StringLiterals;
using namespace Spectator;

struct RequireTester
{
    RequireTester()
    {
        if (QProcessEnvironment::systemEnvironment().value(u"REQUIRE_TYPE"_s) == u"SUCCEED_IN_CONSTRUCTOR"_s)
        {
            REQUIRE(true);
        }
        else if (QProcessEnvironment::systemEnvironment().value(u"REQUIRE_TYPE"_s) == u"FAIL_IN_CONSTRUCTOR"_s)
        {
            REQUIRE(false);
        }
    }

    ~RequireTester() noexcept(false)
    {
        if (QProcessEnvironment::systemEnvironment().value(u"REQUIRE_TYPE"_s) == u"SUCCEED_IN_DESTRUCTOR"_s)
        {
            REQUIRE(true);
        }
        else if (QProcessEnvironment::systemEnvironment().value(u"REQUIRE_TYPE"_s) == u"FAIL_IN_DESTRUCTOR"_s)
        {
            REQUIRE(false);
        }
    }
};

static RequireTester requireTester;

int main(int argc, char ** argv)
{
    const auto envVarValue = QProcessEnvironment::systemEnvironment().value(u"REQUIRE_TYPE"_s);
    if (envVarValue != u"SUCCEED_IN_CONSTRUCTOR"_s && envVarValue != u"SUCCEED_IN_DESTRUCTOR"_s
        && envVarValue != u"FAIL_IN_CONSTRUCTOR"_s && envVarValue != u"FAIL_IN_DESTRUCTOR"_s)
    {
        qFatal().noquote() << u"Failed to test REQUIRE outside scenario scope. "_s
                           << u"This test requires the environment variable REQUIRE_TYPE to be set "_s
                           << u"to one of: SUCCEED_IN_CONSTRUCTOR, SUCCEED_IN_DESTRUCTOR, FAIL_IN_CONSTRUCTOR or FAIL_IN_DESTRUCTOR."_s << Qt::endl;
    }
    else
    {
        QString buffer;
        buffer.reserve(128);
        Spectator::GlobalScopeData::printGlobalStats(buffer);
        QTextStream(stdout) << u"Buffer Contents: "_s << Qt::endl << buffer << Qt::endl;
    }
    return 0;
}
