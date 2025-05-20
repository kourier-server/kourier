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

#ifndef KOURIER_Q_TCP_SERVER_BASED_CONNECTION_LISTENER_PRIVATE_H
#define KOURIER_Q_TCP_SERVER_BASED_CONNECTION_LISTENER_PRIVATE_H

#include <QTcpServer>
#include <vector>


namespace Kourier
{
class QTcpServerBasedConnectionListener;

class QTcpServerBasedConnectionListenerPrivate : public QTcpServer
{
Q_OBJECT
public:
    QTcpServerBasedConnectionListenerPrivate(QTcpServerBasedConnectionListener *pListener);
    QTcpServerBasedConnectionListenerPrivate(const QTcpServerBasedConnectionListenerPrivate&) = delete;
    QTcpServerBasedConnectionListenerPrivate &operator=(const QTcpServerBasedConnectionListenerPrivate&) = delete;
    ~QTcpServerBasedConnectionListenerPrivate() override = default;

private slots:
    void processConnections();

private:
    void incomingConnection(qintptr socketDescriptor) override;

private:
    QTcpServerBasedConnectionListener * const m_pListener;
    std::vector<qintptr> m_connections;
    bool m_hasQueuedSlotCall = false;
};

}

#endif // KOURIER_Q_TCP_SERVER_BASED_CONNECTION_LISTENER_PRIVATE_H
