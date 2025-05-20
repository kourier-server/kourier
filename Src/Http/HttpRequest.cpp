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

#include "HttpRequest.h"
#include "HttpRequestPrivate.h"


namespace Kourier
{

/*!
\class Kourier::HttpRequest
\brief The HttpRequest class represents an HTTP request.

HttpRequest cannot be created by you.
It is created by HttpServer and passed as an argument to the mapped handler.
You can call HttpServer::addRoute to map handlers to HTTP methods and paths.

HttpServer calls the mapped handler right after it parses the request header block.
If the request has a body that is not chunked, HttpServer processes
all body data that were available when the header block was fully parsed. You can call
isComplete() to know if HttpRequest represents a complete request.

HttpRequest is only valid inside the called handler. You cannot call its methods outside of the handler
function. You can use the HttpBroker instance HttpServer passes as an argument to the handler to receive
the pending body data.
*/

/*!
\fn HttpRequest::~HttpRequest()
Destroys the object.
*/

/*!
\enum HttpRequest::Method
\brief This enum describes the HTTP request method
\var HttpRequest::Method::GET
\brief HTTP GET request.
\var HttpRequest::Method::PUT
\brief HTTP PUT request.
\var HttpRequest::Method::POST
\brief HTTP POST request.
\var HttpRequest::Method::PATCH
\brief HTTP PATCH request.
\var HttpRequest::Method::DELETE
\brief HTTP DELETE request.
\var HttpRequest::Method::HEAD
\brief HTTP HEAD request.
\var HttpRequest::Method::OPTIONS
\brief HTTP OPTIONS request.
*/

/*!
\fn HttpRequest::method()
Returns the request method.
*/

/*!
\fn HttpRequest::targetPath()
Returns the request path.
*/

/*!
\fn HttpRequest::targetQuery()
Returns the request query. Returns an empty string view if the request has no query.
*/

/*!
\fn HttpRequest::headersCount()
Returns the number of field lines in the header block.
*/

/*!
\fn HttpRequest::headerCount(std::string_view name)
Returns the number of field lines with the given \a name in the header block.
*/

/*!
\fn HttpRequest::hasHeader(std::string_view name)
Returns true if the header block contains at least one field line with the given \a name.
*/

/*!
\fn HttpRequest::header(std::string_view name, int pos = 1)
Returns the field line's field value with the given \a name at position \a pos in the header block. Position
is relative to field lines having the same \a name.
*/

/*!
\fn HttpRequest::chunked()
Returns true if the request has a body given in chunks.
If the request has no body or if the body is not given in chunks, this method returns false.
*/

/*!
\fn HttpRequest::isComplete()
Returns true if the request represented by this instance is complete.
A request can only be complete when HttpServer calls the mapped handler if the request doesn't have a body or if
the body is not chunked and all of its data is available when the request header block is parsed.
*/

/*!
\enum HttpRequest::BodyType
\brief This enum describes the request's body type
\var HttpRequest::BodyType::NoBody
\brief The request does not have a body.
\var HttpRequest::BodyType::NotChunked
\brief The request contains a body of a known size that is given right after the header block and the body is not chunked.
\var HttpRequest::BodyType::Chunked
\brief The request contains a body of an unknown size, which is given in chunks.
*/

/*!
\fn HttpRequest::bodyType()
Returns the request body type.
*/

/*!
\fn HttpRequest::requestBodySize()
Returns the size of the body in the request. If the request has no body or if it has a chunked body, this method returns 0.
*/

/*!
\fn HttpRequest::pendingBodySize()
Returns the number of bytes pending for the request body to be fully received.
If the request has no body or if it has a chunked body, this method returns 0.
*/

/*!
\fn HttpRequest::hasBody()
Returns true if the request has a body. You can call chunked() or bodyType() to
know whether the request body is chunked. If the request body is not chunked, you can call
requestBodySize() to get the size of the body and body() to fetch the body data that
was available at the time HttpServer finished parsing the header block and created this
instance before passing it to the mapped handler.
*/

/*!
\fn HttpRequest::body()
This method returns an empty string view if the request has no body or if it has a chunked body. Otherwise,
this method returns the available body data when HttpServer parses the headers block. In this case, you can
call requestBodySize() to know the size of the request body and pendingBodySize() to know how much body data
is still pending to be processed. Both requestBodySize() and pendingBodySize() methods return zero if the body is chunked.
*/

/*!
\fn HttpRequest::peerAddress()
Returns the requester's IP.
*/

/*!
\fn HttpRequest::peerPort()
Returns the requester's port.
*/

HttpRequest::~HttpRequest()
{
    delete d_ptr;
}

HttpRequest::Method HttpRequest::method() const
{
    Q_D(const HttpRequest);
    return d->method();
}

std::string_view HttpRequest::targetPath() const
{
    Q_D(const HttpRequest);
    return d->targetPath();
}

std::string_view HttpRequest::targetQuery() const
{
    Q_D(const HttpRequest);
    return d->targetQuery();
}

size_t HttpRequest::headersCount() const
{
    Q_D(const HttpRequest);
    return d->headersCount();
}

size_t HttpRequest::headerCount(std::string_view name) const
{
    Q_D(const HttpRequest);
    return d->headerCount(name);
}

bool HttpRequest::hasHeader(std::string_view name) const
{
    Q_D(const HttpRequest);
    return d->hasHeader(name);
}

std::string_view HttpRequest::header(std::string_view name, int pos) const
{
    Q_D(const HttpRequest);
    return d->header(name, pos);
}

bool HttpRequest::chunked() const
{
    Q_D(const HttpRequest);
    return d->chunked();
}

bool HttpRequest::isComplete() const
{
    Q_D(const HttpRequest);
    return d->isComplete();
}

HttpRequest::BodyType HttpRequest::bodyType() const
{
    Q_D(const HttpRequest);
    return d->bodyType();
}

size_t HttpRequest::requestBodySize() const
{
    Q_D(const HttpRequest);
    return d->requestBodySize();
}

size_t HttpRequest::pendingBodySize() const
{
    Q_D(const HttpRequest);
    return d->pendingBodySize();
}

bool HttpRequest::hasBody() const
{
    Q_D(const HttpRequest);
    return d->hasBody();
}

std::string_view HttpRequest::body() const
{
    Q_D(const HttpRequest);
    return d->body();
}

std::string_view HttpRequest::peerAddress() const
{
    Q_D(const HttpRequest);
    return d->peerAddress();
}

uint16_t HttpRequest::peerPort() const
{
    Q_D(const HttpRequest);
    return d->peerPort();
}

HttpRequest::HttpRequest(HttpRequestPrivate *pHttpRequestPrivate) :
    d_ptr(pHttpRequestPrivate)
{
    assert(d_ptr);
}

}
