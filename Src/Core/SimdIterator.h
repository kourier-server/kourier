//
// Copyright (C) 2023 Glauco Pacheco <glauco@kourier.io>
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

#ifndef KOURIER_SIMD_ITERATOR_H
#define KOURIER_SIMD_ITERATOR_H

#include "IOChannel.h"
#include <x86intrin.h>


namespace Kourier
{

class SimdIterator
{
public:
    SimdIterator(const IOChannel &ioDevice);
    SimdIterator(const SimdIterator &) = delete;
    ~SimdIterator() = default;
    SimdIterator &operator=(const SimdIterator &) = delete;
    inline __m256i nextAt(size_t index) const
    {
        if ((index < m_ioChannel.m_readBuffer.m_rightBlockSize))
            return _mm256_loadu_si256((__m256i_u const *)(m_ioChannel.m_readBuffer.m_pData + index));
        else
            return _mm256_loadu_si256((__m256i_u const *)(m_ioChannel.m_readBuffer.m_pBuffer + index - m_ioChannel.m_readBuffer.m_rightBlockSize));
    }

private:
    const IOChannel &m_ioChannel;
};

}

#endif // KOURIER_SIMD_ITERATOR_H
