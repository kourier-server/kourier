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

#include "TcpServer.h"


namespace Kourier
{

void TcpServer::incomingConnection(qintptr socketDescriptor)
{
    auto *pSocket = new TcpSocket(socketDescriptor);
    if (pSocket->state() == TcpSocket::State::Connected)
        newConnection(pSocket);
    else
        delete pSocket;
}

Signal TcpServer::newConnection(TcpSocket *pSocket) KOURIER_SIGNAL(&TcpServer::newConnection, pSocket)

}
