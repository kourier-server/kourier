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

#ifndef KOURIER_HTTP_SERVER_OPTIONS_H
#define KOURIER_HTTP_SERVER_OPTIONS_H

#include "HttpServer.h"
#include <string>
#include <string_view>
#include <map>


namespace Kourier
{

class HttpServerOptions
{
public:
    bool setOption(HttpServer::ServerOption option, int64_t value);
    int64_t getOption(HttpServer::ServerOption option) const;
    inline std::string_view errorMessage() const {return m_errorMessage;}
    static int64_t defaultOptionValue(HttpServer::ServerOption option);
    static int64_t maxOptionValue(HttpServer::ServerOption option);

private:
    std::map<HttpServer::ServerOption, int64_t> m_options;
    std::string m_errorMessage;
};

}

#endif // KOURIER_HTTP_SERVER_OPTIONS_H
