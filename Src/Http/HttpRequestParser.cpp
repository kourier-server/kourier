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

#include "HttpRequestParser.h"
#include "HttpRequestPrivate.h"
#include "HttpChunkMetadataParser.h"
#include <cctype>
#include <charconv>


namespace Kourier
{

HttpRequestParser::HttpRequestParser(IOChannel &pIOChannel,
                                     std::shared_ptr<HttpRequestLimits> pHttpRequestLimits) :
    m_ioChannel(pIOChannel),
    m_pHttpRequestLimits(pHttpRequestLimits),
    m_request(new HttpRequestPrivate(pIOChannel))
{
    assert(m_pHttpRequestLimits);
}

size_t HttpRequestParser::trailersCount() const
{
    return (m_trailersSize > 0) ? m_request.d_ptr->trailersCount() : 0;
}

size_t HttpRequestParser::trailerCount(std::string_view name) const
{
    return (m_trailersSize > 0) ? m_request.d_ptr->trailerCount(name) : 0;
}

bool HttpRequestParser::hasTrailer(std::string_view name) const
{
    return (m_trailersSize > 0) && m_request.d_ptr->hasTrailer(name);
}

std::string_view HttpRequestParser::trailer(std::string_view name, int pos) const
{
    return (m_trailersSize > 0) ? m_request.d_ptr->trailer(name, pos) : std::string_view{};
}

bool HttpRequestParser::validateHeaderLine(SimdIterator &it,
                                           size_t fieldNameStartIndex,
                                           size_t fieldNameEndIndex,
                                           size_t fieldValueStartIndex,
                                           size_t fieldValueEndIndex)
{
    // Only Host, Content-Length, Transfer-Encoding and Expect headers
    // are checked. To prevent request smuggling, requests
    // cannot have both Content-Length and Transfer-Encoding
    // headers in the same request.
    const auto fieldNameSize = fieldNameEndIndex - fieldNameStartIndex + 1;
    switch (fieldNameSize)
    {
        case 4:
            // Let's check if the field name is Host
            {
                const auto fieldName = m_ioChannel.slice(fieldNameStartIndex, fieldNameSize);
                if (Q_LIKELY(0 != strncasecmp(fieldName.data(), "Host", 4)))
                    return true;
                else
                {
                    m_alreadyProcessedHostHeaderField = !m_alreadyProcessedHostHeaderField;
                    return m_alreadyProcessedHostHeaderField;
                }
            }
        case 14:
        {
            // Let's check if the field name is Content-Length
            const auto rawData = it.nextAt(fieldNameStartIndex);
            const auto rawDataFilter = _mm256_setr_epi8(0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);
            const auto headerName = _mm256_and_si256(rawData, rawDataFilter);
            const auto lowerCaseData = _mm256_setr_epi8('c','o','n','t','e','n','t','-','l','e','n','g','t','h',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);
            const auto upperCaseData = _mm256_setr_epi8('C','O','N','T','E','N','T','-','L','E','N','G','T','H',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);
            const auto result = static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_or_si256(_mm256_cmpeq_epi8(headerName, lowerCaseData), _mm256_cmpeq_epi8(headerName, upperCaseData))));
            if (UINT32_MAX != result)
                return true;
            else
            {
                if (fieldValueEndIndex == 0 || fieldValueStartIndex == 0 || fieldValueEndIndex < fieldValueStartIndex)
                    return false;
                auto contentLengthValueSlice = m_ioChannel.slice(fieldValueStartIndex, fieldValueEndIndex - fieldValueStartIndex + 1);
                char const *pBegin = contentLengthValueSlice.data();
                char const *pEnd = contentLengthValueSlice.data() + fieldValueEndIndex - fieldValueStartIndex;
                while (pBegin < pEnd && isWhitespace(*pEnd))
                    --pEnd;
                while (pBegin < pEnd && isWhitespace(*pBegin))
                    ++pBegin;
                if (((pEnd - pBegin) < 19) && ((pEnd > pBegin) || !isWhitespace(*pBegin)))
                {
                    const auto rawData = it.nextAt(fieldValueStartIndex + (pBegin - contentLengthValueSlice.data()));
                    const auto lowerBound = _mm256_set1_epi8('0');
                    const auto upperBound = _mm256_set1_epi8('9');
                    const auto matchCount = _tzcnt_u32(static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_or_si256(_mm256_cmpgt_epi8(lowerBound, rawData), _mm256_cmpgt_epi8(rawData, upperBound)))));
                    if (matchCount >= (pEnd - pBegin + 1))
                    {
                        std::string_view lengthSlice(pBegin, pEnd - pBegin + 1);
                        size_t size = 0;
                        auto [ptr, ec] {std::from_chars(pBegin, pEnd + 1, size)};
                        if (std::errc() == ec)
                        {
                            switch (m_request.d_ptr->requestBody().bodyType())
                            {
                                case HttpRequest::BodyType::Chunked:
                                    return false;
                                case HttpRequest::BodyType::NoBody:
                                    m_request.d_ptr->requestBody().setNotChunkedBody(size);
                                case HttpRequest::BodyType::NotChunked:
                                    return m_request.d_ptr->requestBodySize() == size;
                            }
                        }
                    }
                }
            }
            return false;
        }
        case 17:
        {
            // Let's check if the field name is Transfer-Encoding
            const auto rawData = it.nextAt(fieldNameStartIndex);
            const auto rawDataFilter = _mm256_setr_epi8(0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);
            const auto headerName = _mm256_and_si256(rawData, rawDataFilter);
            const auto lowerCaseData = _mm256_setr_epi8('t','r','a','n','s','f','e','r','-','e','n','c','o','d','i','n','g',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);
            const auto upperCaseData = _mm256_setr_epi8('T','R','A','N','S','F','E','R','-','E','N','C','O','D','I','N','G',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);
            const auto result = static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_or_si256(_mm256_cmpeq_epi8(headerName, lowerCaseData), _mm256_cmpeq_epi8(headerName, upperCaseData))));
            if (UINT32_MAX != result)
                return true;
            else
            {
                /*
                    Transfer-Encoding = #t-codings
                    t-codings          = transfer-coding [ weight ]
                    transfer-coding    = token *( OWS ";" OWS transfer-parameter )
                    transfer-parameter = token BWS "=" BWS ( token / quoted-string )

                    weight = OWS ";" OWS "q=" qvalue
                    qvalue = ( "0" [ "." 0*3DIGIT ] ) / ( "1" [ "." 0*3("0") ] )

                    chunked must be the last coding and must not contain parameters or weight. Also, chunked
                    is case-insensitive.
                */
                if (fieldValueEndIndex == 0 || fieldValueStartIndex == 0 || fieldValueEndIndex < fieldValueStartIndex)
                    return false;
                auto valueSlice = m_ioChannel.slice(fieldValueStartIndex, fieldValueEndIndex - fieldValueStartIndex + 1);
                char const *pBegin = valueSlice.data();
                char const *pEnd = valueSlice.data() + fieldValueEndIndex - fieldValueStartIndex;
                while (pBegin < pEnd && isWhitespace(*pEnd))
                    --pEnd;
                while (pBegin < pEnd && isWhitespace(*pBegin))
                    ++pBegin;
                if ((pEnd - pBegin) >= 6
                    && 0 == strncasecmp(pEnd - 6, "chunked", 7)
                    && m_request.d_ptr->requestBody().bodyType() == HttpRequest::BodyType::NoBody)
                {
                    for (char const *pChar = (pEnd - 7); pChar >= pBegin; --pChar)
                    {
                        switch (*pChar)
                        {
                            case ' ':
                            case '\t':
                                continue;
                            case ',':
                                m_request.d_ptr->requestBody().setChunkedBody();
                                return true;
                            default:
                                return false;
                        }
                    }
                    m_request.d_ptr->requestBody().setChunkedBody();
                    return true;
                }
            }
            return false;
        }
        case 6:
            // Let's check if the field name is Expect
            {
                if (m_hasExpectHeader
                    || (fieldValueEndIndex - fieldValueStartIndex) < 11
                    || 0 != strncasecmp(m_ioChannel.slice(fieldNameStartIndex, fieldNameSize).data(), "Expect", 6))
                {
                    return true;
                }
                else
                {
                    auto valueSlice = m_ioChannel.slice(fieldValueStartIndex, fieldValueEndIndex - fieldValueStartIndex + 1);
                    char const *pBegin = valueSlice.data();
                    char const *pEnd = valueSlice.data() + fieldValueEndIndex - fieldValueStartIndex;
                    while (pBegin < pEnd && isWhitespace(*pEnd))
                        --pEnd;
                    while (pBegin < pEnd && isWhitespace(*pBegin))
                        ++pBegin;
                    m_hasExpectHeader = ((pEnd - pBegin) == 11 && (0 == strncasecmp(pEnd - 11, "100-continue", 12)));
                    return true;
                }
            }
            break;

        default:
            return true;
    }
    return false;
}

