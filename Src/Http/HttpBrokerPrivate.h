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

#ifndef KOURIER_HTTP_BROKER_PRIVATE_H
#define KOURIER_HTTP_BROKER_PRIVATE_H

#include "HttpBroker.h"
#include "../Core/IOChannel.h"
#include "../Core/Object.h"
#include <QObject>
#include <initializer_list>
#include <utility>
#include <string>
#include <vector>

namespace Test::HttpBrokerPrivate {class TestHttpBrokerPrivate;}

namespace Kourier
{

class IOChannel;
class HttpRequestParser;

class HttpBrokerPrivate : public Object
{
KOURIER_OBJECT(Kourier::HttpBrokerPrivate);
public:
    HttpBrokerPrivate(IOChannel *pIOChannel,
                      HttpRequestParser *pRequestParser);
    ~HttpBrokerPrivate() override;
    inline void closeConnectionAfterResponding() {m_closeAfterResponding = true;}
    inline void writeResponse(HttpStatusCode statusCode = HttpStatusCode::OK,
        std::initializer_list<std::pair<std::string, std::string>> headers = {}) {doWriteResponse({}, {}, statusCode, headers.begin(), headers.end());}
    inline void writeResponse(HttpStatusCode statusCode,
        const std::vector<std::pair<std::string, std::string>> &headers) {doWriteResponse({}, {}, statusCode, headers.begin(), headers.end());}
    inline void writeResponse(std::string_view body,
        HttpStatusCode statusCode = HttpStatusCode::OK,
        std::initializer_list<std::pair<std::string, std::string>> headers = {}) {doWriteResponse(body, {}, statusCode, headers.begin(), headers.end());}
    inline void writeResponse(std::string_view body,
        HttpStatusCode statusCode,
        const std::vector<std::pair<std::string, std::string>> &headers) {doWriteResponse(body, {}, statusCode, headers.begin(), headers.end());}
    inline void writeResponse(std::string_view body,
        std::string_view mimeType,
        HttpStatusCode statusCode = HttpStatusCode::OK,
        std::initializer_list<std::pair<std::string, std::string>> headers = {}) {doWriteResponse(body, mimeType, statusCode, headers.begin(), headers.end());}
    inline void writeResponse(std::string_view body,
        std::string_view mimeType,
        HttpStatusCode statusCode,
        const std::vector<std::pair<std::string, std::string>> &headers) {doWriteResponse(body, mimeType, statusCode, headers.begin(), headers.end());}
    inline void writeChunkedResponse(HttpStatusCode statusCode = HttpStatusCode::OK,
        std::initializer_list<std::pair<std::string, std::string>> headers = {},
        std::initializer_list<std::string> expectedTrailerNames = {}) {doWriteChunkedResponse({}, statusCode, headers.begin(), headers.end(), expectedTrailerNames.begin(), expectedTrailerNames.end());}
    inline void writeChunkedResponse(HttpStatusCode statusCode,
        const std::vector<std::pair<std::string, std::string>> &headers,
        const std::vector<std::string> &expectedTrailerNames) {doWriteChunkedResponse({}, statusCode, headers.begin(), headers.end(), expectedTrailerNames.begin(), expectedTrailerNames.end());}
    inline void writeChunkedResponse(std::string_view mimeType,
        HttpStatusCode statusCode = HttpStatusCode::OK,
        std::initializer_list<std::pair<std::string, std::string>> headers = {},
        std::initializer_list<std::string> expectedTrailerNames = {}) {doWriteChunkedResponse(mimeType, statusCode, headers.begin(), headers.end(), expectedTrailerNames.begin(), expectedTrailerNames.end());}
    inline void writeChunkedResponse(std::string_view mimeType,
        HttpStatusCode statusCode,
        const std::vector<std::pair<std::string, std::string>> &headers,
        const std::vector<std::string> &expectedTrailerNames) {doWriteChunkedResponse(mimeType, statusCode, headers.begin(), headers.end(), expectedTrailerNames.begin(), expectedTrailerNames.end());}
    void writeChunk(std::string_view data);
    void writeLastChunk(std::initializer_list<std::pair<std::string, std::string>> trailers = {}) {doWriteLastChunk(trailers.begin(), trailers.end());}
    void writeLastChunk(const std::vector<std::pair<std::string, std::string>> &trailers) {doWriteLastChunk(trailers.begin(), trailers.end());}
    size_t bytesToSend() const;
    bool hasTrailers() const {return trailersCount() > 0;}
    size_t trailersCount() const;
    size_t trailerCount(std::string_view name) const;
    bool hasTrailer(std::string_view name) const;
    std::string_view trailer(std::string_view name, int pos = 1) const;
    Signal wroteResponse();
    void setQObject(QObject *pObject);
    inline bool hasQObject() const {return m_pObject != nullptr;}
    inline void setConnected(bool connected) {m_isConnected = connected;}
    inline void resetResponseWriting()
    {
        if (!m_hasWrittenCloseConnectionHeader)
            m_wroteResponse = false;
        if (m_pBroker && m_isConnected)
            m_pBroker->disconnect();
        m_isConnected = false;
        if (m_pObject)
            m_pObject->deleteLater();
        m_pObject = nullptr;
    }
    inline bool responded() const {return m_wroteResponse;}
    inline void setBroker(HttpBroker *pBroker) {m_pBroker = pBroker;}

private:
    void onSentData(size_t count);
    void writeStatusLine(HttpStatusCode statusCode);
    void writeContentLengthHeader(size_t size);
    void writeChunkMetadata(size_t size);
    void writeDateHeader();
    inline void writeCloseConnectionHeaderIfNecessary()
    {
        if (m_closeAfterResponding)
        {
            m_hasWrittenCloseConnectionHeader = true;
            m_pIOChannel->write("Connection: close\r\n");
        }
    }
    void writeServerHeader();
    void finishWritingChunkedResponse();
    static std::string getCurrentDate();
    void finishResponseWritingAndEmitWroteResponse();

private:
    template<class ItType>
    inline void doWriteResponse(std::string_view body,
        std::string_view mimeType,
        HttpStatusCode statusCode,
        const ItType itBegin,
        const ItType itEnd)
    {
        if (m_wroteResponse)
            return;
        if (m_isWritingChunkedResponse)
        {
            finishWritingChunkedResponse();
            return;
        }
        writeStatusLine(statusCode);
        writeServerHeader();
        writeDateHeader();
        writeCloseConnectionHeaderIfNecessary();
        if (!body.empty())
            writeContentLengthHeader(body.size());
        else
            m_pIOChannel->write("Content-Length: 0\r\n");
        if (!mimeType.empty())
        {
            m_pIOChannel->write("Content-Type: ");
            m_pIOChannel->write(mimeType);
            m_pIOChannel->write("\r\n");
        }
        for (auto it = itBegin; it != itEnd; ++it)
        {
            m_pIOChannel->write(it->first);
            m_pIOChannel->write(": ");
            m_pIOChannel->write(it->second);
            m_pIOChannel->write("\r\n");
        }
        m_pIOChannel->write("\r\n");
        if (!body.empty())
            m_pIOChannel->write(body);
        finishResponseWritingAndEmitWroteResponse();
    }

