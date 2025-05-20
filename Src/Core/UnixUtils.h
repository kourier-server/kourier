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

#ifndef KOURIER_UNIX_UTILS_H
#define KOURIER_UNIX_UTILS_H

#include <QtGlobal>


namespace Kourier
{

struct UnixUtils
{
    static void safeClose(qint64 fd);
    static size_t safeRead(intptr_t fd, char *pBuffer, size_t count);
    static size_t safeReceive(intptr_t fd, char *pBuffer, size_t count);
    static size_t safeWrite(intptr_t fd, const char *pData, size_t count);
    static size_t safeSend(intptr_t fd, const char *pData, size_t count);

private:
    UnixUtils() = delete;
    ~UnixUtils() = delete;
};

}

#endif // KOURIER_UNIX_UTILS_H
