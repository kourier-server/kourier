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

#ifndef KOURIER_HTTP_REQUEST_ROUTER_H
#define KOURIER_HTTP_REQUEST_ROUTER_H

#include "HttpRequest.h"
#include <vector>
#include <string>
#include <string_view>


namespace Kourier
{
class HttpBroker;

class HttpRequestRouter
{
public:
    HttpRequestRouter() = default;
    HttpRequestRouter(const HttpRequestRouter&) = default;
    HttpRequestRouter &operator=(const HttpRequestRouter&) = default;
    ~HttpRequestRouter() = default;
    typedef void(*RequestHandler)(const HttpRequest &, HttpBroker&);
    bool addRoute(HttpRequest::Method method, std::string_view path, RequestHandler pRequestHandler);
    inline std::string_view errorMessage() const {return m_errorMessage;}
    RequestHandler getHandler(HttpRequest::Method method, std::string_view path) const;

private:
    bool isAbsolutePath(std::string_view path);

private:
    struct HandlerInfo
    {
        HttpRequest::Method method = HttpRequest::Method::GET;
        std::string path;
        RequestHandler pHandler = nullptr;
    };
    std::vector<HandlerInfo> m_handlers[7];
    std::string m_errorMessage;
};

}

#endif // KOURIER_HTTP_REQUEST_ROUTER_H
