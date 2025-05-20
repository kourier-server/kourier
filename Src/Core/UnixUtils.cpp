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

#include "UnixUtils.h"
#include <sys/socket.h>
#include <unistd.h>


namespace Kourier
{

void UnixUtils::safeClose(qint64 fd)
{
    if (fd >= 0)
    {
        int result;
        do
        {
            result = ::close(fd);
        }
        while (-1 == result && EINTR == errno);
    }
}

size_t UnixUtils::safeRead(int64_t fd, char *pBuffer, size_t count)
{
    assert(fd >= 0 && pBuffer != nullptr);
    size_t bytesRead = 0;
    ssize_t result = 0;
    while (bytesRead < count)
    {
        result = ::read(fd, pBuffer + bytesRead, count - bytesRead);
        if (result > 0)
            bytesRead += result;
        else
        {
            if (-1 == result && errno == EINTR)
                continue;
            else
                return bytesRead;
        }
    }
    return bytesRead;
}

size_t UnixUtils::safeReceive(intptr_t fd, char *pBuffer, size_t count)
{
    assert(fd >= 0 && pBuffer != nullptr);
    size_t bytesRead = 0;
    ssize_t result = 0;
    while (bytesRead < count)
    {
        result = ::recv(fd, pBuffer + bytesRead, count - bytesRead, 0);
        if (result > 0)
            bytesRead += result;
        else
        {
            if (-1 == result && errno == EINTR)
                continue;
            else
                return bytesRead;
        }
    }
    return bytesRead;
}

size_t UnixUtils::safeWrite(int64_t fd, const char *pData, size_t count)
{
    assert(fd >= 0 && pData != nullptr);
    size_t bytesWritten = 0;
    ssize_t result = 0;
    while (bytesWritten < count)
    {
        result = ::write(fd, pData + bytesWritten, count - bytesWritten);
        if (result > 0)
            bytesWritten += result;
        else
        {
            if (-1 == result && errno == EINTR)
                continue;
            else
                return bytesWritten;
        }
    }
    return bytesWritten;
}

size_t UnixUtils::safeSend(intptr_t fd, const char *pData, size_t count)
{
    assert(fd >= 0 && pData != nullptr);
    size_t bytesWritten = 0;
    ssize_t result = 0;
    while (bytesWritten < count)
    {
        result = ::send(fd, pData + bytesWritten, count - bytesWritten, 0);
        if (result > 0)
            bytesWritten += result;
        else
        {
            if (-1 == result && errno == EINTR)
                continue;
            else
                return bytesWritten;
        }
    }
    return bytesWritten;
}

}