HttpRequestParser::ParserStatus HttpRequestParser::parseRequestLine()
{
    switch (m_request.d_ptr->requestBody().bodyType())
    {
        case HttpRequest::BodyType::NotChunked:
            m_ioChannel.skip(m_request.d_ptr->requestBody().currentBodyPartIndex() + m_request.d_ptr->requestBody().currentBodyPartSize());
            break;
        case HttpRequest::BodyType::NoBody:
            m_ioChannel.skip(m_requestSize);
            break;
        case HttpRequest::BodyType::Chunked:
            m_ioChannel.skip(m_trailersSize);
            break;
    }
    m_requestSize = 0;
    m_trailersSize = 0;
    m_request.d_ptr->clear();
    return parseRequestLineMethod();
}

HttpRequestParser::ParserStatus HttpRequestParser::parseRequestLineMethod()
{
    // Supported methods: POST, PUT, PATCH, GET, HEAD, DELETE, OPTIONS
    if (m_ioChannel.dataAvailable() >= 8)
    {
        uint64_t methodMask = 0;
        auto methodSlice = m_ioChannel.slice(0, 8);
        for (char ch : methodSlice)
        {
            if (ch != ' ')
                methodMask = (methodMask << 8) | static_cast<uint64_t>(ch);
            else
            {
                auto method = HttpRequest::Method::GET;
                switch (methodMask)
                {
                    case 0x0000000000474554: // GET
                        method = HttpRequest::Method::GET;
                        m_requestSize = 4;
                        break;
                    case 0x0000000000505554: // PUT
                        method = HttpRequest::Method::PUT;
                        m_requestSize = 4;
                        break;
                    case 0x00000000504F5354: // POST
                        method = HttpRequest::Method::POST;
                        m_requestSize = 5;
                        break;
                    case 0x0000005041544348: // PATCH
                        method = HttpRequest::Method::PATCH;
                        m_requestSize = 6;
                        break;
                    case 0x0000000048454144: // HEAD
                        method = HttpRequest::Method::HEAD;
                        m_requestSize = 5;
                        break;
                    case 0x000044454C455445: // DELETE
                        method = HttpRequest::Method::DELETE;
                        m_requestSize = 7;
                        break;
                    case 0x004F5054494F4E53: // OPTIONS
                        method = HttpRequest::Method::OPTIONS;
                        m_requestSize = 8;
                        break;
                    default:
                        setError(HttpServer::ServerError::MalformedRequest);
                        return HttpRequestParser::ParserStatus::Failed;
                }
                m_request.d_ptr->requestLine().setMethod(method);
                return parseRequestLineTarget();
            }
        }
        setError(HttpServer::ServerError::MalformedRequest);
        return HttpRequestParser::ParserStatus::Failed;
    }
    return HttpRequestParser::ParserStatus::NeedsMoreData;
}

