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

#include "HttpFieldBlock.h"
#include <strings.h>


namespace Kourier
{

void HttpFieldBlock::addFieldLine(size_t fieldNameStartIndex,
                                  size_t fieldNameEndIndex,
                                  size_t fieldValueStartIndex,
                                  size_t fieldValueEndIndex)
{
    assert(m_fieldLinesCount < (maxFieldLines() - 1));
    m_entries[m_fieldLinesCount++] = FieldNameValueSizes{.nameSize = static_cast<uint16_t>(1 + fieldNameEndIndex - fieldNameStartIndex),
                                                       .valueSize = static_cast<uint16_t>((fieldValueEndIndex >= fieldValueStartIndex) ? 1 + fieldValueEndIndex - fieldValueStartIndex : 0)};
}

bool HttpFieldBlock::hasField(std::string_view fieldName) const
{
    if (!fieldName.empty())
    {
        auto nameStartIndex = m_fieldBlockStartIndex;
        for (auto i = 0; i < m_fieldLinesCount; ++i)
        {
            const auto &currentFieldSize = m_entries[i];
            const auto currentNameStartIndex = nameStartIndex;
            nameStartIndex += currentFieldSize.nameSize + currentFieldSize.valueSize + 3;
            if (fieldName.size() != currentFieldSize.nameSize)
                continue;
            else
            {
                const auto currentFieldName = m_pIoChannel->slice(currentNameStartIndex, currentFieldSize.nameSize);
                if (0 != strncasecmp(currentFieldName.data(), fieldName.data(), fieldName.size()))
                    continue;
                else
                    return true;
            }
        }
        return false;
    }
    else
        return false;
}

int HttpFieldBlock::fieldCount(std::string_view fieldName) const
{
    int matchCount = 0;
    if (!fieldName.empty())
    {
        auto nameStartIndex = m_fieldBlockStartIndex;
        for (auto i = 0; i < m_fieldLinesCount; ++i)
        {
            const auto &currentFieldSize = m_entries[i];
            const auto currentNameStartIndex = nameStartIndex;
            nameStartIndex += currentFieldSize.nameSize + currentFieldSize.valueSize + 3;
            if (fieldName.size() != currentFieldSize.nameSize)
                continue;
            else
            {
                const auto currentFieldName = m_pIoChannel->slice(currentNameStartIndex, currentFieldSize.nameSize);
                if (0 != strncasecmp(currentFieldName.data(), fieldName.data(), fieldName.size()))
                    continue;
                else
                    ++matchCount;
            }
        }
    }
    return matchCount;
}

std::string_view HttpFieldBlock::fieldValue(std::string_view fieldName, int pos) const
{
    int currentPos = 0;
    if (!fieldName.empty())
    {
        auto nameStartIndex = m_fieldBlockStartIndex;
        for (auto i = 0; i < m_fieldLinesCount; ++i)
        {
            const auto &currentFieldSize = m_entries[i];
            const auto currentNameStartIndex = nameStartIndex;
            nameStartIndex += currentFieldSize.nameSize + currentFieldSize.valueSize + 3;
            if (fieldName.size() != currentFieldSize.nameSize)
                continue;
            else
            {
                const auto currentFieldName = m_pIoChannel->slice(currentNameStartIndex, currentFieldSize.nameSize);
                if (0 != strncasecmp(currentFieldName.data(), fieldName.data(), fieldName.size()))
                    continue;
                else
                {
                    if (++currentPos == pos)
                    {
                        if (currentFieldSize.valueSize > 0)
                        {
                            const int64_t currentFieldValueStartIndex = currentNameStartIndex + currentFieldSize.nameSize + 1;
                            const auto rawFieldValue = m_pIoChannel->slice(currentFieldValueStartIndex, currentFieldSize.valueSize);
                            const char *pBegin = rawFieldValue.data();
                            const char *pEnd = rawFieldValue.data() + rawFieldValue.size() - 1;
                            while (pBegin < pEnd && (0x20 == *pEnd || 0x09 == *pEnd))
                                --pEnd;
                            while (pBegin < pEnd && (0x20 == *pBegin || 0x09 == *pBegin))
                                ++pBegin;
                            if ((pEnd > pBegin) || (0x09 != *pBegin && 0x20 != *pBegin))
                                return std::string_view(pBegin, pEnd - pBegin + 1);
                            else
                                return {};
                        }
                        else
                            return {};
                    }
                }
            }
        }
    }
    return {};
}

}
