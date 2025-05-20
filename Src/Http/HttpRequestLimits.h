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

#ifndef KOURIER_HTTP_REQUEST_LIMITS_H
#define KOURIER_HTTP_REQUEST_LIMITS_H

#include <cstddef>


namespace Kourier
{

struct HttpRequestLimits
{
public:
    size_t maxUrlSize = 1 << 13;
    size_t maxHeaderNameSize = 1 << 10;
    size_t maxHeaderValueSize = 1 << 13;
    size_t maxHeaderLineCount = 1 << 6;
    size_t maxTrailerNameSize = 1 << 10;
    size_t maxTrailerValueSize = 1 << 13;
    size_t maxTrailerLineCount = 1 << 6;
    size_t maxChunkMetadataSize = 1 << 10;
    size_t maxRequestSize = 1 << 25;
    size_t maxBodySize = 1 << 25;
};

}

#endif // KOURIER_HTTP_REQUEST_LIMITS_H
