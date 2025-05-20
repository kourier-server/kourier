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

#include "../../../Core/UnixSignalListener.h"
#include <QObject>
#include <QCoreApplication>
#include <QTextStream>
#include <memory>
#include <csignal>

using Kourier::UnixSignalListener;

void writeSignalInfoToStdOut(int signalNumber);
static void deleteUnixSignalListener(UnixSignalListener *pUnixSignalListener)
{
    if (pUnixSignalListener)
        pUnixSignalListener->deleteLater();
}

int main( int argc, char* argv[] )
{
    QCoreApplication app(argc, argv);
    std::unique_ptr<UnixSignalListener, decltype(&deleteUnixSignalListener)> unixSignalListener(new UnixSignalListener({SIGTERM, SIGINT, SIGHUP, SIGWINCH, SIGUSR1, SIGUSR2}), &deleteUnixSignalListener);
    QObject::connect(unixSignalListener.get(), &UnixSignalListener::receivedSignal, &writeSignalInfoToStdOut);
    QTextStream stream(stdout);
    stream << "App is ready." << Qt::endl;
    stream.flush();
    return QCoreApplication::exec();
}

void writeSignalInfoToStdOut(int signalNumber)
{
    QTextStream stream(stdout);
    stream << QString("Received UNIX signal %1.").arg(signalNumber) << Qt::endl;
    stream.flush();
}
