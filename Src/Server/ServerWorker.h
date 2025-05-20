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

#ifndef KOURIER_SERVER_WORKER_H
#define KOURIER_SERVER_WORKER_H

#include "ExecutionState.h"
#include <QObject>
#include <QVariant>


namespace Kourier
{

class ConnectionListener;
class ConnectionHandlerFactory;
class ConnectionHandlerRepository;
class ServerWorkerImpl;


class ServerWorker : public QObject
{
Q_OBJECT
public:
    ServerWorker();
    ServerWorker(std::shared_ptr<ConnectionListener> pListener,
                 std::shared_ptr<ConnectionHandlerFactory> pHandlerFactory,
                 std::shared_ptr<ConnectionHandlerRepository> pHandlerRepository);
    ServerWorker(const ServerWorker &) = delete;
    ServerWorker &operator=(const ServerWorker&) = delete;
    ~ServerWorker() override;

public slots:
    void start(QVariant data) {doStart(data);}
    void stop() {doStop();}

public:
    virtual ExecutionState state() const;
    static constexpr size_t connectionCountMaxLimit() {return std::numeric_limits<int64_t>::max();}

signals:
    void started();
    void stopped();
    void failed(std::string_view errorMessage);

protected:
    virtual void doStart(QVariant data);
    virtual void doStop();

private:
    std::unique_ptr<ServerWorkerImpl> m_pServerWorkerImpl;
};

}

Q_DECLARE_METATYPE(std::shared_ptr<std::atomic_size_t>);

#endif // KOURIER_SERVER_WORKER_H
