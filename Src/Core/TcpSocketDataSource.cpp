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

#include "TcpSocketDataSource.h"
#include "UnixUtils.h"
#include <sys/ioctl.h>


namespace Kourier
{

size_t TcpSocketDataSource::dataAvailable() const
{
    int byteCount = 0;
    ::ioctl(m_socketDescriptor, FIONREAD, &byteCount);
    return byteCount;
}

size_t TcpSocketDataSource::read(char *pBuffer, size_t count)
{
    return UnixUtils::safeReceive(m_socketDescriptor, pBuffer, count);
}

}