HttpRequestParser::ParserStatus HttpRequestParser::parseRequestLineTarget()
{
    //
    // As this parser is intended to be used on origin servers and not proxies, we do not
    // process HTTP CONNECT method. Thus, in our case, request-target can be either a
    // server-wide options request or an absolute-path.
    //
    // As the request-target does not contain an authority component, per section 3.3 of RFC3986,
    // absolute-path cannot begin with two slash characters.
    //
    // Per section 3.2.1 of RFC9112, the request-target of the request line has the following format.
    //
    // URL Format     = origin-form
    // origin-form    = absolute-path [ "?" query ]
    //
    // Per section 4.1 of RFC9110, the absolute-path has the following format:
    //
    // absolute-path  = 1*( "/" segment )
    //
    // Per section 3.3 of RFC3986, segment and query have the following formats:
    //
    // segment        = *pchar
    // query          = *( pchar / "/" / "?" )
    // pchar          = unreserved / pct-encoded / sub-delims / ":" / "@"
    // unreserved     = ALPHA / DIGIT / "-" / "." / "_" / "~"
    // pct-encoded    = "%" HEXDIG HEXDIG
    // sub-delims     = "!" / "$" / "&" / "'" / "(" / ")" / "*" / "+" / "," / ";" / "="
    //
    const auto &currentIndex = m_requestSize;
    if ((currentIndex + 2) <= m_ioChannel.dataAvailable())
    {
        if (m_ioChannel.peekChar(currentIndex) == '/'
            && m_ioChannel.peekChar(currentIndex + 1) != '/')
        {
            return parseRequestLineAbsolutePath();
        }
        else if (m_request.d_ptr->requestLine().method() == HttpRequest::Method::OPTIONS
                 && m_ioChannel.peekChar(currentIndex) == '*'
                 && m_ioChannel.peekChar(currentIndex + 1) == ' ')
        {
            m_request.d_ptr->requestLine().setTargetPathStartIndex(currentIndex);
            m_request.d_ptr->requestLine().setTargetPathSize(1);
            m_requestSize += 2;
            return parseRequestLineHttpVersion();
        }
        else
        {
            setError(HttpServer::ServerError::MalformedRequest);
            return HttpRequestParser::ParserStatus::Failed;
        }
    }
    m_requestSize = 0;
    return HttpRequestParser::ParserStatus::NeedsMoreData;
}

HttpRequestParser::ParserStatus HttpRequestParser::parseRequestLineAbsolutePath()
{
    auto &currentIndex = m_requestSize;
    m_request.d_ptr->requestLine().setTargetPathStartIndex(currentIndex);
    SimdIterator it(m_ioChannel);
    while (true)
    {
        const auto data = it.nextAt(currentIndex);
        const auto idxRows = _mm256_shuffle_epi8(m_idxRowsMaskLow, _mm256_srli_epi16(_mm256_and_si256(m_rowNibble, data), 4));
        const auto columnsLow = _mm256_shuffle_epi8(m_urlAbsolutePathLookupTableLow, data);
        const auto bits = _mm256_and_si256(idxRows, columnsLow);
        const auto result = _mm256_cmpeq_epi8(bits, m_zero256Bits);
        const auto matchCount = std::min<size_t>(m_ioChannel.dataAvailable() - 1 - currentIndex, _tzcnt_u32(static_cast<uint32_t>(_mm256_movemask_epi8(result))));
        currentIndex += matchCount;
        if (matchCount == 32)
            continue;
        switch (m_ioChannel.peekChar(currentIndex))
        {
            case ' ':
            {
                m_request.d_ptr->requestLine().setTargetPathSize(currentIndex - m_request.d_ptr->requestLine().targetPathStartIndex());
                ++m_requestSize;
                if (m_request.d_ptr->requestLine().targetPathSize() <= m_pHttpRequestLimits->maxUrlSize
                    && m_requestSize <= m_pHttpRequestLimits->maxRequestSize)
                {
                    return parseRequestLineHttpVersion();
                }
                else
                {
                    setError(HttpServer::ServerError::TooBigRequest);
                    return HttpRequestParser::ParserStatus::Failed;
                }
            }
            case '%':
            {
                if ((currentIndex + 3) < m_ioChannel.dataAvailable())
                {
                    if (isHexChar(m_ioChannel.peekChar(currentIndex + 1)) && isHexChar(m_ioChannel.peekChar(currentIndex + 2)))
                    {
                        currentIndex += 3;
                        continue;
                    }
                    else
                    {
                        setError(HttpServer::ServerError::MalformedRequest);
                        return HttpRequestParser::ParserStatus::Failed;
                    }
                }
                else
                {
                    m_requestSize += 3;
                    const auto urlSize = m_requestSize - m_request.d_ptr->requestLine().targetPathStartIndex();
                    if (urlSize <= m_pHttpRequestLimits->maxUrlSize && m_requestSize <= m_pHttpRequestLimits->maxRequestSize)
                    {
                        m_requestSize = 0;
                        return HttpRequestParser::ParserStatus::NeedsMoreData;
                    }
                    else
                    {
                        setError(HttpServer::ServerError::TooBigRequest);
                        return HttpRequestParser::ParserStatus::Failed;
                    }
                }
            }
            case '?':
            {
                m_request.d_ptr->requestLine().setTargetPathSize(currentIndex - m_request.d_ptr->requestLine().targetPathStartIndex());
                ++m_requestSize;
                if ((m_request.d_ptr->requestLine().targetPathSize() + 1) <= m_pHttpRequestLimits->maxUrlSize
                    && m_requestSize <= m_pHttpRequestLimits->maxRequestSize)
                {
                    return parseRequestLineQuery();
                }
                else
                {
                    setError(HttpServer::ServerError::TooBigRequest);
                    return HttpRequestParser::ParserStatus::Failed;
                }
            }
            default:
            {
                if ((currentIndex + 1) == m_ioChannel.dataAvailable())
                {
                    ++m_requestSize;
                    const auto urlSize = m_requestSize - m_request.d_ptr->requestLine().targetPathStartIndex();
                    if (urlSize <= m_pHttpRequestLimits->maxUrlSize && m_requestSize <= m_pHttpRequestLimits->maxRequestSize)
                    {
                        m_requestSize = 0;
                        return HttpRequestParser::ParserStatus::NeedsMoreData;
                    }
                    else
                    {
                        setError(HttpServer::ServerError::TooBigRequest);
                        return HttpRequestParser::ParserStatus::Failed;
                    }
                }
                else
                {
                    setError(HttpServer::ServerError::MalformedRequest);
                    return HttpRequestParser::ParserStatus::Failed;
                }
            }
        }
    }
}

