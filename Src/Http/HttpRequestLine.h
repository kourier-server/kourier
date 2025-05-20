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

#ifndef KOURIER_HTTP_REQUEST_LINE_H
#define KOURIER_HTTP_REQUEST_LINE_H

#include "HttpRequest.h"


namespace Kourier
{

class HttpRequestLine
{
public:
    inline HttpRequest::Method method() const {return m_method;}
    inline void setMethod(HttpRequest::Method method) {m_method = method;}
    inline size_t targetPathStartIndex() const {return m_targetPathStartIndex;}
    inline void setTargetPathStartIndex(size_t idx) {m_targetPathStartIndex = idx;}
    inline size_t targetPathSize() const {return m_targetPathSize;}
    inline void setTargetPathSize(size_t size) {m_targetPathSize = size;}
    inline size_t targetQueryStartIndex() const {return m_targetQueryStartIndex;}
    inline void setTargetQueryStartIndex(size_t idx) {m_targetQueryStartIndex = idx;}
    inline size_t targetQuerySize() const {return m_targetQuerySize;}
    inline void setTargetQuerySize(size_t size) {m_targetQuerySize = size;}
    inline size_t targetUriSize() const {return m_targetPathSize + m_targetQuerySize + 1;}

private:
    HttpRequest::Method m_method = HttpRequest::Method::GET;
    size_t m_targetPathStartIndex = 0;
    size_t m_targetPathSize = 0;
    size_t m_targetQueryStartIndex = 0;
    size_t m_targetQuerySize = 0;
};

}

#endif // KOURIER_HTTP_REQUEST_LINE_H
