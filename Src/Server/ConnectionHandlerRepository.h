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

#ifndef KOURIER_CONNECTION_HANDLER_REPOSITORY_H
#define KOURIER_CONNECTION_HANDLER_REPOSITORY_H

#include "../Core/Object.h"


namespace Kourier
{
class ConnectionHandler;

class ConnectionHandlerRepository : public Object
{
    KOURIER_OBJECT(Kourier::ConnectionHandlerRepository)
public:
    ConnectionHandlerRepository() = default;
    ConnectionHandlerRepository(const ConnectionHandlerRepository&) = delete;
    ConnectionHandlerRepository &operator=(const ConnectionHandlerRepository&) = delete;
    ~ConnectionHandlerRepository() override;
    void add(ConnectionHandler *pHandler);
    void stop();
    Signal stopped();
    size_t handlerCount() const {return m_handlersCount;}

private:
    void onHandlerFinished(ConnectionHandler *pHandler);

private:
    ConnectionHandler *m_pHandlers = nullptr;
    ConnectionHandler *m_pNextHandlerToBeFinished = nullptr;
    size_t m_handlersCount = 0;
    bool m_isStopping = false;
};

}

#endif // KOURIER_CONNECTION_HANDLER_REPOSITORY_H
