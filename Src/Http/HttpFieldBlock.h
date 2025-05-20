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

#ifndef KOURIER_HTTP_FIELD_BLOCK_H
#define KOURIER_HTTP_FIELD_BLOCK_H

#include "../Core/IOChannel.h"
#include <string_view>
#include <limits>


namespace Kourier
{

struct FieldNameValueSizes {uint16_t nameSize = 0; uint16_t valueSize = 0;};

class HttpFieldBlock
{
public:
    HttpFieldBlock(IOChannel &ioChannel) : m_pIoChannel(&ioChannel) {}
    ~HttpFieldBlock() = default;
    void addFieldLine(size_t fieldNameStartIndex,
                      size_t fieldNameEndIndex,
                      size_t fieldValueStartIndex,
                      size_t fieldValueEndIndex);
    bool hasField(std::string_view fieldName) const;
    int fieldCount(std::string_view fieldName) const;
    inline void reset(size_t fieldBlockStartIndex) {m_fieldBlockStartIndex = fieldBlockStartIndex; m_fieldLinesCount = 0;}
    std::string_view fieldValue(std::string_view fieldName, int pos = 1) const;
    inline int fieldLinesCount() const {return m_fieldLinesCount;}
    constexpr static size_t maxFieldLines() {return sizeof(m_entries)/sizeof(FieldNameValueSizes);}
    constexpr static size_t maxFieldNameSize() {return std::numeric_limits<uint16_t>::max();}
    constexpr static size_t maxFieldValueSize() {return std::numeric_limits<uint16_t>::max();}

private:
    IOChannel *m_pIoChannel;
    size_t m_fieldBlockStartIndex = 0;
    uint8_t m_fieldLinesCount = 0;
    FieldNameValueSizes m_entries[128];
};

}

#endif // KOURIER_HTTP_FIELD_BLOCK_H
