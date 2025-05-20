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

#include "QTcpServerBasedConnectionListenerPrivate.h"
#include "QTcpServerBasedConnectionListener.h"


namespace Kourier
{

QTcpServerBasedConnectionListenerPrivate::QTcpServerBasedConnectionListenerPrivate(QTcpServerBasedConnectionListener *pListener) :
    m_pListener(pListener)
{
    assert(m_pListener);
    m_connections.reserve(8192);
}

void QTcpServerBasedConnectionListenerPrivate::processConnections()
{
    m_hasQueuedSlotCall = false;
    for(auto const& socketDescriptor: m_connections)
        m_pListener->newConnection(socketDescriptor);
    m_connections.clear();
}

void QTcpServerBasedConnectionListenerPrivate::incomingConnection(qintptr socketDescriptor)
{
    m_connections.push_back(socketDescriptor);
    if (!m_hasQueuedSlotCall)
    {
        m_hasQueuedSlotCall = true;
        QMetaObject::invokeMethod(this, "processConnections", Qt::QueuedConnection);
    }
}

}
