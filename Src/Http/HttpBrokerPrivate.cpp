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

#include "HttpBrokerPrivate.h"
#include "HttpRequestParser.h"
#include "../Core/TcpSocket.h"
#include "../Core/NoDestroy.h"
#include "../Core/Timer.h"
#include <string>
#include <QDateTime>
#include <charconv>
#include <cstdio>


namespace Kourier
{

HttpBrokerPrivate::HttpBrokerPrivate(IOChannel *pIOChannel,
    HttpRequestParser *pRequestParser) :
    m_pIOChannel(pIOChannel),
    m_pRequestParser(pRequestParser)
{
    assert(m_pIOChannel && m_pRequestParser);
    Object::connect(m_pIOChannel, &IOChannel::sentData, this, &HttpBrokerPrivate::onSentData);
}

HttpBrokerPrivate::~HttpBrokerPrivate()
{
    if (m_pObject)
        m_pObject->deleteLater();
}

void HttpBrokerPrivate::writeChunk(std::string_view data)
{
    if (m_isWritingChunkedResponse && !data.empty())
    {
        writeChunkMetadata(data.size());
        m_pIOChannel->write(data);
        m_pIOChannel->write("\r\n");
    }
}

size_t HttpBrokerPrivate::bytesToSend() const
{
    return m_pIOChannel->dataToWrite();
}

size_t HttpBrokerPrivate::trailersCount() const
{
    return m_pRequestParser->trailersCount();
}

size_t HttpBrokerPrivate::trailerCount(std::string_view name) const
{
    return m_pRequestParser->trailerCount(name);
}

bool HttpBrokerPrivate::hasTrailer(std::string_view name) const
{
    return m_pRequestParser->hasTrailer(name);
}

std::string_view HttpBrokerPrivate::trailer(std::string_view name, int pos) const
{
    return m_pRequestParser->trailer(name, pos);
}

Signal HttpBrokerPrivate::wroteResponse() KOURIER_SIGNAL(&HttpBrokerPrivate::wroteResponse)

void HttpBrokerPrivate::setQObject(QObject *pObject)
{
    if (m_pObject)
        m_pObject->deleteLater();
    m_pObject = pObject;
}

void HttpBrokerPrivate::onSentData(size_t count)
{
    if (m_pBroker)
        emit m_pBroker->sentData(count);
}

void HttpBrokerPrivate::writeStatusLine(HttpStatusCode statusCode)
{
    struct StatusLines
    {
        std::string_view statusLines[44] = {
            "HTTP/1.1 100 Continue\r\n",
            "HTTP/1.1 101 Switching Protocols\r\n",
            "HTTP/1.1 200 OK\r\n",
            "HTTP/1.1 201 Created\r\n",
            "HTTP/1.1 202 Accepted\r\n",
            "HTTP/1.1 203 Non-Authoritative Information\r\n",
            "HTTP/1.1 204 No Content\r\n",
            "HTTP/1.1 205 Reset Content\r\n",
            "HTTP/1.1 206 Partial Content\r\n",
            "HTTP/1.1 300 Multiple Choices\r\n",
            "HTTP/1.1 301 Moved Permanently\r\n",
            "HTTP/1.1 302 Found\r\n",
            "HTTP/1.1 303 See Other\r\n",
            "HTTP/1.1 304 Not Modified\r\n",
            "HTTP/1.1 305 Use Proxy\r\n",
            "HTTP/1.1 307 Temporary Redirect\r\n",
            "HTTP/1.1 308 Permanent Redirect\r\n",
            "HTTP/1.1 400 Bad Request\r\n",
            "HTTP/1.1 401 Unauthorized\r\n",
            "HTTP/1.1 402 Payment Required\r\n",
            "HTTP/1.1 403 Forbidden\r\n",
            "HTTP/1.1 404 Not Found\r\n",
            "HTTP/1.1 405 Method Not Allowed\r\n",
            "HTTP/1.1 406 Not Acceptable\r\n",
            "HTTP/1.1 407 Proxy Authentication Required\r\n",
            "HTTP/1.1 408 Request Timeout\r\n",
            "HTTP/1.1 409 Conflict\r\n",
            "HTTP/1.1 410 Gone\r\n",
            "HTTP/1.1 411 Length Required\r\n",
            "HTTP/1.1 412 Precondition Failed\r\n",
            "HTTP/1.1 413 Content Too Large\r\n",
            "HTTP/1.1 414 URI Too Long\r\n",
            "HTTP/1.1 415 Unsupported Media Type\r\n",
            "HTTP/1.1 416 Range Not Satisfiable\r\n",
            "HTTP/1.1 417 Expectation Failed\r\n",
            "HTTP/1.1 421 Misdirected Request\r\n",
            "HTTP/1.1 422 Unprocessable Content\r\n",
            "HTTP/1.1 426 Upgrade Required\r\n",
            "HTTP/1.1 500 Internal Server Error\r\n",
            "HTTP/1.1 501 Not Implemented\r\n",
            "HTTP/1.1 502 Bad Gateway\r\n",
            "HTTP/1.1 503 Service Unavailable\r\n",
            "HTTP/1.1 504 Gateway Timeout\r\n",
            "HTTP/1.1 505 HTTP Version Not Supported\r\n",
        };
    };
    static constinit NoDestroy<StatusLines> statusLines;
    m_pIOChannel->write(statusLines().statusLines[(size_t)statusCode]);
}

void HttpBrokerPrivate::writeContentLengthHeader(size_t size)
{
    static_assert(std::numeric_limits<size_t>::max() == 18446744073709551615ull);
    char buffer[20];
    std::to_chars_result result = std::to_chars(buffer, buffer + 20, size);
    assert(result.ec == std::errc());
    m_pIOChannel->write("Content-Length: ");
    m_pIOChannel->write(buffer, result.ptr - buffer);
    m_pIOChannel->write("\r\n");
}

void HttpBrokerPrivate::writeChunkMetadata(size_t size)
{
    static_assert(sizeof(size_t) == 8);
    char buffer[16];
    std::to_chars_result result = std::to_chars(buffer, buffer + 16, size, 16);
    assert(result.ec == std::errc());
    m_pIOChannel->write(buffer, result.ptr - buffer);
    m_pIOChannel->write("\r\n");
}

void HttpBrokerPrivate::writeDateHeader()
{
    // RFC9110 5.6.7. Date/Time Formats
    // IMF-fixdate  = day-name "," SP date1 SP time-of-day SP GMT
    // ; fixed length/zone/capitalization subset of the format
    // ; see Section 3.3 of [RFC5322]
    // day-name     = %s"Mon" / %s"Tue" / %s"Wed"
    // / %s"Thu" / %s"Fri" / %s"Sat" / %s"Sun"
    // date1        = day SP month SP year
    // ; e.g., 02 Jun 1982
    // day          = 2DIGIT
    // month        = %s"Jan" / %s"Feb" / %s"Mar" / %s"Apr"
    // / %s"May" / %s"Jun" / %s"Jul" / %s"Aug"
    // / %s"Sep" / %s"Oct" / %s"Nov" / %s"Dec"
    // year         = 4DIGIT
    // GMT          = %s"GMT"
    // time-of-day  = hour ":" minute ":" second
    // ; 00:00:00 - 23:59:60 (leap second)
    // hour         = 2DIGIT
    // minute       = 2DIGIT
    // second       = 2DIGIT
    static thread_local NoDestroy<std::string*> dateTimeUtc;
    static thread_local NoDestroyPtrDeleter<std::string*> dateTimeUtcDeleter(dateTimeUtc);
    static constinit thread_local NoDestroy<bool> firstRun = true;
    static thread_local NoDestroy<Timer*> dateTimeUpdater;
    static thread_local NoDestroyPtrDeleter<Timer*> dateTimeUpdaterDeleter(dateTimeUpdater);
    if (firstRun())
    {
        firstRun() = false;
        dateTimeUtc() = new std::string;
        *dateTimeUtc() = getCurrentDate();
        dateTimeUpdater() = new Timer;
        dateTimeUpdater()->setSingleShot(false);
        Object::connect(dateTimeUpdater(), &Timer::timeout, []()
        {
            if (dateTimeUtc() != nullptr)
                *dateTimeUtc() = getCurrentDate();
        });
        dateTimeUpdater()->start(1000);
    }
    if (dateTimeUtc() != nullptr && !dateTimeUtc()->empty())
    {
        m_pIOChannel->write("Date: ");
        m_pIOChannel->write(*dateTimeUtc());
        m_pIOChannel->write("\r\n");
    }
    else
    {
        m_pIOChannel->write("Date: ");
        m_pIOChannel->write(getCurrentDate());
        m_pIOChannel->write("\r\n");
    }
}

void HttpBrokerPrivate::writeServerHeader()
{
    m_pIOChannel->write("Server: Kourier\r\n");
}

void HttpBrokerPrivate::finishWritingChunkedResponse()
{
    m_pIOChannel->write("0\r\n\r\n");
    finishResponseWritingAndEmitWroteResponse();
}

std::string HttpBrokerPrivate::getCurrentDate()
{
    QString date;
    date.reserve(30);
    const auto dateTimeUtc = QDateTime::currentDateTimeUtc();
    const auto dateUtc = dateTimeUtc.date();
    const auto timeUtc = dateTimeUtc.time();
    const auto weekDay = dateUtc.dayOfWeek();
    if (1 <= weekDay && weekDay <= 7 && dateUtc.isValid() && timeUtc.isValid())
    {
        std::string_view weekDayStrs[7] = {"Mon, ", "Tue, ", "Wed, ", "Thu, ", "Fri, ", "Sat, ", "Sun, "};
        date.append(weekDayStrs[weekDay - 1])
            .append(dateUtc.toString(Qt::RFC2822Date))
            .append(" ")
            .append(timeUtc.toString(Qt::RFC2822Date))
            .append(" GMT");
    }
    return date.toStdString();
}

void HttpBrokerPrivate::finishResponseWritingAndEmitWroteResponse()
{
    m_isWritingChunkedResponse = false;
    m_wroteResponse = true;
    if (m_hasWrittenCloseConnectionHeader)
    {
        auto *pSocket = m_pIOChannel->tryCast<TcpSocket*>();
        if (pSocket)
            pSocket->disconnectFromPeer();
    }
    wroteResponse();
}

}
