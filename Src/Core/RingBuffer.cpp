//
// Copyright (C) 2022 Glauco Pacheco <glauco@kourier.io>
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

#include "RingBuffer.h"
#include "NoDestroy.h"
#include <algorithm>
#include <vector>
#include <cstring>
#include <bit>


namespace Kourier
{

// extraSizeAtBufferEndInBytes is used by the SIMD/Data iterators
// to optimize data processing.
constexpr static size_t extraSizeAtBufferEnd = 64;

RingBuffer::RingBuffer(size_t capacity) :
    m_capacity(capacity)
{
    m_currentCapacity = (m_capacity > 0) ? std::min(m_capacity, RingBuffer::defaultCapacity()) : RingBuffer::defaultCapacity();
    m_spaceAvailableAtRightSide = m_currentCapacity;
    m_pBuffer = new char[m_currentCapacity + extraSizeAtBufferEnd];
    m_pData = m_pBuffer;
}

RingBuffer::~RingBuffer()
{
    if (nullptr != m_pBuffer)
        delete [] m_pBuffer;
}

size_t RingBuffer::read(char *pData, const size_t maxSize)
{
    if (m_rightBlockSize > maxSize)
    {
        std::memcpy(pData, m_pData, maxSize);
        m_pData += maxSize;
        m_rightBlockSize -= maxSize;
        return maxSize;
    }
    else
    {
        const size_t sizeToRead = std::min<size_t>(maxSize, size());
        const size_t remainingSize = size() - sizeToRead;
        const size_t dataToReadAtLeftSide = sizeToRead - m_rightBlockSize;
        std::memcpy(pData, m_pData, m_rightBlockSize);
        std::memcpy(pData + m_rightBlockSize, m_pBuffer, dataToReadAtLeftSide);
        m_pData = m_pBuffer + dataToReadAtLeftSide;
        m_rightBlockSize = m_leftBlockSize - dataToReadAtLeftSide;
        m_spaceAvailableAtRightSide = m_currentCapacity - m_leftBlockSize;
        m_leftBlockSize = 0;
        if (remainingSize == 0)
        {
            m_spaceAvailableAtRightSide = m_currentCapacity;
            m_pData = m_pBuffer;
        }
        return sizeToRead;
    }
}

size_t RingBuffer::read(DataSink &dataSink)
{
    size_t sizeRead = dataSink.write(m_pData, m_rightBlockSize);
    if (m_rightBlockSize > sizeRead)
    {
        m_pData += sizeRead;
        m_rightBlockSize -= sizeRead;
        return sizeRead;
    }
    else
    {
        const size_t dataReadAtLeftSide = dataSink.write(m_pBuffer, m_leftBlockSize);
        sizeRead += dataReadAtLeftSide;
        const auto remainingSize = size() - sizeRead;
        m_pData = m_pBuffer + dataReadAtLeftSide;
        m_rightBlockSize = m_leftBlockSize - dataReadAtLeftSide;
        m_spaceAvailableAtRightSide = m_currentCapacity - m_leftBlockSize;
        m_leftBlockSize = 0;
        if (remainingSize == 0)
        {
            m_spaceAvailableAtRightSide = m_currentCapacity;
            m_pData = m_pBuffer;
        }
        return sizeRead;
    }
}

size_t RingBuffer::write(const char *pData, size_t maxSize)
{
    const size_t availableFreeSize = m_currentCapacity - size();
    if (availableFreeSize < maxSize)
        tryToEnlargeBuffer(maxSize - availableFreeSize);
    if (m_spaceAvailableAtRightSide > maxSize)
    {
        std::memcpy(m_pData + m_rightBlockSize, pData, maxSize);
        m_rightBlockSize += maxSize;
        m_spaceAvailableAtRightSide -= maxSize;
        return maxSize;
    }
    else
    {
        const size_t sizeToWrite = std::min<size_t>(maxSize, m_currentCapacity - size());
        std::memcpy(m_pData + m_rightBlockSize, pData, m_spaceAvailableAtRightSide);
        m_rightBlockSize += m_spaceAvailableAtRightSide;
        std::memcpy(m_pBuffer + m_leftBlockSize, pData + m_spaceAvailableAtRightSide, sizeToWrite - m_spaceAvailableAtRightSide);
        m_leftBlockSize += sizeToWrite - m_spaceAvailableAtRightSide;
        m_spaceAvailableAtRightSide = 0;
        return sizeToWrite;
    }
}

size_t RingBuffer::write(DataSource &dataSource)
{
    const size_t dataAvailable = dataSource.dataAvailable();
    const size_t availableFreeSize = m_currentCapacity - size();
    if (availableFreeSize < dataAvailable)
        tryToEnlargeBuffer(dataAvailable - availableFreeSize);
    size_t writtenSize = dataSource.read(m_pData + m_rightBlockSize, m_spaceAvailableAtRightSide);
    if (m_spaceAvailableAtRightSide > writtenSize)
    {
        m_rightBlockSize += writtenSize;
        m_spaceAvailableAtRightSide -= writtenSize;
        return writtenSize;
    }
    else
    {
        writtenSize += dataSource.read(m_pBuffer + m_leftBlockSize, m_currentCapacity - size() - m_spaceAvailableAtRightSide);
        m_rightBlockSize += m_spaceAvailableAtRightSide;
        m_leftBlockSize += writtenSize - m_spaceAvailableAtRightSide;
        m_spaceAvailableAtRightSide = 0;
        return writtenSize;
    }
}

static std::vector<char> *threadLocalBuffer()
{
    static thread_local NoDestroy<std::vector<char>*> pThreadLocalBuffer(new std::vector<char>);
    static thread_local NoDestroyPtrDeleter<std::vector<char>*> vectorBufferDestroyer(pThreadLocalBuffer);
    return pThreadLocalBuffer();
}

std::string_view RingBuffer::slice(size_t pos, size_t count)
{
    assert(count > 0 && (pos + count) <= size());
    if ((pos + count) <= m_rightBlockSize)
        return std::string_view{m_pData + pos, count};
    else if (pos >= m_rightBlockSize)
        return std::string_view{m_pBuffer + pos - m_rightBlockSize, count};
    else
    {
        std::vector<char> *pBuffer = threadLocalBuffer();
        if (pBuffer != nullptr)
        {
            auto &buffer = *pBuffer;
            if (buffer.size() < count)
            {
                buffer.resize(std::max<size_t>(4096, count));
                buffer.shrink_to_fit();
            }
            std::memcpy(buffer.data(), m_pData + pos, m_rightBlockSize - pos);
            std::memcpy(buffer.data() + m_rightBlockSize - pos, m_pBuffer, count - (m_rightBlockSize - pos));
            return std::string_view{buffer.data(), count};
        }
        else
        {
            char *pNewBuffer = new char[m_currentCapacity + extraSizeAtBufferEnd];
            std::memcpy(pNewBuffer, m_pData, m_rightBlockSize);
            std::memcpy(pNewBuffer + m_rightBlockSize, m_pBuffer, m_leftBlockSize);
            delete [] m_pBuffer;
            m_pBuffer = pNewBuffer;
            m_pData = pNewBuffer;
            m_rightBlockSize += m_leftBlockSize;
            m_leftBlockSize = 0;
            m_spaceAvailableAtRightSide = m_currentCapacity - m_rightBlockSize;
            return std::string_view{m_pData + pos, count};
        }
    }
}

size_t RingBuffer::popFront(size_t maxSize)
{
    if (m_rightBlockSize > maxSize)
    {
        m_pData += maxSize;
        m_rightBlockSize -= maxSize;
        return maxSize;
    }
    else
    {
        const size_t sizeToPop = std::min<size_t>(maxSize, size());
        const size_t sizeToPopAtLeftSide = sizeToPop - m_rightBlockSize;
        const size_t remainingSize = size() - sizeToPop;
        m_pData = m_pBuffer + sizeToPopAtLeftSide;
        m_rightBlockSize = m_leftBlockSize - sizeToPopAtLeftSide;
        m_spaceAvailableAtRightSide = m_currentCapacity - m_leftBlockSize;
        m_leftBlockSize = 0;
        if (remainingSize == 0)
        {
            m_spaceAvailableAtRightSide = m_currentCapacity;
            m_pData = m_pBuffer;
        }
        return sizeToPop;
    }
}

bool RingBuffer::setCapacity(size_t capacity)
{
    if (capacity >= m_currentCapacity || capacity == 0)
    {
        m_capacity = capacity;
        return true;
    }
    else
    {
        if (size() > capacity)
            return false;
        else
        {
            m_capacity = capacity;
            const auto newCurrentCapacity = std::min<size_t>(m_capacity, std::max<size_t>(defaultCapacity(), std::bit_ceil(size())));
            char *pNewBuffer = new char[newCurrentCapacity + extraSizeAtBufferEnd];
            std::memcpy(pNewBuffer, m_pData, m_rightBlockSize);
            std::memcpy(pNewBuffer + m_rightBlockSize, m_pBuffer, m_leftBlockSize);
            delete [] m_pBuffer;
            m_pBuffer = pNewBuffer;
            m_pData = pNewBuffer;
            m_currentCapacity = newCurrentCapacity;
            m_rightBlockSize += m_leftBlockSize;
            m_leftBlockSize = 0;
            m_spaceAvailableAtRightSide = m_currentCapacity - m_rightBlockSize;
            return true;
        }
    }
}

void RingBuffer::clear()
{
    if (m_currentCapacity > defaultCapacity())
    {
        m_currentCapacity = defaultCapacity();
        delete [] m_pBuffer;
        m_pBuffer = new char[m_currentCapacity + extraSizeAtBufferEnd];
    }
    m_pData = m_pBuffer;
    m_rightBlockSize = 0;
    m_spaceAvailableAtRightSide = m_currentCapacity;
    m_leftBlockSize = 0;
}

bool RingBuffer::reset()
{
    if (isEmpty())
    {
        clear();
        return true;
    }
    else
        return false;
}

bool RingBuffer::tryToEnlargeBuffer(size_t count)
{
    if (m_currentCapacity == m_capacity)
        return false;
    const auto newCurrentCapacityAsPowerOfTwo = std::bit_ceil<size_t>(m_currentCapacity + count);
    const auto newCurrentCapacity = (m_capacity > 0) ? std::min<size_t>(m_capacity, newCurrentCapacityAsPowerOfTwo) : newCurrentCapacityAsPowerOfTwo;
    char *pNewBuffer = new char[newCurrentCapacity + extraSizeAtBufferEnd];
    assert(newCurrentCapacity > m_currentCapacity);
    std::memcpy(pNewBuffer, m_pData, m_rightBlockSize);
    std::memcpy(pNewBuffer + m_rightBlockSize, m_pBuffer, m_leftBlockSize);
    delete [] m_pBuffer;
    m_pBuffer = pNewBuffer;
    m_pData = pNewBuffer;
    m_currentCapacity = newCurrentCapacity;
    m_rightBlockSize += m_leftBlockSize;
    m_leftBlockSize = 0;
    m_spaceAvailableAtRightSide = m_currentCapacity - m_rightBlockSize;
    return true;
}

}
