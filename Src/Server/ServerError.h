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

#ifndef KOURIER_SERVER_ERROR_H
#define KOURIER_SERVER_ERROR_H

#include <string_view>


namespace Kourier
{

enum class ServerError
{
    AcceptError,
    TlsError,
    RequestTimeout,
    IdleTimeout,
    MalformedRequest,
    InvalidRequest
};


class ServerErrorHandler
{
public:
    virtual void handleError(ServerError errorCode,
                             std::string_view errorMessage,
                             std::string_view clientIp);
};

}

#endif // KOURIER_SERVER_H
