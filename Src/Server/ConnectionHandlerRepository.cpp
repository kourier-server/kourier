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

#include "ConnectionHandlerRepository.h"
#include "ConnectionHandler.h"


namespace Kourier
{

ConnectionHandlerRepository::~ConnectionHandlerRepository()
{
    m_isStopping = true;
    while (m_pHandlers != nullptr)
    {
        auto *pHandler = m_pHandlers;
        m_pHandlers = m_pHandlers->m_pNext;
        Object::disconnect(pHandler, &ConnectionHandler::finished, this, &ConnectionHandlerRepository::onHandlerFinished);
        delete pHandler;
    }
}

void ConnectionHandlerRepository::add(ConnectionHandler *pHandler)
{
    assert(pHandler);
    if (m_isStopping)
        delete pHandler;
    else
    {
        Object::connect(pHandler, &ConnectionHandler::finished, this, &ConnectionHandlerRepository::onHandlerFinished);
        pHandler->m_pNext = m_pHandlers;
        pHandler->m_pPrevious = nullptr;
        if (m_pHandlers)
            m_pHandlers->m_pPrevious = pHandler;
        m_pHandlers = pHandler;
        ++m_handlersCount;
    }
}

void ConnectionHandlerRepository::stop()
{
    if (m_isStopping)
        return;
    m_isStopping = true;
    if (m_handlersCount == 0)
        stopped();
    else
    {
        auto *pHandler = m_pHandlers;
        while (pHandler != nullptr)
        {
            m_pNextHandlerToBeFinished = pHandler->m_pNext;
            pHandler->finish();
            pHandler = m_pNextHandlerToBeFinished;
        }
        m_pNextHandlerToBeFinished = nullptr;
    }
}

Signal ConnectionHandlerRepository::stopped() KOURIER_SIGNAL(&ConnectionHandlerRepository::stopped)

void ConnectionHandlerRepository::onHandlerFinished(ConnectionHandler *pHandler)
{
    assert(pHandler);
    if (pHandler->m_pPrevious)
        pHandler->m_pPrevious->m_pNext = pHandler->m_pNext;
    if (pHandler->m_pNext)
        pHandler->m_pNext->m_pPrevious = pHandler->m_pPrevious;
    if (m_pHandlers == pHandler)
        m_pHandlers = m_pHandlers->m_pNext;
    if (m_pNextHandlerToBeFinished == pHandler)
        m_pNextHandlerToBeFinished = m_pNextHandlerToBeFinished->m_pNext;
    --m_handlersCount;
    pHandler->scheduleForDeletion();
    if (m_isStopping && m_handlersCount == 0)
        stopped();
}

}
