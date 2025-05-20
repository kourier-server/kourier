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

#include "HttpChunkMetadataParser.h"
#include "../Core/IOChannel.h"
#include "../Core/SimdIterator.h"
#include <charconv>


namespace Kourier
{

HttpChunkMetadataParser::ChunkMetadataParserStatus HttpChunkMetadataParser::parse(IOChannel &ioChannel, size_t &chunkDataSize, size_t &chunkMetadataSize)
{
    // chunk-metadata = chunk-size [ chunk-ext ] CRLF
    // chunk-size     = 1*HEXDIG
    // chunk-ext      = *( BWS ";" BWS chunk-ext-name[ BWS "=" BWS chunk-ext-val ] )
    // chunk-ext-name = token
    // chunk-ext-val  = token / quoted-string
    // quoted-string  = DQUOTE *( qdtext / quoted-pair ) DQUOTE
    // qdtext         = HTAB / SP / %x21 / %x23-5B / %x5D-7E / obs-text
    // quoted-pair    = "\" ( HTAB / SP / VCHAR / obs-text )
    chunkDataSize = 0;
    chunkMetadataSize = 0;
    if (ioChannel.dataAvailable() < 3)
        return ChunkMetadataParserStatus::NeedsMoreData;
    SimdIterator it(ioChannel);
    size_t currentIndex = 0;
    const auto rawData = it.nextAt(0);
    const auto isNotDigit = _mm256_or_si256(_mm256_cmpgt_epi8(m_zero, rawData), _mm256_cmpgt_epi8(rawData, m_nine));
    const auto toUpper = _mm256_and_si256(rawData, m_upperCaseMask);
    const auto isNotAlpha = _mm256_or_si256(_mm256_cmpgt_epi8(m_upperCaseA, toUpper), _mm256_cmpgt_epi8(toUpper, m_upperCaseF));
    const auto matchCount = _tzcnt_u32(static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_and_si256(isNotDigit, isNotAlpha))));
    const auto hexDigitCount = std::min<size_t>(ioChannel.dataAvailable() - 1, matchCount);
    currentIndex += hexDigitCount;
    if (hexDigitCount > 0 && hexDigitCount <= 12 && ((hexDigitCount + 1) < ioChannel.dataAvailable()))
    {
        auto chunkDataSizeSlice = ioChannel.slice(0, hexDigitCount);
        auto [ptr, ec] {std::from_chars(chunkDataSizeSlice.data(), chunkDataSizeSlice.data() + hexDigitCount, chunkDataSize, 16)};
        if (std::errc() == ec)
        {
            if ((currentIndex + 2) > ioChannel.dataAvailable())
                return ChunkMetadataParserStatus::NeedsMoreData;
            while (true)
            {
                const auto data = it.nextAt(currentIndex);
                const auto result = _mm256_or_si256(_mm256_cmpeq_epi8(m_del, data), _mm256_andnot_si256(_mm256_cmpeq_epi8(m_htab, data), _mm256_and_si256(_mm256_cmpgt_epi8(data, m_minus1), _mm256_cmpgt_epi8(m_space, data))));
                const auto matchCount = std::min<size_t>(ioChannel.dataAvailable() - 2 - currentIndex, _tzcnt_u32(static_cast<uint32_t>(_mm256_movemask_epi8(result))));
                currentIndex += matchCount;
                if (matchCount == 32)
                    continue;
                if (ioChannel.slice(currentIndex, 2) == "\r\n")
                {
                    currentIndex += 2;
                    chunkMetadataSize = currentIndex;
                    if (chunkDataSize > 0)
                        return ChunkMetadataParserStatus::ExpectingChunkData;
                    else if ((currentIndex + 2) <= ioChannel.dataAvailable() && ioChannel.slice(currentIndex, 2) == "\r\n")
                    {
                        chunkMetadataSize += 2;
                        return ChunkMetadataParserStatus::ParsedRequest;
                    }
                    else
                        return ((currentIndex + 2) <= ioChannel.dataAvailable()) ? ChunkMetadataParserStatus::ExpectingTrailer : ChunkMetadataParserStatus::NeedsMoreData;
                }
                else if (((currentIndex + 2) == ioChannel.dataAvailable())
                           && ioChannel.peekChar(currentIndex) != '\r')
                {
                    chunkMetadataSize = currentIndex;
                    return ChunkMetadataParserStatus::NeedsMoreData;
                }
                else
                    return ChunkMetadataParserStatus::Failed;
            }
        }
        else
            return ChunkMetadataParserStatus::Failed;
    }
    else
    {
        if (hexDigitCount > 12 || hexDigitCount == 0)
            return ChunkMetadataParserStatus::Failed;
        else
            return ChunkMetadataParserStatus::NeedsMoreData;
    }
}

}
