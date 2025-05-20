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

#ifndef KOURIER_ERROR_HANDLER_H
#define KOURIER_ERROR_HANDLER_H

#include "HttpServer.h"
#include <string_view>


namespace Kourier
{

/*!
\class Kourier::ErrorHandler
\brief The ErrorHandler class defines the interface for HttpServer error handlers.

Although HTTP supports communication of errors in responses, some errors cannot be handled this way.

For example, a request may not be valid HTTP or, even if it is valid, no handler may be mapped to
the request's method/path. Also, the request may be too big, or a timeout may occur while waiting
for requests or parsing them.
Error handlers exist to allow you to take action when such events occur.
You can call HttpServer::setErrorHandler to set the error handler for a HttpServer instance.

*/

class KOURIER_EXPORT ErrorHandler
{
public:
    ErrorHandler() = default;
    virtual ~ErrorHandler() = default;
    /*!
    HttpServer calls handleError when an error occurs while processing the HTTP request.
    HttpServer calls the error handler set by you when it processes an invalid HTTP request,
    when the request is valid but has no handler mapped to its method/path, or when a timeout
    occurs while waiting for a request or while parsing one.

    You can use \a error to know which type of error happened. The client IP/port is given by
    \a clientIP and \a clientPort, respectively. HttpServer does not serialize access to
    this method. Thus, it is your responsibility to provide a thread-safe implementation for it.
     */
    virtual void handleError(HttpServer::ServerError error, std::string_view clientIp, uint16_t clientPort) = 0;
};

}

#endif // KOURIER_ERROR_HANDLER_H
