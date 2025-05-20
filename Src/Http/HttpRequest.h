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

#ifndef KOURIER_HTTP_REQUEST_H
#define KOURIER_HTTP_REQUEST_H

#include "../Core/SDK.h"
#include <string_view>
#include <cstddef>


namespace Kourier
{
class HttpRequestPrivate;

class KOURIER_EXPORT HttpRequest
{
public:
    HttpRequest() = delete;
    ~HttpRequest();
    enum class Method {GET = 0, PUT, POST, PATCH, DELETE, HEAD, OPTIONS};
    Method method() const;
    std::string_view targetPath() const;
    std::string_view targetQuery() const;
    size_t headersCount() const;
    size_t headerCount(std::string_view name) const;
    bool hasHeader(std::string_view name) const;
    std::string_view header(std::string_view name, int pos = 1) const;
    bool chunked() const;
    bool isComplete() const;
    enum class BodyType {NoBody = 0, NotChunked, Chunked};
    BodyType bodyType() const;
    size_t requestBodySize() const;
    size_t pendingBodySize() const;
    bool hasBody() const;
    std::string_view body() const;
    std::string_view peerAddress() const;
    uint16_t peerPort() const;

private:
    HttpRequest(HttpRequestPrivate *pHttpRequestPrivate);

private:
    HttpRequestPrivate *d_ptr;
    Q_DECLARE_PRIVATE(HttpRequest)
    Q_DISABLE_COPY_MOVE(HttpRequest)
    friend class HttpRequestParser;
};

}

#endif // KOURIER_HTTP_REQUEST_H
