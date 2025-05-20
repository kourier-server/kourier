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

#ifndef KOURIER_HTTP_REQUEST_PARSER_H
#define KOURIER_HTTP_REQUEST_PARSER_H

#include "HttpRequest.h"
#include "HttpRequestLimits.h"
#include "HttpServer.h"
#include "../Core/IOChannel.h"
#include "../Core/SimdIterator.h"
#include <memory>


namespace Kourier
{
class HttpConnectionHandler;

class HttpRequestParser
{
public:
    HttpRequestParser(IOChannel &pIOChannel,
                      std::shared_ptr<HttpRequestLimits> pHttpRequestLimits);
    ~HttpRequestParser() = default;
    enum class ParserStatus {ParsedRequest, ParsedBody, Failed, NeedsMoreData};
    ParserStatus parse()
    {
        switch (m_parserState)
        {
            case ParserState::ParsingRequestLine:
                return parseRequestLine();
            case ParserState::ParsingHeaders:
                return parseHeaders();
            case ParserState::ParsingBody:
                return parseBody();
            case ParserState::ParsingChunkMetadata:
                return parseChunkMetadata();
            case ParserState::ParsingChunkData:
                return parseChunkData();
            case ParserState::ParsingTrailers:
                return parseTrailers();
            default:
                Q_UNREACHABLE();
        }
    }
    inline size_t requestSize() const {return m_requestSize;}
    inline HttpServer::ServerError error() const {return m_error;}
    const HttpRequest &request() const {return m_request;}
    size_t trailersCount() const;
    size_t trailerCount(std::string_view name) const;
    bool hasTrailer(std::string_view name) const;
    std::string_view trailer(std::string_view name, int pos = 1) const;


private:
    ParserStatus parseRequestLine();
    ParserStatus parseRequestLineMethod();
    ParserStatus parseRequestLineTarget();
    ParserStatus parseRequestLineAbsolutePath();
    ParserStatus parseRequestLineQuery();
    ParserStatus parseRequestLineHttpVersion();
    ParserStatus parseHeaders();
    ParserStatus parseBody();
    ParserStatus parseChunkMetadata();
    ParserStatus parseChunkData();
    ParserStatus parseTrailers();
    bool validateHeaderLine(SimdIterator &it,
                            size_t fieldNameStartIndex,
                            size_t fieldNameEndIndex,
                            size_t fieldValueStartIndex,
                            size_t fieldValueEndIndex);
    void setError(HttpServer::ServerError error);
    inline static bool isHexChar(const char ch)
    {
        const char upperCase = ch & 0xDF;
        return ('0' <= ch && ch <= '9') || ('A' <= upperCase && upperCase <= 'F');
    }
    inline static bool isWhitespace(const char ch)
    {
        return ('\t' == ch || ' ' == ch);
    }

private:
    IOChannel &m_ioChannel;
    std::shared_ptr<HttpRequestLimits> m_pHttpRequestLimits;
    size_t m_requestSize = 0;
    size_t m_trailersSize = 0;
    HttpRequest m_request;
    HttpServer::ServerError m_error = HttpServer::ServerError::NoError;
    enum class ParserState {ParsingRequestLine, ParsingHeaders, ParsingBody, ParsingChunkMetadata, ParsingChunkData, ParsingTrailers};
    ParserState m_parserState = ParserState::ParsingRequestLine;
    bool m_alreadyProcessedHostHeaderField = false;
    bool m_hasExpectHeader = false;
    static constexpr __m256i m_idxRowsMaskLow = (__m256i)(__v32qi){char(0x01), char(0x02), char(0x04), char(0x08), char(0x10), char(0x20), char(0x40), char(0x80), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x01), char(0x02), char(0x04), char(0x08), char(0x10), char(0x20), char(0x40), char(0x80), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00)};
    static constexpr __m256i m_rowNibble = (__m256i)(__v32qi){char(0xF0), char(0xF0), char(0xF0), char(0xF0), char(0xF0), char(0xF0), char(0xF0), char(0xF0), char(0xF0), char(0xF0), char(0xF0), char(0xF0), char(0xF0), char(0xF0), char(0xF0), char(0xF0), char(0xF0), char(0xF0), char(0xF0), char(0xF0), char(0xF0), char(0xF0), char(0xF0), char(0xF0), char(0xF0), char(0xF0), char(0xF0), char(0xF0), char(0xF0), char(0xF0), char(0xF0), char(0xF0)};
    static constexpr __m256i m_zero256Bits = (__m256i)(__v4di){0, 0, 0, 0};
    static constexpr __m256i m_urlAbsolutePathLookupTableLow = (__m256i)(__v32qi){char(0B10111000), char(0B11111100), char(0B11111000), char(0B11111000), char(0B11111100), char(0B11111000), char(0B11111100), char(0B11111100), char(0B11111100), char(0B11111100), char(0B11111100), char(0B01011100), char(0B01010100), char(0B01011100), char(0B11010100), char(0B01110100), char(0B10111000), char(0B11111100), char(0B11111000), char(0B11111000), char(0B11111100), char(0B11111100), char(0B11111100), char(0B11111100), char(0B11111100), char(0B11111100), char(0B11111100), char(0B01011100), char(0B01010100), char(0B01011100), char(0B11010100), char(0B01110100)};
    static constexpr __m256i m_urlQueryLookupTableLow = (__m256i)(__v32qi){char(0B10111000), char(0B11111100), char(0B11111000), char(0B11111000), char(0B11111100), char(0B11111000), char(0B11111100), char(0B11111100), char(0B11111100), char(0B11111100), char(0B11111100), char(0B01011100), char(0B01010100), char(0B01011100), char(0B11010100), char(0B01111100), char(0B10111000), char(0B11111100), char(0B11111000), char(0B11111000), char(0B11111100), char(0B11111000), char(0B11111100), char(0B11111100), char(0B11111100), char(0B11111100), char(0B11111100), char(0B01011100), char(0B01010100), char(0B01011100), char(0B11010100), char(0B01111100)};
    static constexpr __m256i m_fieldNameLookupTableLow = (__m256i)(__v32qi){char(0B11101000), char(0B11111100), char(0B11111000), char(0B11111100), char(0B11111100), char(0B11111100), char(0B11111100), char(0B11111100), char(0B11111000), char(0B11111000), char(0B11110100), char(0B01010100), char(0B11010000), char(0B01010100), char(0B11110100), char(0B01110000), char(0B11101000), char(0B11111100), char(0B11111000), char(0B11111100), char(0B11111100), char(0B11111100), char(0B11111100), char(0B11111100), char(0B11111000), char(0B11111000), char(0B11110100), char(0B01010100), char(0B11010000), char(0B01010100), char(0B11110100), char(0B01110000)};
    static constexpr __m256i m_htab = (__m256i)(__v32qi){char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09)};
    static constexpr __m256i m_space = (__m256i)(__v32qi){char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20)};
    static constexpr __m256i m_del = (__m256i)(__v32qi){char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F)};
    static constexpr __m256i m_minus1 = (__m256i)(__v32qi){char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1)};
};

}

#endif // KOURIER_HTTP_REQUEST_PARSER_H