HttpRequestParser::ParserStatus HttpRequestParser::parseRequestLineQuery()
{
    auto &currentIndex = m_requestSize;
    m_request.d_ptr->requestLine().setTargetQueryStartIndex(currentIndex);
    if (currentIndex >= m_ioChannel.dataAvailable())
    {
        m_requestSize = 0;
        return HttpRequestParser::ParserStatus::NeedsMoreData;
    }
    SimdIterator it(m_ioChannel);
    while (true)
    {
        const auto data = it.nextAt(currentIndex);
        const auto idxRows = _mm256_shuffle_epi8(m_idxRowsMaskLow, _mm256_srli_epi16(_mm256_and_si256(m_rowNibble, data), 4));
        const auto columnsLow = _mm256_shuffle_epi8(m_urlQueryLookupTableLow, data);
        const auto bits = _mm256_and_si256(idxRows, columnsLow);
        const auto result = _mm256_cmpeq_epi8(bits, m_zero256Bits);
        const auto matchCount = std::min<size_t>(m_ioChannel.dataAvailable() - 1 - currentIndex, _tzcnt_u32(static_cast<uint32_t>(_mm256_movemask_epi8(result))));
        currentIndex += matchCount;
        if (matchCount == 32)
            continue;
        switch (m_ioChannel.peekChar(currentIndex))
        {
            case ' ':
            {
                m_request.d_ptr->requestLine().setTargetQuerySize(currentIndex - m_request.d_ptr->requestLine().targetQueryStartIndex());
                ++m_requestSize;
                if ((m_request.d_ptr->requestLine().targetUriSize()) <= m_pHttpRequestLimits->maxUrlSize
                    && m_requestSize <= m_pHttpRequestLimits->maxRequestSize)
                {
                    return parseRequestLineHttpVersion();
                }
                else
                {
                    setError(HttpServer::ServerError::TooBigRequest);
                    return HttpRequestParser::ParserStatus::Failed;
                }
            }
            case '%':
            {
                if ((currentIndex + 3) < m_ioChannel.dataAvailable())
                {
                    if (isHexChar(m_ioChannel.peekChar(currentIndex + 1)) && isHexChar(m_ioChannel.peekChar(currentIndex + 2)))
                    {
                        currentIndex += 3;
                        continue;
                    }
                    else
                    {
                        setError(HttpServer::ServerError::MalformedRequest);
                        return HttpRequestParser::ParserStatus::Failed;
                    }
                }
                else
                {
                    m_requestSize += 3;
                    const auto urlSize = m_requestSize - m_request.d_ptr->requestLine().targetPathStartIndex();
                    if (urlSize <= m_pHttpRequestLimits->maxUrlSize && m_requestSize <= m_pHttpRequestLimits->maxRequestSize)
                    {
                        m_requestSize = 0;
                        return HttpRequestParser::ParserStatus::NeedsMoreData;
                    }
                    else
                    {
                        setError(HttpServer::ServerError::TooBigRequest);
                        return HttpRequestParser::ParserStatus::Failed;
                    }
                }
            }
            default:
            {
                if ((currentIndex + 1) == m_ioChannel.dataAvailable())
                {
                    ++m_requestSize;
                    const auto urlSize = m_requestSize - m_request.d_ptr->requestLine().targetPathStartIndex();
                    if (urlSize <= m_pHttpRequestLimits->maxUrlSize && m_requestSize <= m_pHttpRequestLimits->maxRequestSize)
                    {
                        m_requestSize = 0;
                        return HttpRequestParser::ParserStatus::NeedsMoreData;
                    }
                    else
                    {
                        setError(HttpServer::ServerError::TooBigRequest);
                        return HttpRequestParser::ParserStatus::Failed;
                    }
                }
                else
                {
                    setError(HttpServer::ServerError::MalformedRequest);
                    return HttpRequestParser::ParserStatus::Failed;
                }
            }
        }
    }
}

