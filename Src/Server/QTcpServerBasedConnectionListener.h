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

#ifndef KOURIER_Q_TCP_SERVER_BASED_CONNECTION_LISTENER_H
#define KOURIER_Q_TCP_SERVER_BASED_CONNECTION_LISTENER_H

#include "ConnectionListener.h"
#include <string>
#include <memory>


namespace Kourier
{
class QTcpServerBasedConnectionListenerPrivate;

class QTcpServerBasedConnectionListener : public ConnectionListener
{
KOURIER_OBJECT(Kourier::QTcpServerBasedConnectionListener)
public:
    QTcpServerBasedConnectionListener();
    QTcpServerBasedConnectionListener(const QTcpServerBasedConnectionListener&) = delete;
    QTcpServerBasedConnectionListener &operator=(const QTcpServerBasedConnectionListener&) = delete;
    ~QTcpServerBasedConnectionListener() override;
    bool start(QVariant data) override;
    std::string_view errorMessage() const override {return m_errorMessage;}
    int backlogSize() const override;
    qintptr socketDescriptor() const override;

private:
    std::unique_ptr<QTcpServerBasedConnectionListenerPrivate> m_pListener;
    std::string m_errorMessage;
    bool m_hasAlreadyStarted = false;
};

}

#endif // KOURIER_Q_TCP_SERVER_BASED_CONNECTION_LISTENER_H
