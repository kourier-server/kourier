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

#ifndef KOURIER_CONNECTION_HANDLER_FACTORY_H
#define KOURIER_CONNECTION_HANDLER_FACTORY_H

#include "ConnectionHandler.h"
#include <QtTypes>


namespace Kourier
{

class ConnectionHandlerFactory
{
public:
    ConnectionHandlerFactory() = default;
    virtual ~ConnectionHandlerFactory() = default;
    virtual ConnectionHandler *create(qintptr socketDescriptor) = 0;

private:
    Q_DISABLE_COPY_MOVE(ConnectionHandlerFactory)
};

}

#endif // KOURIER_CONNECTION_HANDLER_FACTORY_H
