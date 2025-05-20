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

#include "HttpBroker.h"
#include "HttpBrokerPrivate.h"

/// @brief Kourier
namespace Kourier
{

/*!
\class Kourier::HttpBroker
\brief The HttpBroker class acts as a broker for HTTP-based communication.
You can use it to receive the remaining request body and to write the HTTP response.

HttpBroker cannot be created by you.
It is created by HttpServer and passed as an argument to the mapped handler.
You can call HttpServer::addRoute to map handlers to HTTP methods and paths.

HttpServer calls the mapped handler right after it parses the request header block.
If the request has a body that is not chunked, the server processes
all body data available when the header block was fully parsed. You can use
the HttpBroker instance that the server passes as an argument to the mapped handler to write the response
and to receive any pending body data for the request.

Unlike the HttpRequest argument, which you can only use inside the handler, the HttpBroker argument
can be used until you finish writing the response. For example, you may request a NoSQL database and
only write the response when you receive the queried data. However, to use the HttpBroker argument
outside the handler function, you must call setQObject() with a valid object responsible for processing
the remaining body data and writing the HTTP response. HttpServer closes the connection if the called
handler neither writes a complete response nor sets an object to write it later after the handler returns.
*/

/*!
\enum HttpBroker::HttpStatusCode
\brief This enum describes the status for HTTP responses.
\var HttpBroker::HttpStatusCode::Continue
\brief 100 Continue
\var HttpBroker::HttpStatusCode::SwitchingProtocols
\brief 101 SwitchingProtocols
\var HttpBroker::HttpStatusCode::OK
\brief 200 OK
\var HttpBroker::HttpStatusCode::Created
\brief 201 Created
\var HttpBroker::HttpStatusCode::Accepted
\brief 202 Accepted
\var HttpBroker::HttpStatusCode::NonAuthoritativeInformation
\brief 203 Non Authoritative Information
\var HttpBroker::HttpStatusCode::NoContent
\brief 204 No Content
\var HttpBroker::HttpStatusCode::ResetContent
\brief 205 Reset Content
\var HttpBroker::HttpStatusCode::PartialContent
\brief 206 Partial Content
\var HttpBroker::HttpStatusCode::MultipleChoices
\brief 300 Multiple Choices
\var HttpBroker::HttpStatusCode::MovedPermanently
\brief 301 Moved Permanently
\var HttpBroker::HttpStatusCode::Found
\brief 302 Found
\var HttpBroker::HttpStatusCode::SeeOther
\brief 303 See Other
\var HttpBroker::HttpStatusCode::NotModified
\brief 304 Not Modified
\var HttpBroker::HttpStatusCode::UseProxy
\brief 305 Use Proxy
\var HttpBroker::HttpStatusCode::TemporaryRedirect
\brief 307 Temporary Redirect
\var HttpBroker::HttpStatusCode::PermanentRedirect
\brief 308 Permanent Redirect
\var HttpBroker::HttpStatusCode::BadRequest
\brief 400 Bad Request
\var HttpBroker::HttpStatusCode::Unauthorized
\brief 401 Unauthorized
\var HttpBroker::HttpStatusCode::PaymentRequired
\brief 402 Payment Required
\var HttpBroker::HttpStatusCode::Forbidden
\brief 403 Forbidden
\var HttpBroker::HttpStatusCode::NotFound
\brief 404 Not Found
\var HttpBroker::HttpStatusCode::MethodNotAllowed
\brief 405 Method Not Allowed
\var HttpBroker::HttpStatusCode::NotAcceptable
\brief 406 Not Acceptable
\var HttpBroker::HttpStatusCode::ProxyAuthenticationRequired
\brief 407 Proxy Authentication Required
\var HttpBroker::HttpStatusCode::RequestTimeout
\brief 408 Request Timeout
\var HttpBroker::HttpStatusCode::Conflict
\brief 409 Conflict
\var HttpBroker::HttpStatusCode::Gone
\brief 410 Gone
\var HttpBroker::HttpStatusCode::LengthRequired
\brief 411 Length Required
\var HttpBroker::HttpStatusCode::PreconditionFailed
\brief 412 Precondition Failed
\var HttpBroker::HttpStatusCode::ContentTooLarge
\brief 413 Content Too Large
\var HttpBroker::HttpStatusCode::URITooLong
\brief 414 URI Too Long
\var HttpBroker::HttpStatusCode::UnsupportedMediaType
\brief 415 Unsupported Media Type
\var HttpBroker::HttpStatusCode::RangeNotSatisfiable
\brief 416 Range Not Satisfiable
\var HttpBroker::HttpStatusCode::ExpectationFailed
\brief 417 Expectation Failed
\var HttpBroker::HttpStatusCode::MisdirectedRequest
\brief 421 Misdirected Request
\var HttpBroker::HttpStatusCode::UnprocessableContent
\brief 422 Unprocessable Content
\var HttpBroker::HttpStatusCode::UpgradeRequired
\brief 426 Upgrade Required
\var HttpBroker::HttpStatusCode::InternalServerError
\brief 500 Internal Server Error
\var HttpBroker::HttpStatusCode::NotImplemented
\brief 501 Not Implemented
\var HttpBroker::HttpStatusCode::BadGateway
\brief 502 Bad Gateway
\var HttpBroker::HttpStatusCode::ServiceUnavailable
\brief 503 Service Unavailable
\var HttpBroker::HttpStatusCode::GatewayTimeout
\brief 504 Gateway Timeout
\var HttpBroker::HttpStatusCode::HTTPVersionNotSupported
\brief 505 HTTP Version Not Supported
*/

/*!
\fn HttpBroker::~HttpBroker()
Destroys the object.
*/

/*!
\fn HttpBroker::closeConnectionAfterResponding()
Makes the broker write the <em>Connection: close</em> field line on the response header block and close
the connection after sending it to the peer. This method will be applied to the next response you write.
For example, if you call this method in the middle of writing a chunked response, it will take effect on
the next written response.
*/

/*!
\fn HttpBroker::writeResponse(HttpStatusCode statusCode = HttpStatusCode::OK,
                               std::initializer_list<std::pair<std::string, std::string>> headers = {})
Writes response with \a statusCode and \a headers to the peer. HttpBroker writes the server, date, and content length
headers. HttpBroker writes the <em>Connection: close</em> field line to the header block if you called
closeConnectionAfterResponding() before calling this method.

If you call this method after writing the response, HttpBroker returns without writing another response.
If you call this method while writing a chunked response, HttpBroker finishes the current chunked response
and returns without writing another one.
*/

/*!
\fn HttpBroker::writeResponse(HttpStatusCode statusCode,
                               const std::vector<std::pair<std::string, std::string>> &headers)
Writes response with \a statusCode and \a headers to the peer. HttpBroker writes the server, date, and content length
headers. HttpBroker writes the <em>Connection: close</em> field line to the header block if you called
closeConnectionAfterResponding() before calling this method.

If you call this method after writing the response, HttpBroker returns without writing another response.
If you call this method while writing a chunked response, HttpBroker finishes the current chunked response
and returns without writing another one.
*/

/*!
\fn HttpBroker::writeResponse(std::string_view body,
                               HttpStatusCode statusCode = HttpStatusCode::OK,
                               std::initializer_list<std::pair<std::string, std::string>> headers = {})
Writes a response with \a statusCode, \a headers, and \a body to the peer. HttpBroker writes the server, date, and content-length
headers. HttpBroker writes, as the <em>Content-Length</em> field line's field value, the size of the \a body if it is not empty,
or zero otherwise.
HttpBroker writes the <em>Connection: close</em> field line to the header block if you called closeConnectionAfterResponding()
before calling this method.

If you call this method after writing the response, HttpBroker returns without writing another response.
If you call this method while writing a chunked response, HttpBroker finishes the current chunked response
and returns without writing another one.
*/

/*!
\fn HttpBroker::writeResponse(std::string_view body,
                               HttpStatusCode statusCode,
                               const std::vector<std::pair<std::string, std::string>> &headers)
Writes a response with \a statusCode, \a headers, and \a body to the peer. HttpBroker writes the server, date, and content-length
headers. HttpBroker writes, as the <em>Content-Length</em> field line's field value, the size of the \a body if it is not empty,
or zero otherwise.
HttpBroker writes the <em>Connection: close</em> field line to the header block if you called closeConnectionAfterResponding()
before calling this method.

If you call this method after writing the response, HttpBroker returns without writing another response.
If you call this method while writing a chunked response, HttpBroker finishes the current chunked response
and returns without writing another one.
*/

/*!
\fn HttpBroker::writeResponse(std::string_view body,
                               std::string_view mimeType,
                               HttpStatusCode statusCode = HttpStatusCode::OK,
                               std::initializer_list<std::pair<std::string, std::string>> headers = {})
Writes a response with \a statusCode, \a headers, and \a body to the peer. HttpBroker writes the server, date, and content-length
headers. HttpBroker writes, as the <em>Content-Length</em> field line's field value, the size of the \a body if it is not empty,
or zero otherwise. If \a mimeType is not empty, HttpBroker writes the <em>Content-Type</em> header.
HttpBroker writes the <em>Connection: close</em> field line to the header block if you called closeConnectionAfterResponding()
before calling this method.

If you call this method after writing the response, HttpBroker returns without writing another response.
If you call this method while writing a chunked response, HttpBroker finishes the current chunked response
and returns without writing another one.
*/

/*!
\fn HttpBroker::writeResponse(std::string_view body,
                               std::string_view mimeType,
                               HttpStatusCode statusCode,
                               const std::vector<std::pair<std::string, std::string>> &headers)
Writes a response with \a statusCode, \a headers, and \a body to the peer. HttpBroker writes the server, date, and content-length
headers. HttpBroker writes, as the <em>Content-Length</em> field line's field value, the size of the \a body if it is not empty,
or zero otherwise. If \a mimeType is not empty, HttpBroker writes the <em>Content-Type</em> header.
HttpBroker writes the <em>Connection: close</em> field line to the header block if you called closeConnectionAfterResponding()
before calling this method.

If you call this method after writing the response, HttpBroker returns without writing another response.
If you call this method while writing a chunked response, HttpBroker finishes the current chunked response
and returns without writing another one.
*/

/*!
\fn HttpBroker::writeChunkedResponse(HttpStatusCode statusCode = HttpStatusCode::OK,
                                      std::initializer_list<std::pair<std::string, std::string>> headers = {},
                                      std::initializer_list<std::string> expectedTrailerNames = {})
Writes status line and header block of the chunked response with \a statusCode and \a headers to the peer.
HttpBroker writes the server, date, and transfer encoding headers. If \a expectedTrailerNames is not empty,
HttpBroker writes to the header block a field line named \a Trailer containing as value all names given in \a expectedTrailerNames.
HttpBroker writes the <em>Connection: close</em> field line to the header block if you called closeConnectionAfterResponding()
before calling this method.

After calling this method to initiate a chunked response, you can call writeChunk() to write non-empty
chunks and writeLastChunk() to write the last chunk of the response.

If you call this method after writing the response, HttpBroker returns without writing another response.
If you call this method while writing a chunked response, HttpBroker finishes the current chunked response
and returns without writing another one.
*/

/*!
\fn HttpBroker::writeChunkedResponse(HttpStatusCode statusCode,
                                      const std::vector<std::pair<std::string, std::string>> &headers,
                                      const std::vector<std::string> &expectedTrailerNames)
Writes status line and header block of the chunked response with \a statusCode and \a headers to the peer.
HttpBroker writes the server, date, and transfer encoding headers. If \a expectedTrailerNames is not empty,
HttpBroker writes to the header block a field line named \a Trailer containing as value all names given in \a expectedTrailerNames.
HttpBroker writes the <em>Connection: close</em> field line to the header block if you called closeConnectionAfterResponding()
before calling this method.

After calling this method to initiate a chunked response, you can call writeChunk() to write non-empty
chunks and writeLastChunk() to write the last chunk of the response.

If you call this method after writing the response, HttpBroker returns without writing another response.
If you call this method while writing a chunked response, HttpBroker finishes the current chunked response
and returns without writing another one.
*/

/*!
\fn HttpBroker::writeChunkedResponse(std::string_view mimeType,
                                      HttpStatusCode statusCode = HttpStatusCode::OK,
                                      std::initializer_list<std::pair<std::string, std::string>> headers = {},
                                      std::initializer_list<std::string> expectedTrailerNames = {})
Writes status line and header block of the chunked response with \a statusCode and \a headers to the peer.
HttpBroker writes the server, date, and transfer encoding headers. If \a expectedTrailerNames is not empty,
HttpBroker writes to the header block a field line named \a Trailer containing as value all names given in \a expectedTrailerNames.
If \a mimeType is not empty, HttpBroker writes the <em>Content-Type</em> header. HttpBroker writes the <em>Connection: close</em>
field line to the header block if you called closeConnectionAfterResponding() before calling this method.

After calling this method to initiate a chunked response, you can call writeChunk() to write non-empty
chunks and writeLastChunk() to write the last chunk of the response.

If you call this method after writing the response, HttpBroker returns without writing another response.
If you call this method while writing a chunked response, HttpBroker finishes the current chunked response
and returns without writing another one.
*/

/*!
\fn HttpBroker::writeChunkedResponse(std::string_view mimeType,
                                      HttpStatusCode statusCode,
                                      const std::vector<std::pair<std::string, std::string>> &headers,
                                      const std::vector<std::string> &expectedTrailerNames)
Writes status line and header block of the chunked response with \a statusCode and \a headers to the peer.
HttpBroker writes the server, date, and transfer encoding headers. If \a expectedTrailerNames is not empty,
HttpBroker writes to the header block a field line named \a Trailer containing as value all names given in \a expectedTrailerNames.
If \a mimeType is not empty, HttpBroker writes the <em>Content-Type</em> header. HttpBroker writes the <em>Connection: close</em>
field line to the header block if you called closeConnectionAfterResponding() before calling this method.

After calling this method to initiate a chunked response, you can call writeChunk() to write non-empty
chunks and writeLastChunk() to write the last chunk of the response.

If you call this method after writing the response, HttpBroker returns without writing another response.
If you call this method while writing a chunked response, HttpBroker finishes the current chunked response
and returns without writing another one.
*/

/*!
\fn HttpBroker::writeChunk(std::string_view data)
Writes \a data chunk to the peer if you initiated a chunked response by calling writeChunkedResponse()
and if \a data is not empty. You can call writeLastChunk() to write the last chunk and finish writing
the chunked response.
*/

/*!
\fn HttpBroker::writeLastChunk(std::initializer_list<std::pair<std::string, std::string>> trailers = {})
Writes the last chunk to the peer if you initiated a chunked response by calling writeChunkedResponse().
If \a trailers is not empty, HttpBroker writes a trailer section after the last chunk.
*/

/*!
\fn HttpBroker::writeLastChunk(const std::vector<std::pair<std::string, std::string>> &trailers)
Writes the last chunk to the peer if you initiated a chunked response by calling writeChunkedResponse().
If \a trailers is not empty, HttpBroker writes a trailer section after the last chunk.
*/

/*!
\fn HttpBroker::bytesToSend()
Returns the bytes pending to be sent to the peer. You can use sentData() and bytesToSend() to write
well-behaved peers that write data according to the peer's capacity to process it.
*/

/*!
\fn HttpBroker::hasTrailers()
Returns true if the last chunk of the request has a trailer section.
*/

/*!
\fn HttpBroker::trailersCount()
Returns the number of field lines in the request trailer section.
*/

/*!
\fn HttpBroker::trailerCount(std::string_view name)
Returns the number of field lines with the given \a name in the request trailer section.
*/

/*!
\fn HttpBroker::hasTrailer(std::string_view name)
Returns true if the request trailer section contains at least one field line with the given \a name.
*/

/*!
\fn HttpBroker::trailer(std::string_view name, int pos = 1)
Returns the field line's field value with the given \a name at position \a pos in the request trailer section.
Position is relative to field lines having the same \a name.
*/

/*!
\fn HttpBroker::setQObject(QObject *pObject)
Sets the object responsible for receiving any pending body data for the request and writing the response.
HttpBroker takes ownership of \a pObject and deletes it after the set object finishes writing the response.

You should set a non-null object if you want to receive any pending body data and write a response after the handler
returns. HttpServer closes the connection if the called handler does not write the response and does not
set any object responsible for doing so.
*/

/*!
\fn HttpBroker::sentData(size_t count)
HttpBroker emits this signal whenever data is sent, at the socket level, to the connected peer.
You can use this signal and bytesToSend() to write well-behaved peers that write data according
to the connected peer's capacity to process them.
*/

/*!
\fn HttpBroker::receivedBodyData(std::string_view data, bool isLastPart)
HttpBroker emits this signal when it receives pending \a data for the request body.
HttpBroker sets \a isLastPart to true if the request body has been fully received.
For chunked requests, the last chunk is empty. Thus, \a data will be empty for
chunked requests when \a isLastPart is true. In this case, you can call hasTrailers()
to know if the peer sent a trailer section after the last chunk of the request.
*/

void HttpBroker::closeConnectionAfterResponding()
{
    Q_D(HttpBroker);
    d->closeConnectionAfterResponding();
}

void HttpBroker::writeResponse(HttpStatusCode statusCode,
                               std::initializer_list<std::pair<std::string, std::string>> headers)
{
    Q_D(HttpBroker);
    d->writeResponse(statusCode, headers);
}

void HttpBroker::writeResponse(HttpStatusCode statusCode,
                               const std::vector<std::pair<std::string, std::string>> &headers)
{
    Q_D(HttpBroker);
    d->writeResponse(statusCode, headers);
}

void HttpBroker::writeResponse(std::string_view body,
                               HttpStatusCode statusCode,
                               std::initializer_list<std::pair<std::string, std::string>> headers)
{
    Q_D(HttpBroker);
    d->writeResponse(body, statusCode, headers);
}

void HttpBroker::writeResponse(std::string_view body,
                               HttpStatusCode statusCode,
                               const std::vector<std::pair<std::string, std::string>> &headers)
{
    Q_D(HttpBroker);
    d->writeResponse(body, statusCode, headers);
}

void HttpBroker::writeResponse(std::string_view body,
                               std::string_view mimeType,
                               HttpStatusCode statusCode,
                               std::initializer_list<std::pair<std::string, std::string>> headers)
{
    Q_D(HttpBroker);
    d->writeResponse(body, mimeType, statusCode, headers);
}

void HttpBroker::writeResponse(std::string_view body,
                               std::string_view mimeType,
                               HttpStatusCode statusCode,
                               const std::vector<std::pair<std::string, std::string>> &headers)
{
    Q_D(HttpBroker);
    d->writeResponse(body, mimeType, statusCode, headers);
}

void HttpBroker::writeChunkedResponse(HttpStatusCode statusCode,
                                      std::initializer_list<std::pair<std::string, std::string>> headers,
                                      std::initializer_list<std::string> expectedTrailerNames)
{
    Q_D(HttpBroker);
    d->writeChunkedResponse(statusCode, headers, expectedTrailerNames);
}

void HttpBroker::writeChunkedResponse(HttpStatusCode statusCode,
                                      const std::vector<std::pair<std::string, std::string>> &headers,
                                      const std::vector<std::string> &expectedTrailerNames)
{
    Q_D(HttpBroker);
    d->writeChunkedResponse(statusCode, headers, expectedTrailerNames);
}

void HttpBroker::writeChunkedResponse(std::string_view mimeType,
                                      HttpStatusCode statusCode,
                                      std::initializer_list<std::pair<std::string, std::string>> headers,
                                      std::initializer_list<std::string> expectedTrailerNames)
{
    Q_D(HttpBroker);
    d->writeChunkedResponse(mimeType, statusCode, headers, expectedTrailerNames);
}

void HttpBroker::writeChunkedResponse(std::string_view mimeType,
                                      HttpStatusCode statusCode,
                                      const std::vector<std::pair<std::string, std::string>> &headers,
                                      const std::vector<std::string> &expectedTrailerNames)
{
    Q_D(HttpBroker);
    d->writeChunkedResponse(mimeType, statusCode, headers, expectedTrailerNames);
}

void HttpBroker::writeChunk(std::string_view data)
{
    Q_D(HttpBroker);
    d->writeChunk(data);
}

void HttpBroker::writeLastChunk(std::initializer_list<std::pair<std::string, std::string>> trailers)
{
    Q_D(HttpBroker);
    d->writeLastChunk(trailers);
}

void HttpBroker::writeLastChunk(const std::vector<std::pair<std::string, std::string>> &trailers)
{
    Q_D(HttpBroker);
    d->writeLastChunk(trailers);
}

size_t HttpBroker::bytesToSend() const
{
    Q_D(const HttpBroker);
    return d->bytesToSend();
}

bool HttpBroker::hasTrailers() const
{
    Q_D(const HttpBroker);
    return d->hasTrailers();
}

size_t HttpBroker::trailersCount() const
{
    Q_D(const HttpBroker);
    return d->trailersCount();
}

size_t HttpBroker::trailerCount(std::string_view name) const
{
    Q_D(const HttpBroker);
    return d->trailerCount(name);
}

bool HttpBroker::hasTrailer(std::string_view name) const
{
    Q_D(const HttpBroker);
    return d->hasTrailer(name);
}

std::string_view HttpBroker::trailer(std::string_view name, int pos) const
{
    Q_D(const HttpBroker);
    return d->trailer(name, pos);
}

void HttpBroker::setQObject(QObject *pObject)
{
    Q_D(HttpBroker);
    d->setQObject(pObject);
}

HttpBroker::HttpBroker(HttpBrokerPrivate *pBrokerPrivate) :
    d_ptr(pBrokerPrivate)
{
    assert(d_ptr);
    d_ptr->setBroker(this);
}

void HttpBroker::connectNotify(const QMetaMethod &signal)
{
    Q_D(HttpBroker);
    d->setConnected(true);
}

}