    template<class ItHeaderType, class ItTrailerType>
    inline void doWriteChunkedResponse(std::string_view mimeType,
        HttpStatusCode statusCode,
        const ItHeaderType itHeaderBegin,
        const ItHeaderType itHeaderEnd,
        const ItTrailerType itTrailerBegin,
        const ItTrailerType itTrailerEnd)
    {
        if (m_wroteResponse)
            return;
        if (m_isWritingChunkedResponse)
        {
            finishWritingChunkedResponse();
            return;
        }
        m_isWritingChunkedResponse = true;
        writeStatusLine(statusCode);
        writeServerHeader();
        writeDateHeader();
        writeCloseConnectionHeaderIfNecessary();
        if (!mimeType.empty())
        {
            m_pIOChannel->write("Content-Type: ");
            m_pIOChannel->write(mimeType);
            m_pIOChannel->write("\r\n");
        }
        m_pIOChannel->write("Transfer-Encoding: chunked\r\n");
        if (itTrailerBegin != itTrailerEnd)
        {
            m_pIOChannel->write("Trailer: ");
            m_pIOChannel->write(*itTrailerBegin);
            auto it = itTrailerBegin;
            while (++it != itTrailerEnd)
            {
                m_pIOChannel->write(", ");
                m_pIOChannel->write(*it);
            }
            m_pIOChannel->write("\r\n");
        }
        for (auto it = itHeaderBegin; it != itHeaderEnd; ++it)
        {
            m_pIOChannel->write(it->first);
            m_pIOChannel->write(": ");
            m_pIOChannel->write(it->second);
            m_pIOChannel->write("\r\n");
        }
        m_pIOChannel->write("\r\n");
    }

    template<class ItTrailerType>
    inline void doWriteLastChunk(const ItTrailerType itTrailerBegin, const ItTrailerType itTrailerEnd)
    {
        if (!m_isWritingChunkedResponse)
            return;
        m_isWritingChunkedResponse = false;
        m_pIOChannel->write("0\r\n");
        for (auto it = itTrailerBegin; it != itTrailerEnd; ++it)
        {
            m_pIOChannel->write(it->first);
            m_pIOChannel->write(": ");
            m_pIOChannel->write(it->second);
            m_pIOChannel->write("\r\n");
        }
        m_pIOChannel->write("\r\n");
        finishResponseWritingAndEmitWroteResponse();
    }

private:
    IOChannel * const m_pIOChannel;
    HttpRequestParser * const m_pRequestParser;
    HttpBroker *m_pBroker = nullptr;
    QObject *m_pObject = nullptr;
    bool m_isWritingChunkedResponse = false;
    bool m_wroteResponse = false;
    bool m_closeAfterResponding = false;
    bool m_hasWrittenCloseConnectionHeader = false;
    bool m_isConnected = false;
    friend class HttpConnectionHandler;
    friend class Test::HttpBrokerPrivate::TestHttpBrokerPrivate;
};

}

#endif // KOURIER_HTTP_BROKER_PRIVATE_H
