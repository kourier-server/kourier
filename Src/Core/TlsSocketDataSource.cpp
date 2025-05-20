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

#include "TlsSocketDataSource.h"
#include "RuntimeError.h"
#include <openssl/err.h>


namespace Kourier
{

size_t TlsSocketDataSource::read(char *pBuffer, size_t count)
{
    if (pBuffer == nullptr || count == 0)
        return 0;
    int bytesDecrypted = 0;
    while (true)
    {
        const auto result = SSL_read(m_pSSL, pBuffer + bytesDecrypted, count - bytesDecrypted);
        if (result > 0)
        {
            bytesDecrypted += result;
            if (bytesDecrypted < count)
                continue;
            else
                return bytesDecrypted;
        }
        else
        {
            switch (SSL_get_error(m_pSSL, result))
            {
                case SSL_ERROR_WANT_READ:
                case SSL_ERROR_ZERO_RETURN:
                    return bytesDecrypted;
                case SSL_ERROR_SYSCALL:
                case SSL_ERROR_SSL:
                    throw RuntimeError("Failed to decrypt data.", RuntimeError::ErrorType::TLS);
                default:
                    return bytesDecrypted;
            }
        }
    }
}

}
