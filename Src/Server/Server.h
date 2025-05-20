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

#ifndef KOURIER_SERVER_H
#define KOURIER_SERVER_H

#include "ExecutionState.h"
#include <QObject>
#include <QVariant>
#include <vector>
#include <string>
#include <string_view>
#include <memory>


namespace Kourier
{

class ServerWorkerFactory;
class ServerWorker;

class Server : public QObject
{
Q_OBJECT
public:
    Server(std::shared_ptr<ServerWorkerFactory> pServerWorkerFactory);
    ~Server() override = default;
    bool start(QVariant data);
    void stop();
    inline ExecutionState state() const {return m_state;}
    void setWorkerCount(int workerCount);
    inline int workerCount() const {return m_workerCount;}

signals:
    void started();
    void stopped();
    void failed(std::string_view errorMessage);

private:
    void onWorkerStarted();
    void onWorkerStopped();
    void onWorkerFailed(std::string_view errorMessage);
    void processStartingServerWorkers();
    void processStoppingServerWorkers();

private:
    std::shared_ptr<ServerWorkerFactory> m_pServerWorkerFactory;
    std::vector<std::shared_ptr<ServerWorker>> m_workers;
    std::string m_errorMessage;
    int m_workerCount;
    ExecutionState m_state = ExecutionState::Stopped;
    bool m_pendingStop = false;
    bool m_hasFailed = false;
};

}

#endif // KOURIER_SERVER_H
