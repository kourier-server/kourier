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

#include "HttpRequestRouter.h"
#include <QUrl>
#include <QDir>


namespace Kourier
{

bool HttpRequestRouter::addRoute(HttpRequest::Method method, std::string_view path, RequestHandler pRequestHandler)
{
    if (!isAbsolutePath(path) && (method != HttpRequest::Method::OPTIONS || path != "*"))
        return false;
    else if (pRequestHandler == nullptr)
    {
        m_errorMessage = std::string("Failed to register route ").append(path).append(". Given function pointer is null.");
        return false;
    }
    else
    {
        auto &handlers = m_handlers[(size_t)method];
        auto pos = handlers.begin();
        while (pos != handlers.end())
        {
            if (path > pos->path)
            {
                handlers.insert(pos, {method, std::string{path}, pRequestHandler});
                return true;
            }
            else if (path == pos->path)
            {
                pos->pHandler = pRequestHandler;
                return true;
            }
            ++pos;
        }
        handlers.insert(pos, {method, std::string{path}, pRequestHandler});
        return true;
    }
}

HttpRequestRouter::RequestHandler HttpRequestRouter::getHandler(HttpRequest::Method method, std::string_view path) const
{
    auto &handlers = m_handlers[(size_t)method];
    for (auto &handler : handlers)
    {
        if (path.starts_with(handler.path))
            return handler.pHandler;
    }
    return nullptr;
}

bool HttpRequestRouter::isAbsolutePath(std::string_view path)
{
    if (path.empty())
    {
        m_errorMessage = "Failed to add route. Given path is empty.";
        return false;
    }
    else if (!path.starts_with('/'))
    {
        m_errorMessage = "Failed to add route. Given path is not an absolute path.";
        return false;
    }
    QUrl url = QUrl::fromEncoded(QByteArray(path.data(), path.size()), QUrl::StrictMode);
    if (!url.isValid())
    {
        m_errorMessage = std::string("Failed to add route. Given path is not valid. ").append(url.errorString().toStdString());
        return false;
    }
    if (!QDir::isAbsolutePath(url.path())
        || url.hasQuery()
        || url.hasFragment()
        || !url.authority().isEmpty()
        || !url.scheme().isEmpty())
    {
        m_errorMessage = std::string("Failed to add route. Given path is not an absolute path.");
        return false;
    }
    return true;
}

}
