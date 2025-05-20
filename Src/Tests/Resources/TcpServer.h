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

#ifndef KOURIER_TCP_SERVER_H
#define KOURIER_TCP_SERVER_H

#include "../../Core/TcpSocket.h"
#include <QTcpServer>


namespace Kourier
{

class TcpServer : private QTcpServer, public Object
{
Q_OBJECT
KOURIER_OBJECT(Kourier::TcpServer)
public:
    TcpServer() = default;
    ~TcpServer() override {close();}
    void setListenBacklogSize(int size) {QTcpServer::setListenBacklogSize(size);}
    void setMaxPendingConnections(int size) {QTcpServer::setMaxPendingConnections(size);}
    bool listen(const QHostAddress &address = QHostAddress::Any, quint16 port = 0) {return QTcpServer::listen(address, port);}
    QHostAddress serverAddress() const {return QTcpServer::serverAddress();}
    quint16 serverPort() const {return QTcpServer::serverPort();}
    void pauseAccepting() {QTcpServer::pauseAccepting();}
    void resumeAccepting() {QTcpServer::resumeAccepting();}
    void close() {QTcpServer::close();}
    auto socketDescriptor() const {return QTcpServer::socketDescriptor();}
    Signal newConnection(TcpSocket *pSocket);

private:
    void incomingConnection(qintptr socketDescriptor) override;
};

}

#endif // KOURIER_TCP_SERVER_H
