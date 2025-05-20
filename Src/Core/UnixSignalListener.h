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

#ifndef KOURIER_UNIX_SIGNAL_LISTENER_H
#define KOURIER_UNIX_SIGNAL_LISTENER_H

#include "SDK.h"
#include <QObject>
#include <QSocketNotifier>
#include <memory>
#include <initializer_list>


namespace Kourier
{

class KOURIER_EXPORT UnixSignalListener : public QObject
{
Q_OBJECT
public:
    UnixSignalListener(std::initializer_list<int> signalsToHandle);
    UnixSignalListener(const UnixSignalListener &) = delete;
    ~UnixSignalListener() override;
    UnixSignalListener& operator=(const UnixSignalListener &) = delete;
    Q_SIGNAL void receivedSignal(int signalNumber);
    static void blockSignalProcessingForCurrentThread();

private:
    void processReceivedSignal();
    static void deleteSocketNotifier(QSocketNotifier *pSocketNotifier);

private:
    std::unique_ptr<QSocketNotifier, decltype(&deleteSocketNotifier)> m_socketNotifier;
    int m_signalFd = -1;
};

}

#endif // KOURIER_UNIX_SIGNAL_LISTENER_H
