//
// Copyright (C) 2024 Glauco Pacheco <glauco@kourier.io>
// SPDX-License-Identifier: AGPL-3.0-only
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, version 3 of the License.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#include <QProcess>
#include <QDir>
#include <QCoreApplication>
#include <QSemaphore>
#include <csignal>
#include <Spectator.h>

using Spectator::SemaphoreAwaiter;


SCENARIO("Listens to UNIX signals")
{
    GIVEN("UnixSignalListenerTestApp is running")
    {
        QProcess testApp;
        auto unitTestsAppDir = QDir(QCoreApplication::applicationDirPath());
        auto testAppFilePath = unitTestsAppDir.absoluteFilePath("UnixSignalListenerTestApp/UnixSignalListenerTestApp");
        QSemaphore testAppReceivedStandardOutputDataSemaphore;
        QObject::connect(&testApp, &QProcess::readyReadStandardOutput,[&]()
        {
            testAppReceivedStandardOutputDataSemaphore.release(1);
        });
        testApp.start(testAppFilePath, QStringList());
        REQUIRE(testApp.waitForStarted(30000));
        pid_t testAppPid = testApp.processId();
        while(true)
        {
            REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(testAppReceivedStandardOutputDataSemaphore, 60));
            QString standardOutput = QString(testApp.readAllStandardOutput());
            if (standardOutput.contains("App is ready."))
                break;
        }

        WHEN("signals that are being listen to are sent to app")
        {
            REQUIRE(0 == ::kill(testAppPid, SIGTERM));
            REQUIRE(0 == ::kill(testAppPid, SIGINT));
            REQUIRE(0 == ::kill(testAppPid, SIGHUP));
            REQUIRE(0 == ::kill(testAppPid, SIGWINCH));
            REQUIRE(0 == ::kill(testAppPid, SIGUSR1));
            REQUIRE(0 == ::kill(testAppPid, SIGUSR2));

            THEN("app successfully processes sent signals")
            {
                QString stdOutput;
                while (true)
                {
                    REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(testAppReceivedStandardOutputDataSemaphore, 60));
                    stdOutput.append(testApp.readAllStandardOutput());
                    if (6 == stdOutput.count("Received UNIX signal"))
                        break;
                }
                REQUIRE(0 == ::kill(testAppPid, SIGKILL));
                REQUIRE(testApp.waitForFinished(30000));
                stdOutput.append(testApp.readAllStandardOutput());
                REQUIRE(stdOutput.contains(QString("Received UNIX signal %1").arg(SIGTERM)));
                REQUIRE(stdOutput.contains(QString("Received UNIX signal %1").arg(SIGINT)));
                REQUIRE(stdOutput.contains(QString("Received UNIX signal %1").arg(SIGHUP)));
                REQUIRE(stdOutput.contains(QString("Received UNIX signal %1").arg(SIGWINCH)));
                REQUIRE(stdOutput.contains(QString("Received UNIX signal %1").arg(SIGUSR1)));
                REQUIRE(stdOutput.contains(QString("Received UNIX signal %1").arg(SIGUSR2)));
            }
        }
    }
}