HttpRequestParser::ParserStatus HttpRequestParser::parseRequestLineHttpVersion()
{
    const auto &currentIndex = m_requestSize;
    if ((m_requestSize + 10) <= m_ioChannel.dataAvailable())
    {
        if (m_ioChannel.slice(currentIndex, 10) == "HTTP/1.1\r\n")
        {
            m_requestSize += 10;
            if (m_requestSize <= m_pHttpRequestLimits->maxRequestSize)
            {
                m_parserState = ParserState::ParsingHeaders;
                m_request.d_ptr->fieldBlock().reset(currentIndex);
                m_request.d_ptr->requestBody().setNoBody();
                m_alreadyProcessedHostHeaderField = false;
                m_hasExpectHeader = false;
                return parseHeaders();
            }
            else
            {
                setError(HttpServer::ServerError::TooBigRequest);
                return HttpRequestParser::ParserStatus::Failed;
            }
        }
        else
        {
            setError(HttpServer::ServerError::MalformedRequest);
            return HttpRequestParser::ParserStatus::Failed;
        }
    }
    else
    {
        m_requestSize += 10;
        if (m_requestSize <= m_pHttpRequestLimits->maxRequestSize)
        {
            m_requestSize = 0;
            return HttpRequestParser::ParserStatus::NeedsMoreData;
        }
        else
        {
            setError(HttpServer::ServerError::TooBigRequest);
            return HttpRequestParser::ParserStatus::Failed;
        }
    }
}

HttpRequestParser::ParserStatus HttpRequestParser::parseHeaders()
{
    //
    // Per section 3.2 of RFC9112 servers must reject all requests without
    // a Host header field.
    //
    // This parser does not accept line folding on field values.
    //
    // header-block   = *( field-line CRLF )CRLF (RFC9112, section 2.1)
    // field-line     = field-name ":" OWS field-value OWS (RFC9112, section 5)
    // field-name     = token (RFC9110, section 5.1)
    // token          = 1*tchar
    // tchar          = "!" / "#" / "$" / "%" / "&" / "'" / "*"
    //                  "+" / "-" / "." / "^" / "_" / "`" / "|" / "~"
    //                  DIGIT / ALPHA
    // field-value    = *field-content (RFC9110, section 5.5)
    // field-content  = field-vchar[ 1*( SP / HTAB / field-vchar ) field-vchar ] (RFC9110, section 5.5)
    // field-vchar    = VCHAR / obs-text (RFC9110, section 5.5)
    // obs-text       = %x80-FF (RFC9110, section 5.5)
    //
    auto currentIndex = m_requestSize;
    if (currentIndex >= m_ioChannel.dataAvailable())
        return HttpRequestParser::ParserStatus::NeedsMoreData;
    SimdIterator it(m_ioChannel);
    const auto maxAllowedFieldLines = std::min(HttpFieldBlock::maxFieldLines(), m_pHttpRequestLimits->maxHeaderLineCount) - m_request.d_ptr->fieldBlock().fieldLinesCount();
    const auto maxAllowedFieldNameSize = std::min(HttpFieldBlock::maxFieldNameSize(), m_pHttpRequestLimits->maxHeaderNameSize);
    const auto maxAllowedFieldValueSize = std::min(HttpFieldBlock::maxFieldValueSize(), m_pHttpRequestLimits->maxHeaderValueSize);
    for (auto i = 0; i < maxAllowedFieldLines; ++i)
    {
        const size_t fieldNameStartIndex = currentIndex;
        while (true)
        {
            const auto data = it.nextAt(currentIndex);
            const auto idxRows = _mm256_shuffle_epi8(m_idxRowsMaskLow, _mm256_srli_epi16(_mm256_and_si256(m_rowNibble, data), 4));
            const auto columnsLow = _mm256_shuffle_epi8(m_fieldNameLookupTableLow, data);
            const auto bits = _mm256_and_si256(idxRows, columnsLow);
            const auto result = _mm256_cmpeq_epi8(bits, m_zero256Bits);
            const auto matchCount = std::min<size_t>(m_ioChannel.dataAvailable() - 1 - currentIndex, _tzcnt_u32(static_cast<uint32_t>(_mm256_movemask_epi8(result))));
            currentIndex += matchCount;
            if (matchCount == 32)
                continue;
            if (m_ioChannel.peekChar(currentIndex) == ':')
            {
                if ((currentIndex > fieldNameStartIndex) && ((currentIndex - fieldNameStartIndex) <= maxAllowedFieldNameSize))
                    break;
                else
                {
                    setError((currentIndex > fieldNameStartIndex) ? HttpServer::ServerError::TooBigRequest : HttpServer::ServerError::MalformedRequest);
                    return HttpRequestParser::ParserStatus::Failed;
                }
            }
            else if ((currentIndex + 1) == m_ioChannel.dataAvailable())
            {
                if (((currentIndex - fieldNameStartIndex + 1) <= maxAllowedFieldNameSize)
                    && ((currentIndex + 1) <= m_pHttpRequestLimits->maxRequestSize))
                    return HttpRequestParser::ParserStatus::NeedsMoreData;
                else
                {
                    setError(HttpServer::ServerError::TooBigRequest);
                    return HttpRequestParser::ParserStatus::Failed;
                }
            }
            else
            {
                setError(HttpServer::ServerError::MalformedRequest);
                return HttpRequestParser::ParserStatus::Failed;
            }
        }
        if ((currentIndex + 5) > m_ioChannel.dataAvailable())
            return HttpRequestParser::ParserStatus::NeedsMoreData;
        const size_t fieldNameEndIndex = currentIndex - 1;
        const size_t fieldValueStartIndex = ++currentIndex;
        while (true)
        {
            const auto data = it.nextAt(currentIndex);
            const auto result = _mm256_or_si256(_mm256_cmpeq_epi8(m_del, data), _mm256_andnot_si256(_mm256_cmpeq_epi8(m_htab, data), _mm256_and_si256(_mm256_cmpgt_epi8(data, m_minus1), _mm256_cmpgt_epi8(m_space, data))));
            const auto matchCount = std::min<size_t>(m_ioChannel.dataAvailable() - 4 - currentIndex, _tzcnt_u32(static_cast<uint32_t>(_mm256_movemask_epi8(result))));
            currentIndex += matchCount;
            if (matchCount == 32)
                continue;
            if (m_ioChannel.slice(currentIndex, 2) == "\r\n")
            {
                if (((currentIndex - fieldValueStartIndex) <= maxAllowedFieldValueSize)
                    && ((currentIndex + 2) <= m_pHttpRequestLimits->maxRequestSize))
                    break;
                else
                {
                    setError(HttpServer::ServerError::TooBigRequest);
                    return HttpRequestParser::ParserStatus::Failed;
                }
            }
            else if ((currentIndex + 4) == m_ioChannel.dataAvailable())
            {
                if (((currentIndex - fieldValueStartIndex) <= maxAllowedFieldValueSize)
                    && ((currentIndex + 4) <= m_pHttpRequestLimits->maxRequestSize))
                    return HttpRequestParser::ParserStatus::NeedsMoreData;
                else
                {
                    setError(HttpServer::ServerError::TooBigRequest);
                    return HttpRequestParser::ParserStatus::Failed;
                }
            }
            else
            {
                setError(HttpServer::ServerError::MalformedRequest);
                return HttpRequestParser::ParserStatus::Failed;
            }
        }
        const size_t fieldValueEndIndex = currentIndex - 1;
        currentIndex += 2;
        m_requestSize = currentIndex;
        if (!validateHeaderLine(it, fieldNameStartIndex, fieldNameEndIndex, fieldValueStartIndex, fieldValueEndIndex))
        {
            setError(HttpServer::ServerError::MalformedRequest);
            return HttpRequestParser::ParserStatus::Failed;
        }
        m_request.d_ptr->fieldBlock().addFieldLine(fieldNameStartIndex, fieldNameEndIndex, fieldValueStartIndex, fieldValueEndIndex);
        if (m_ioChannel.slice(currentIndex, 2) == "\r\n")
        {
            if (m_alreadyProcessedHostHeaderField)
            {
                m_requestSize = currentIndex + 2;
                if (m_hasExpectHeader)
                    m_ioChannel.write("HTTP/1.1 100 Continue\r\n\r\n");
                switch (m_request.d_ptr->requestBody().bodyType())
                {
                    case HttpRequest::BodyType::NotChunked:
                        if ((m_request.d_ptr->requestBody().requestBodySize() <= m_pHttpRequestLimits->maxBodySize)
                            && ((m_requestSize + m_request.d_ptr->requestBody().requestBodySize()) <= m_pHttpRequestLimits->maxRequestSize))
                        {
                            if ((m_requestSize + m_request.d_ptr->requestBody().requestBodySize()) <= m_ioChannel.dataAvailable())
                            {
                                m_request.d_ptr->requestBody().setCurrentBodyPart(m_requestSize, m_request.d_ptr->requestBody().requestBodySize());
                                m_requestSize += m_request.d_ptr->requestBody().requestBodySize();
                                m_parserState = ParserState::ParsingRequestLine;
                            }
                            else
                            {
                                m_request.d_ptr->requestBody().setCurrentBodyPart(m_requestSize, m_ioChannel.dataAvailable() - m_requestSize);
                                m_requestSize += (m_ioChannel.dataAvailable() - m_requestSize);
                                m_parserState = ParserState::ParsingBody;
                            }
                        }
                        else
                        {
                            setError(HttpServer::ServerError::TooBigRequest);
                            return HttpRequestParser::ParserStatus::Failed;
                        }
                        break;
                    case HttpRequest::BodyType::Chunked:
                        m_request.d_ptr->requestBody().setCurrentBodyPart(m_requestSize, 0);
                        m_parserState = ParserState::ParsingChunkMetadata;
                        break;
                    case HttpRequest::BodyType::NoBody:
                        m_parserState = ParserState::ParsingRequestLine;
                        break;
                }
                return HttpRequestParser::ParserStatus::ParsedRequest;
            }
            else
            {
                setError(HttpServer::ServerError::MalformedRequest);
                return HttpRequestParser::ParserStatus::Failed;
            }
        }
        else
            continue;
    }
    setError(HttpServer::ServerError::TooBigRequest);
    return HttpRequestParser::ParserStatus::Failed;
}

