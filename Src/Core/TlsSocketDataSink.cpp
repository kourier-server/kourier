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

#include "TlsSocketDataSink.h"
#include "RuntimeError.h"
#include <openssl/err.h>


namespace Kourier
{

bool TlsSocketDataSink::needsToRead() const
{
    return (1 == SSL_want_read(m_pSSL));
}

size_t TlsSocketDataSink::write(const char *pData, size_t count)
{
    if (pData == nullptr || count == 0)
        return 0;
    const auto result = SSL_write(m_pSSL, pData, count);
    if (result > 0)
    {
        assert(result == count);
        return result;
    }
    else
    {
        switch (SSL_get_error(m_pSSL, result))
        {
            case SSL_ERROR_WANT_READ:
                return 0;
            case SSL_ERROR_SYSCALL:
            case SSL_ERROR_SSL:
                throw RuntimeError("Failed to encrypt data.", RuntimeError::ErrorType::TLS);
            default:
                return 0;
        }
    }
}

}
