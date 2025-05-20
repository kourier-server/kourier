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

#ifndef KOURIER_HTTP_BROKER_H
#define KOURIER_HTTP_BROKER_H

#include "../Core/SDK.h"
#include <initializer_list>
#include <utility>
#include <string>
#include <string_view>
#include <QObject>
#include <initializer_list>
#include <utility>
#include <string>
#include <vector>


namespace Test::HttpRequestRouter {class TestHttpRequestRouter;}

namespace Kourier
{

class HttpBrokerPrivate;

class KOURIER_EXPORT HttpBroker : public QObject
{
Q_OBJECT
public:
    HttpBroker() = delete;
    ~HttpBroker() override = default;
    enum class HttpStatusCode
    {
        Continue = 0, // 100,
        SwitchingProtocols, // = 101,
        OK, // = 200,
        Created, // = 201,
        Accepted, // = 202,
        NonAuthoritativeInformation, // = 203,
        NoContent, // = 204,
        ResetContent, // = 205,
        PartialContent, // = 206,
        MultipleChoices, // = 300,
        MovedPermanently, // = 301,
        Found, // = 302,
        SeeOther, // = 303,
        NotModified, // = 304,
        UseProxy, // = 305,
        TemporaryRedirect, // = 307,
        PermanentRedirect, // = 308,
        BadRequest, // = 400,
        Unauthorized, // = 401,
        PaymentRequired, // = 402,
        Forbidden, // = 403,
        NotFound, // = 404,
        MethodNotAllowed, // = 405,
        NotAcceptable, // = 406,
        ProxyAuthenticationRequired, // = 407,
        RequestTimeout, // = 408,
        Conflict, // = 409,
        Gone, // = 410,
        LengthRequired, // = 411,
        PreconditionFailed, // = 412,
        ContentTooLarge, // = 413,
        URITooLong, // = 414,
        UnsupportedMediaType, // = 415,
        RangeNotSatisfiable, // = 416,
        ExpectationFailed, // = 417,
        MisdirectedRequest, // = 421,
        UnprocessableContent, // = 422,
        UpgradeRequired, // = 426,
        InternalServerError, // = 500,
        NotImplemented, // = 501,
        BadGateway, // = 502,
        ServiceUnavailable, // = 503,
        GatewayTimeout, // = 504,
        HTTPVersionNotSupported, // = 505
    };
    void closeConnectionAfterResponding();
    void writeResponse(HttpStatusCode statusCode = HttpStatusCode::OK,
                       std::initializer_list<std::pair<std::string, std::string>> headers = {});
    void writeResponse(HttpStatusCode statusCode,
                       const std::vector<std::pair<std::string, std::string>> &headers);
    void writeResponse(std::string_view body,
                       HttpStatusCode statusCode = HttpStatusCode::OK,
                       std::initializer_list<std::pair<std::string, std::string>> headers = {});
    void writeResponse(std::string_view body,
                       HttpStatusCode statusCode,
                       const std::vector<std::pair<std::string, std::string>> &headers);
    void writeResponse(std::string_view body,
                       std::string_view mimeType,
                       HttpStatusCode statusCode = HttpStatusCode::OK,
                       std::initializer_list<std::pair<std::string, std::string>> headers = {});
    void writeResponse(std::string_view body,
                       std::string_view mimeType,
                       HttpStatusCode statusCode,
                       const std::vector<std::pair<std::string, std::string>> &headers);
    void writeChunkedResponse(HttpStatusCode statusCode = HttpStatusCode::OK,
                              std::initializer_list<std::pair<std::string, std::string>> headers = {},
                              std::initializer_list<std::string> expectedTrailerNames = {});
    void writeChunkedResponse(HttpStatusCode statusCode,
                              const std::vector<std::pair<std::string, std::string>> &headers,
                              const std::vector<std::string> &expectedTrailerNames);
    void writeChunkedResponse(std::string_view mimeType,
                              HttpStatusCode statusCode = HttpStatusCode::OK,
                              std::initializer_list<std::pair<std::string, std::string>> headers = {},
                              std::initializer_list<std::string> expectedTrailerNames = {});
    void writeChunkedResponse(std::string_view mimeType,
                              HttpStatusCode statusCode,
                              const std::vector<std::pair<std::string, std::string>> &headers,
                              const std::vector<std::string> &expectedTrailerNames);
    void writeChunk(std::string_view data);
    void writeLastChunk(std::initializer_list<std::pair<std::string, std::string>> trailers = {});
    void writeLastChunk(const std::vector<std::pair<std::string, std::string>> &trailers);
    size_t bytesToSend() const;
    bool hasTrailers() const;
    size_t trailersCount() const;
    size_t trailerCount(std::string_view name) const;
    bool hasTrailer(std::string_view name) const;
    std::string_view trailer(std::string_view name, int pos = 1) const;
    void setQObject(QObject *pObject);

signals:
    void sentData(size_t count);
    void receivedBodyData(std::string_view data, bool isLastPart);

private:
    HttpBroker(HttpBrokerPrivate *pBrokerPrivate);
    void connectNotify(const QMetaMethod &signal) override;

private:
    HttpBrokerPrivate *d_ptr;
    Q_DECLARE_PRIVATE(HttpBroker)
    Q_DISABLE_COPY_MOVE(HttpBroker)
    friend class HttpConnectionHandler;
    friend class Test::HttpRequestRouter::TestHttpRequestRouter;
};

using HttpStatusCode = HttpBroker::HttpStatusCode;

}

#endif // KOURIER_HTTP_BROKER_H
