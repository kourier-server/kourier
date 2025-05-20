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

#include "TlsError.h"
#include <openssl/err.h>


namespace Kourier
{

TlsError TlsError::getError()
{
    TlsError error;
    error.m_errorCode = ERR_get_error();
    const char * pLibErrorString = ERR_lib_error_string(error.m_errorCode);
    if (pLibErrorString)
        error.m_errorLibName = std::string_view(pLibErrorString);
    const char * pReasonErrorString = ERR_reason_error_string(error.m_errorCode);
    if (pReasonErrorString)
        error.m_errorReason = std::string_view(pReasonErrorString);
    return error;
}

}
