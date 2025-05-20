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

#ifndef KOURIER_HTTP_REQUEST_PRIVATE_H
#define KOURIER_HTTP_REQUEST_PRIVATE_H

#include "HttpRequestLine.h"
#include "HttpFieldBlock.h"
#include "HttpRequestBody.h"
#include "../Core/TcpSocket.h"
#include <type_traits>
#include <cstddef>


namespace Kourier
{

class HttpRequestPrivate
{
public:
    HttpRequestPrivate(IOChannel &ioChannel) : m_pIoChannel(&ioChannel), m_fieldBlock(ioChannel) {}
    ~HttpRequestPrivate() = default;
    inline void clear(){std::memset(&m_requestLine, 0, offsetof(HttpRequestPrivate, m_fieldBlock) - offsetof(HttpRequestPrivate, m_requestLine));}
    inline const HttpRequestLine &requestLine() const {return m_requestLine;}
    inline HttpRequestLine &requestLine() {return m_requestLine;}
    inline const HttpFieldBlock &fieldBlock() const {return m_fieldBlock;}
    inline HttpFieldBlock &fieldBlock() {return m_fieldBlock;}
    inline const HttpRequestBody &requestBody() const {return m_requestBody;}
    inline HttpRequestBody &requestBody() {return m_requestBody;}
    HttpRequest::Method method() const {return m_requestLine.method();}
    std::string_view targetPath() const {return m_requestLine.targetPathSize() > 0 ? m_pIoChannel->slice(m_requestLine.targetPathStartIndex(), m_requestLine.targetPathSize()) : std::string_view{};}
    std::string_view targetQuery() const {return m_requestLine.targetQuerySize() > 0 ? m_pIoChannel->slice(m_requestLine.targetQueryStartIndex(), m_requestLine.targetQuerySize()) : std::string_view{};}
    size_t headersCount() const {return m_fieldBlock.fieldLinesCount();}
    size_t headerCount(std::string_view name) const {return m_fieldBlock.fieldCount(name);}
    bool hasHeader(std::string_view name) const {return m_fieldBlock.hasField(name);}
    std::string_view header(std::string_view name, int pos = 1) const {return m_fieldBlock.fieldValue(name, pos);}
    size_t trailersCount() const {return m_fieldBlock.fieldLinesCount();}
    size_t trailerCount(std::string_view name) const {return m_fieldBlock.fieldCount(name);}
    bool hasTrailer(std::string_view name) const {return m_fieldBlock.hasField(name);}
    std::string_view trailer(std::string_view name, int pos = 1) const {return m_fieldBlock.fieldValue(name, pos);}
    inline bool chunked() const {return m_requestBody.chunked();}
    inline HttpRequest::BodyType bodyType() const {return m_requestBody.bodyType();}
    inline size_t requestBodySize() const {return m_requestBody.requestBodySize();}
    inline size_t pendingBodySize() const {return m_requestBody.pendingBodySize();}
    inline bool hasBody() const {return m_requestBody.currentBodyPartSize() > 0;}
    inline bool isComplete() const
    {
        switch (m_requestBody.bodyType())
        {
            case HttpRequest::BodyType::NoBody:
                return true;
            case HttpRequest::BodyType::NotChunked:
                return m_requestBody.pendingBodySize() == 0;
            case HttpRequest::BodyType::Chunked:
                return false;
        }
    }
    inline std::string_view body() const {return (hasBody() && m_requestBody.currentBodyPartSize() > 0) ? m_pIoChannel->slice(m_requestBody.currentBodyPartIndex(), m_requestBody.currentBodyPartSize()) : std::string_view{};}
    inline std::string_view peerAddress() const
    {
        auto *pSocket = m_pIoChannel->tryCast<TcpSocket*>();
        return pSocket ? pSocket->peerAddress() : std::string_view{};
    }
    inline uint16_t peerPort() const
    {
        auto *pSocket = m_pIoChannel->tryCast<TcpSocket*>();
        return pSocket ? pSocket->peerPort() : 0;
    }

private:
    IOChannel * const m_pIoChannel;
    HttpRequestLine m_requestLine;
    HttpRequestBody m_requestBody;
    HttpFieldBlock m_fieldBlock;
};

static_assert(std::is_standard_layout_v<HttpRequestPrivate>);
}

#endif // KOURIER_HTTP_REQUEST_PRIVATE_H
