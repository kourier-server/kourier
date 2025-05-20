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

#ifndef KOURIER_HTTP_REQUEST_BODY_H
#define KOURIER_HTTP_REQUEST_BODY_H

#include "HttpRequest.h"


namespace Kourier
{

class HttpRequestBody
{
public:
    inline void setChunkedBody()
    {
        m_bodyType = HttpRequest::BodyType::Chunked;
        m_requestBodySize = 0;
        m_pendingBodySize = 0;
        m_currentBodyPartIndex = 0;
        m_currentBodyPartSize = 0;
    }
    inline void setNotChunkedBody(size_t requestBodySize)
    {
        m_bodyType = HttpRequest::BodyType::NotChunked;
        m_requestBodySize = requestBodySize;
        m_pendingBodySize = requestBodySize;
        m_currentBodyPartIndex = 0;
        m_currentBodyPartSize = 0;
    }
    inline void setNoBody()
    {
        m_bodyType = HttpRequest::BodyType::NoBody;
        m_requestBodySize = 0;
        m_pendingBodySize = 0;
        m_currentBodyPartIndex = 0;
        m_currentBodyPartSize = 0;
    }
    inline void setChunkDataSize(size_t size)
    {
        m_pendingBodySize = size;
        m_currentBodyPartIndex = 0;
        m_currentBodyPartSize = 0;
    }
    inline bool chunked() const {return m_bodyType == HttpRequest::BodyType::Chunked;}
    inline HttpRequest::BodyType bodyType() const {return m_bodyType;}
    inline size_t requestBodySize() const {return m_requestBodySize;}
    inline size_t pendingBodySize() const {return m_pendingBodySize;}
    inline size_t currentBodyPartIndex() const {return m_currentBodyPartIndex;}
    inline size_t currentBodyPartSize() const {return m_currentBodyPartSize;}
    inline void setCurrentBodyPart(size_t currentBodyPartStartIndex, size_t currentBodyPartSize)
    {
        m_currentBodyPartIndex = currentBodyPartStartIndex;
        m_currentBodyPartSize = currentBodyPartSize;
        if (chunked())
            m_requestBodySize += currentBodyPartSize;
        m_pendingBodySize -= currentBodyPartSize;
    }

private:
    size_t m_requestBodySize = 0;
    size_t m_pendingBodySize = 0;
    size_t m_currentBodyPartIndex = 0;
    size_t m_currentBodyPartSize = 0;
    HttpRequest::BodyType m_bodyType = HttpRequest::BodyType::NoBody;
};

}

#endif // KOURIER_HTTP_REQUEST_BODY_H
