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

#ifndef KOURIER_CONNECTION_LISTENER_H
#define KOURIER_CONNECTION_LISTENER_H

#include "../Core/Object.h"
#include <QVariant>
#include <QtTypes>
#include <string_view>


namespace Kourier
{

class ConnectionListener : public Object
{
KOURIER_OBJECT(Kourier::ConnectionListener)
public:
    ConnectionListener() = default;
    ConnectionListener(const ConnectionListener&) = delete;
    ConnectionListener &operator=(const ConnectionListener&) = delete;
    ~ConnectionListener() override = default;
    virtual bool start(QVariant data) = 0;
    virtual std::string_view errorMessage() const = 0;
    virtual int backlogSize() const = 0;
    virtual qintptr socketDescriptor() const = 0;
    Signal newConnection(qintptr socketDescriptor);
};

}

#endif // KOURIER_CONNECTION_LISTENER_H