HttpRequestParser::ParserStatus HttpRequestParser::parseBody()
{
    m_ioChannel.skip(m_request.d_ptr->requestBody().currentBodyPartIndex() + m_request.d_ptr->requestBody().currentBodyPartSize());
    if (m_request.d_ptr->requestBody().pendingBodySize() <= m_ioChannel.dataAvailable())
    {
        m_requestSize += m_request.d_ptr->requestBody().pendingBodySize();
        m_request.d_ptr->requestBody().setCurrentBodyPart(0, m_request.d_ptr->requestBody().pendingBodySize());
        m_parserState = ParserState::ParsingRequestLine;
    }
    else
    {
        m_requestSize += m_ioChannel.dataAvailable();
        m_request.d_ptr->requestBody().setCurrentBodyPart(0, m_ioChannel.dataAvailable());
    }
    return (m_request.d_ptr->requestBody().currentBodyPartSize() > 0) ? HttpRequestParser::ParserStatus::ParsedBody : HttpRequestParser::ParserStatus::NeedsMoreData;
}

HttpRequestParser::ParserStatus HttpRequestParser::parseChunkMetadata()
{
    // chunk-metadata = chunk-size [ chunk-ext ] CRLF
    // chunk-size     = 1*HEXDIG
    // chunk-ext      = *( BWS ";" BWS chunk-ext-name[ BWS "=" BWS chunk-ext-val ] )
    // chunk-ext-name = token
    // chunk-ext-val  = token / quoted-string
    // quoted-string  = DQUOTE *( qdtext / quoted-pair ) DQUOTE
    // qdtext         = HTAB / SP / %x21 / %x23-5B / %x5D-7E / obs-text
    // quoted-pair    = "\" ( HTAB / SP / VCHAR / obs-text )
    m_ioChannel.skip(m_request.d_ptr->requestBody().currentBodyPartIndex() + ((m_request.d_ptr->requestBody().currentBodyPartSize() > 0) ? (m_request.d_ptr->requestBody().currentBodyPartSize() + 2) : 0));
    m_request.d_ptr->requestBody().setCurrentBodyPart(0, 0);
    m_request.d_ptr->fieldBlock().reset(0);
    size_t chunkDataSize = 0;
    size_t chunkMetadataSize = 0;
    switch (HttpChunkMetadataParser::parse(m_ioChannel, chunkDataSize, chunkMetadataSize))
    {
        case HttpChunkMetadataParser::ChunkMetadataParserStatus::ExpectingChunkData:
            m_ioChannel.skip(chunkMetadataSize);
            m_parserState = ParserState::ParsingChunkData;
            m_request.d_ptr->requestBody().setChunkDataSize(chunkDataSize);
            m_requestSize += chunkMetadataSize;
            m_trailersSize = 0;
            if ((m_requestSize + chunkDataSize + 2) <= m_pHttpRequestLimits->maxRequestSize
                && (m_request.d_ptr->requestBodySize() + m_request.d_ptr->pendingBodySize()) <= m_pHttpRequestLimits->maxBodySize
                && chunkMetadataSize <= m_pHttpRequestLimits->maxChunkMetadataSize)
                return parseChunkData();
            else
            {
                setError(HttpServer::ServerError::TooBigRequest);
                return HttpRequestParser::ParserStatus::Failed;
            }
        case HttpChunkMetadataParser::ChunkMetadataParserStatus::ParsedRequest:
            m_ioChannel.skip(chunkMetadataSize);
            m_parserState = ParserState::ParsingRequestLine;
            m_requestSize += chunkMetadataSize;
            m_trailersSize = 0;
            if (m_requestSize <= m_pHttpRequestLimits->maxRequestSize
                && chunkMetadataSize <= m_pHttpRequestLimits->maxChunkMetadataSize)
                return HttpRequestParser::ParserStatus::ParsedRequest;
            else
            {
                setError(HttpServer::ServerError::TooBigRequest);
                return HttpRequestParser::ParserStatus::Failed;
            }
        case HttpChunkMetadataParser::ChunkMetadataParserStatus::ExpectingTrailer:
            m_ioChannel.skip(chunkMetadataSize);
            m_parserState = ParserState::ParsingTrailers;
            m_requestSize += chunkMetadataSize;
            m_trailersSize = 0;
            if (m_requestSize <= m_pHttpRequestLimits->maxRequestSize
                && chunkMetadataSize <= m_pHttpRequestLimits->maxChunkMetadataSize)
                return parseTrailers();
            else
            {
                setError(HttpServer::ServerError::TooBigRequest);
                return HttpRequestParser::ParserStatus::Failed;
            }
        case HttpChunkMetadataParser::ChunkMetadataParserStatus::NeedsMoreData:
            if ((m_requestSize + m_ioChannel.dataAvailable()) <= m_pHttpRequestLimits->maxRequestSize
                && m_ioChannel.dataAvailable() <= m_pHttpRequestLimits->maxChunkMetadataSize)
                return HttpRequestParser::ParserStatus::NeedsMoreData;
            else
            {
                setError(HttpServer::ServerError::TooBigRequest);
                return HttpRequestParser::ParserStatus::Failed;
            }
        case HttpChunkMetadataParser::ChunkMetadataParserStatus::Failed:
            setError(HttpServer::ServerError::MalformedRequest);
            return HttpRequestParser::ParserStatus::Failed;
        default:
            Q_UNREACHABLE();
    }
}

