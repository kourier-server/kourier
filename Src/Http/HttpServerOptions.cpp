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

#include "HttpServerOptions.h"
#include "HttpRequestLimits.h"
#include "HttpFieldBlock.h"
#include <QThread>


namespace Kourier
{

bool HttpServerOptions::setOption(HttpServer::ServerOption option, int64_t value)
{
    if (value < 0)
    {
        m_errorMessage = "Failed to set option. Option values must be non-negative.";
        return false;
    }
    switch (option)
    {
        case HttpServer::ServerOption::WorkerCount:
        case HttpServer::ServerOption::TcpServerBacklogSize:
        case HttpServer::ServerOption::IdleTimeoutInSecs:
        case HttpServer::ServerOption::RequestTimeoutInSecs:
            break;
        case HttpServer::ServerOption::MaxUrlSize:
        case HttpServer::ServerOption::MaxHeaderNameSize:
        case HttpServer::ServerOption::MaxHeaderValueSize:
        case HttpServer::ServerOption::MaxHeaderLineCount:
        case HttpServer::ServerOption::MaxTrailerNameSize:
        case HttpServer::ServerOption::MaxTrailerValueSize:
        case HttpServer::ServerOption::MaxTrailerLineCount:
        case HttpServer::ServerOption::MaxChunkMetadataSize:
        case HttpServer::ServerOption::MaxRequestSize:
        case HttpServer::ServerOption::MaxBodySize:
        case HttpServer::ServerOption::MaxConnectionCount:
            value = (value > 0) ? value : maxOptionValue(option);
            break;
    }
    switch (option)
    {
        case HttpServer::ServerOption::WorkerCount:
            if (value > QThread::idealThreadCount())
            {
                m_errorMessage = std::string("Failed to set worker count. Maximum possible value is ").append(std::to_string(QThread::idealThreadCount())).append(".");
                return false;
            }
            else
                break;
        case HttpServer::ServerOption::TcpServerBacklogSize:
            if (value > std::numeric_limits<int>::max())
            {
                m_errorMessage = std::string("Failed to set server backlog size. Maximum possible value is ").append(std::to_string(std::numeric_limits<int>::max())).append(".");
                return false;
            }
            else if (value == 0)
            {
                m_errorMessage = "Failed to set server backlog size. Value must be positive.";
                return false;
            }
            else
                break;
        case HttpServer::ServerOption::IdleTimeoutInSecs:
        case HttpServer::ServerOption::RequestTimeoutInSecs:
            if (value > std::numeric_limits<int>::max())
            {
                m_errorMessage = std::string("Failed to set timeout. Maximum possible value is ").append(std::to_string(std::numeric_limits<int>::max())).append(".");
                return false;
            }
            else
                break;
        case HttpServer::ServerOption::MaxHeaderNameSize:
        case HttpServer::ServerOption::MaxTrailerNameSize:
            if (value > HttpFieldBlock::maxFieldNameSize())
            {
                m_errorMessage = std::string("Failed to set limit on (header/trailer) field name size. Maximum possible value is ").append(std::to_string(HttpFieldBlock::maxFieldNameSize())).append(".");
                return false;
            }
            else
                break;
        case HttpServer::ServerOption::MaxHeaderValueSize:
        case HttpServer::ServerOption::MaxTrailerValueSize:
            if (value > HttpFieldBlock::maxFieldValueSize())
            {
                m_errorMessage = std::string("Failed to set limit on (header/trailer) field value size. Maximum possible value is ").append(std::to_string(HttpFieldBlock::maxFieldValueSize())).append(".");
                return false;
            }
            else
                break;
        case HttpServer::ServerOption::MaxHeaderLineCount:
        case HttpServer::ServerOption::MaxTrailerLineCount:
            if (value > HttpFieldBlock::maxFieldLines())
            {
                m_errorMessage = std::string("Failed to set limit on (header/trailer) field line count. Maximum possible value is ").append(std::to_string(HttpFieldBlock::maxFieldLines())).append(".");
                return false;
            }
            else
                break;
        case HttpServer::ServerOption::MaxUrlSize:
        case HttpServer::ServerOption::MaxChunkMetadataSize:
        case HttpServer::ServerOption::MaxRequestSize:
        case HttpServer::ServerOption::MaxBodySize:
        case HttpServer::ServerOption::MaxConnectionCount:
            break;
    }
    m_options[option] = value;
    return true;
}

int64_t HttpServerOptions::getOption(HttpServer::ServerOption option) const
{
    auto it = m_options.find(option);
    if (it != m_options.cend())
        return it->second;
    else
        return defaultOptionValue(option);
}

int64_t HttpServerOptions::defaultOptionValue(HttpServer::ServerOption option)
{
    switch (option)
    {
        case HttpServer::ServerOption::WorkerCount:
            return QThread::idealThreadCount();
        case HttpServer::ServerOption::TcpServerBacklogSize:
            return 1 << 12;
        case HttpServer::ServerOption::IdleTimeoutInSecs:
            return 0;
        case HttpServer::ServerOption::RequestTimeoutInSecs:
            return 0;
        case HttpServer::ServerOption::MaxUrlSize:
            return HttpRequestLimits().maxUrlSize;
        case HttpServer::ServerOption::MaxHeaderNameSize:
            return HttpRequestLimits().maxHeaderNameSize;
        case HttpServer::ServerOption::MaxHeaderValueSize:
            return HttpRequestLimits().maxHeaderValueSize;
        case HttpServer::ServerOption::MaxHeaderLineCount:
            return HttpRequestLimits().maxHeaderLineCount;
        case HttpServer::ServerOption::MaxTrailerNameSize:
            return HttpRequestLimits().maxTrailerNameSize;
        case HttpServer::ServerOption::MaxTrailerValueSize:
            return HttpRequestLimits().maxTrailerValueSize;
        case HttpServer::ServerOption::MaxTrailerLineCount:
            return HttpRequestLimits().maxTrailerLineCount;
        case HttpServer::ServerOption::MaxChunkMetadataSize:
            return HttpRequestLimits().maxChunkMetadataSize;
        case HttpServer::ServerOption::MaxRequestSize:
            return HttpRequestLimits().maxRequestSize;
        case HttpServer::ServerOption::MaxBodySize:
            return HttpRequestLimits().maxBodySize;
        case HttpServer::ServerOption::MaxConnectionCount:
            return 0;
        default:
            Q_UNREACHABLE();
    }
}

int64_t HttpServerOptions::maxOptionValue(HttpServer::ServerOption option)
{
    switch (option)
    {
        case HttpServer::ServerOption::WorkerCount:
            return QThread::idealThreadCount();
        case HttpServer::ServerOption::TcpServerBacklogSize:
        case HttpServer::ServerOption::IdleTimeoutInSecs:
        case HttpServer::ServerOption::RequestTimeoutInSecs:
            return std::numeric_limits<int>::max();
        case HttpServer::ServerOption::MaxHeaderNameSize:
        case HttpServer::ServerOption::MaxTrailerNameSize:
            return HttpFieldBlock::maxFieldNameSize();
        case HttpServer::ServerOption::MaxHeaderValueSize:
        case HttpServer::ServerOption::MaxTrailerValueSize:
            return HttpFieldBlock::maxFieldValueSize();
        case HttpServer::ServerOption::MaxHeaderLineCount:
        case HttpServer::ServerOption::MaxTrailerLineCount:
            return HttpFieldBlock::maxFieldLines();
        case HttpServer::ServerOption::MaxUrlSize:
        case HttpServer::ServerOption::MaxChunkMetadataSize:
        case HttpServer::ServerOption::MaxRequestSize:
        case HttpServer::ServerOption::MaxBodySize:
        case HttpServer::ServerOption::MaxConnectionCount:
            return std::numeric_limits<int64_t>::max();
        default:
            Q_UNREACHABLE();
    }
}

}
