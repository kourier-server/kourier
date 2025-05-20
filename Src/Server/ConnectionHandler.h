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

#ifndef KOURIER_CONNECTION_HANDLER_H
#define KOURIER_CONNECTION_HANDLER_H

#include "../Core/Object.h"


namespace Kourier
{

class ConnectionHandler : public Object
{
KOURIER_OBJECT(Kourier::ConnectionHandler)
public:
    ConnectionHandler() = default;
    ~ConnectionHandler() override = default;
    virtual void finish() = 0;
    Signal finished(ConnectionHandler *pHandler);

private:
    ConnectionHandler *m_pNext = nullptr;
    ConnectionHandler *m_pPrevious = nullptr;
    friend class ConnectionHandlerRepository;
};

}

#endif // KOURIER_CONNECTION_HANDLER_H