HttpRequestParser::ParserStatus HttpRequestParser::parseChunkData()
{
    m_ioChannel.skip(m_request.d_ptr->requestBody().currentBodyPartIndex() + m_request.d_ptr->requestBody().currentBodyPartSize());
    m_request.d_ptr->requestBody().setCurrentBodyPart(0, 0);
    if (m_ioChannel.dataAvailable() < 3)
        return HttpRequestParser::ParserStatus::NeedsMoreData;
    if ((m_request.d_ptr->requestBody().pendingBodySize() + 2) <= m_ioChannel.dataAvailable())
    {
        m_requestSize += m_request.d_ptr->requestBody().pendingBodySize() + 2;
        m_request.d_ptr->requestBody().setCurrentBodyPart(0, m_request.d_ptr->requestBody().pendingBodySize());
        if (m_ioChannel.slice(m_request.d_ptr->requestBody().currentBodyPartSize(), 2) == "\r\n")
            m_parserState = ParserState::ParsingChunkMetadata;
        else
        {
            setError(HttpServer::ServerError::MalformedRequest);
            return HttpRequestParser::ParserStatus::Failed;
        }
    }
    else
    {
        m_requestSize += m_ioChannel.dataAvailable() - 2;
        m_request.d_ptr->requestBody().setCurrentBodyPart(0, m_ioChannel.dataAvailable() - 2);
    }
    return (m_request.d_ptr->requestBody().currentBodyPartSize() > 0) ? HttpRequestParser::ParserStatus::ParsedBody : HttpRequestParser::ParserStatus::NeedsMoreData;
}

