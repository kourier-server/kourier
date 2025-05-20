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

#ifndef KOURIER_HTTP_CHUNK_METADATA_PARSER_H
#define KOURIER_HTTP_CHUNK_METADATA_PARSER_H

#include <x86intrin.h>
#include <cstddef>


namespace Kourier
{
class IOChannel;

class HttpChunkMetadataParser
{
public:
    enum class ChunkMetadataParserStatus {ExpectingChunkData, ParsedRequest, ExpectingTrailer, NeedsMoreData, Failed};
    static ChunkMetadataParserStatus parse(IOChannel &ioChannel, size_t &chunkDataSize, size_t &chunkMetadataSize);

private:
    static constexpr __m256i m_zero = (__m256i)(__v32qi){char('0'), char('0'), char('0'), char('0'), char('0'), char('0'), char('0'), char('0'), char('0'), char('0'), char('0'), char('0'), char('0'), char('0'), char('0'), char('0'), char('0'), char('0'), char('0'), char('0'), char('0'), char('0'), char('0'), char('0'), char('0'), char('0'), char('0'), char('0'), char('0'), char('0'), char('0'), char('0')};
    static constexpr __m256i m_nine = (__m256i)(__v32qi){char('9'), char('9'), char('9'), char('9'), char('9'), char('9'), char('9'), char('9'), char('9'), char('9'), char('9'), char('9'), char('9'), char('9'), char('9'), char('9'), char('9'), char('9'), char('9'), char('9'), char('9'), char('9'), char('9'), char('9'), char('9'), char('9'), char('9'), char('9'), char('9'), char('9'), char('9'), char('9')};
    static constexpr __m256i m_upperCaseMask = (__m256i)(__v32qi){char(0xDF), char(0xDF), char(0xDF), char(0xDF), char(0xDF), char(0xDF), char(0xDF), char(0xDF), char(0xDF), char(0xDF), char(0xDF), char(0xDF), char(0xDF), char(0xDF), char(0xDF), char(0xDF), char(0xDF), char(0xDF), char(0xDF), char(0xDF), char(0xDF), char(0xDF), char(0xDF), char(0xDF), char(0xDF), char(0xDF), char(0xDF), char(0xDF), char(0xDF), char(0xDF), char(0xDF), char(0xDF)};
    static constexpr __m256i m_upperCaseA = (__m256i)(__v32qi){char('A'), char('A'), char('A'), char('A'), char('A'), char('A'), char('A'), char('A'), char('A'), char('A'), char('A'), char('A'), char('A'), char('A'), char('A'), char('A'), char('A'), char('A'), char('A'), char('A'), char('A'), char('A'), char('A'), char('A'), char('A'), char('A'), char('A'), char('A'), char('A'), char('A'), char('A'), char('A')};
    static constexpr __m256i m_upperCaseF = (__m256i)(__v32qi){char('F'), char('F'), char('F'), char('F'), char('F'), char('F'), char('F'), char('F'), char('F'), char('F'), char('F'), char('F'), char('F'), char('F'), char('F'), char('F'), char('F'), char('F'), char('F'), char('F'), char('F'), char('F'), char('F'), char('F'), char('F'), char('F'), char('F'), char('F'), char('F'), char('F'), char('F'), char('F')};
    static constexpr __m256i m_htab = (__m256i)(__v32qi){char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09), char(0x09)};
    static constexpr __m256i m_space = (__m256i)(__v32qi){char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20), char(0x20)};
    static constexpr __m256i m_del = (__m256i)(__v32qi){char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F), char(0x7F)};
    static constexpr __m256i m_minus1 = (__m256i)(__v32qi){char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1), char(-1)};
};

}

#endif // KOURIER_HTTP_CHUNK_METADATA_PARSER_H
