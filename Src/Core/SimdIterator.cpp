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

#include "SimdIterator.h"
#include <cstring>


namespace Kourier
{

SimdIterator::SimdIterator(const IOChannel &ioChannel) : m_ioChannel(ioChannel)
{
    assert(m_ioChannel.m_readBuffer.m_currentCapacity >= 32);
    std::memcpy(m_ioChannel.m_readBuffer.m_pBuffer + m_ioChannel.m_readBuffer.m_currentCapacity, m_ioChannel.m_readBuffer.m_pBuffer, 32);
}

}
