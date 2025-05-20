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

#ifndef KOURIER_TCP_SOCKET_DATA_SOURCE_H
#define KOURIER_TCP_SOCKET_DATA_SOURCE_H

#include "RingBuffer.h"


namespace Kourier
{

class TcpSocketDataSource : public DataSource
{
public:
    TcpSocketDataSource(const intptr_t &socketDescriptor) :
        m_socketDescriptor(socketDescriptor) {}
    ~TcpSocketDataSource() override = default;
    size_t dataAvailable() const override;
    size_t read(char *pBuffer, size_t count) override;

private:
    const intptr_t &m_socketDescriptor;
};

}

#endif // KOURIER_TCP_SOCKET_DATA_SOURCE_H