HttpRequestParser::ParserStatus HttpRequestParser::parseTrailers()
{
    size_t currentIndex = m_trailersSize;
    if (currentIndex >= m_ioChannel.dataAvailable())
        return HttpRequestParser::ParserStatus::NeedsMoreData;
    SimdIterator it(m_ioChannel);
    const auto maxAllowedFieldLines = std::min(HttpFieldBlock::maxFieldLines(), m_pHttpRequestLimits->maxTrailerLineCount) - m_request.d_ptr->fieldBlock().fieldLinesCount();
    const auto maxAllowedFieldNameSize = std::min(HttpFieldBlock::maxFieldNameSize(), m_pHttpRequestLimits->maxTrailerNameSize);
    const auto maxAllowedFieldValueSize = std::min(HttpFieldBlock::maxFieldValueSize(), m_pHttpRequestLimits->maxTrailerValueSize);
    for (auto i = 0; i < maxAllowedFieldLines; ++i)
    {
        const size_t fieldNameStartIndex = currentIndex;
        while (true)
        {
            const auto data = it.nextAt(currentIndex);
            const auto idxRows = _mm256_shuffle_epi8(m_idxRowsMaskLow, _mm256_srli_epi16(_mm256_and_si256(m_rowNibble, data), 4));
            const auto columnsLow = _mm256_shuffle_epi8(m_fieldNameLookupTableLow, data);
            const auto bits = _mm256_and_si256(idxRows, columnsLow);
            const auto result = _mm256_cmpeq_epi8(bits, m_zero256Bits);
            const auto matchCount = std::min<size_t>(m_ioChannel.dataAvailable() - 1 - currentIndex, _tzcnt_u32(static_cast<uint32_t>(_mm256_movemask_epi8(result))));
            currentIndex += matchCount;
            if (matchCount == 32)
                continue;
            if (m_ioChannel.peekChar(currentIndex) == ':')
            {
                if ((currentIndex > fieldNameStartIndex) && ((currentIndex - fieldNameStartIndex) <= maxAllowedFieldNameSize))
                    break;
                else
                {
                    setError((currentIndex > fieldNameStartIndex) ? HttpServer::ServerError::TooBigRequest : HttpServer::ServerError::MalformedRequest);
                    return HttpRequestParser::ParserStatus::Failed;
                }
            }
            else if ((currentIndex + 1) == m_ioChannel.dataAvailable())
            {
                if (((currentIndex - fieldNameStartIndex + 1) <= maxAllowedFieldNameSize)
                    && ((m_requestSize + currentIndex + 1) <= m_pHttpRequestLimits->maxRequestSize))
                    return HttpRequestParser::ParserStatus::NeedsMoreData;
                else
                {
                    setError(HttpServer::ServerError::TooBigRequest);
                    return HttpRequestParser::ParserStatus::Failed;
                }
            }
            else
            {
                setError(HttpServer::ServerError::MalformedRequest);
                return HttpRequestParser::ParserStatus::Failed;
            }
        }
        if ((currentIndex + 5) > m_ioChannel.dataAvailable())
            return HttpRequestParser::ParserStatus::NeedsMoreData;
        const size_t fieldNameEndIndex = currentIndex - 1;
        const size_t fieldValueStartIndex = ++currentIndex;
        while (true)
        {
            const auto data = it.nextAt(currentIndex);
            const auto result = _mm256_or_si256(_mm256_cmpeq_epi8(m_del, data), _mm256_andnot_si256(_mm256_cmpeq_epi8(m_htab, data), _mm256_and_si256(_mm256_cmpgt_epi8(data, m_minus1), _mm256_cmpgt_epi8(m_space, data))));
            const auto matchCount = std::min<size_t>(m_ioChannel.dataAvailable() - 4 - currentIndex, _tzcnt_u32(static_cast<uint32_t>(_mm256_movemask_epi8(result))));
            currentIndex += matchCount;
            if (matchCount == 32)
                continue;
            if (m_ioChannel.slice(currentIndex, 2) == "\r\n")
            {
                if (((currentIndex - fieldValueStartIndex) <= maxAllowedFieldValueSize)
                    && ((m_requestSize + currentIndex + 2) <= m_pHttpRequestLimits->maxRequestSize))
                    break;
                else
                {
                    setError(HttpServer::ServerError::TooBigRequest);
                    return HttpRequestParser::ParserStatus::Failed;
                }
            }
            else if ((currentIndex + 4) == m_ioChannel.dataAvailable())
            {
                if (((currentIndex - fieldValueStartIndex + 1) <= maxAllowedFieldValueSize)
                    && ((m_requestSize + currentIndex + 4) <= m_pHttpRequestLimits->maxRequestSize))
                    return HttpRequestParser::ParserStatus::NeedsMoreData;
                else
                {
                    setError(HttpServer::ServerError::TooBigRequest);
                    return HttpRequestParser::ParserStatus::Failed;
                }
            }
            else
            {
                setError(HttpServer::ServerError::MalformedRequest);
                return HttpRequestParser::ParserStatus::Failed;
            }
        }
        const size_t fieldValueEndIndex = currentIndex - 1;
        currentIndex += 2;
        m_trailersSize = currentIndex;
        m_request.d_ptr->fieldBlock().addFieldLine(fieldNameStartIndex, fieldNameEndIndex, fieldValueStartIndex, fieldValueEndIndex);
        if (m_ioChannel.slice(currentIndex, 2) == "\r\n")
        {
            m_trailersSize += 2;
            m_requestSize += m_trailersSize;
            if (m_requestSize <= m_pHttpRequestLimits->maxRequestSize)
            {
                m_parserState = ParserState::ParsingRequestLine;
                return HttpRequestParser::ParserStatus::ParsedRequest;
            }
            else
            {
                setError(HttpServer::ServerError::TooBigRequest);
                return HttpRequestParser::ParserStatus::Failed;
            }
        }
        else
            continue;
    }
    setError(HttpServer::ServerError::TooBigRequest);
    return HttpRequestParser::ParserStatus::Failed;
}

void HttpRequestParser::setError(HttpServer::ServerError error)
{
    m_parserState = ParserState::ParsingRequestLine;
    m_error = error;
}

}
