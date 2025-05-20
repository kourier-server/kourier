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

#include "UnixSignalListener.h"
#include "UnixUtils.h"
#include "NoDestroy.h"
#include <csignal>
#include <cstring>
#include <sys/signalfd.h>
#include <unistd.h>


namespace Kourier
{

static const bool blockSignalsOnThisThread = []()
{
    UnixSignalListener::blockSignalProcessingForCurrentThread();
    return true;
}();

UnixSignalListener::UnixSignalListener(std::initializer_list<int> signalsToHandle) :
    m_socketNotifier(nullptr, &deleteSocketNotifier)
{
    if (!blockSignalsOnThisThread)
        qFatal("Failed to block signals on current thread.");
    UnixSignalListener::blockSignalProcessingForCurrentThread();
    static constinit NoDestroy<std::atomic_flag> m_singleInstanceFlag;
    if (m_singleInstanceFlag().test_and_set())
        qFatal("Only one instance of UnixSignalListener can be created.");
    sigset_t setOfSignalsToHandle;
    if (0 != sigemptyset(&setOfSignalsToHandle))
        qFatal("Failed to change signal mask for thread.");
    for (const auto signalNumber : signalsToHandle)
    {
        if (0 != sigaddset(&setOfSignalsToHandle, signalNumber))
            qFatal("Failed to set UNIX signal handler.");
    }
    m_signalFd = signalfd(-1, &setOfSignalsToHandle, SFD_NONBLOCK | SFD_CLOEXEC);
    if (m_signalFd < 0)
        qFatal("Failed to create file descriptor to handle UNIX signals.");
    m_socketNotifier.reset(new QSocketNotifier(m_signalFd, QSocketNotifier::Read));
    connect(m_socketNotifier.get(), &QSocketNotifier::activated, this, &UnixSignalListener::processReceivedSignal);
}

UnixSignalListener::~UnixSignalListener()
{
    m_socketNotifier->setEnabled(false);
    ::close(m_signalFd);
}

void UnixSignalListener::processReceivedSignal()
{
    signalfd_siginfo signalInfo{};
    std::memset(&signalInfo, 0, sizeof(signalInfo));
    while (sizeof(signalInfo) == UnixUtils::safeRead(m_signalFd, reinterpret_cast<char*>(&signalInfo), sizeof(signalInfo)))
    {
        emit receivedSignal(static_cast<int>(signalInfo.ssi_signo));
        std::memset(&signalInfo, 0, sizeof(signalInfo));
    }
}

void UnixSignalListener::deleteSocketNotifier(QSocketNotifier *pSocketNotifier)
{
    if (pSocketNotifier)
        pSocketNotifier->deleteLater();
}

void UnixSignalListener::blockSignalProcessingForCurrentThread()
{
    sigset_t allSignalsSet;
    if (0 != sigfillset(&allSignalsSet))
        qFatal("Failed to change signal mask for thread.");
    if (0 != pthread_sigmask(SIG_SETMASK, &allSignalsSet, nullptr))
        qFatal("Failed to change signal mask for thread.");
}

}
