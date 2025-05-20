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

#ifndef KOURIER_RING_BUFFER_H
#define KOURIER_RING_BUFFER_H

#include "SDK.h"
#include <string_view>


namespace Kourier
{

class KOURIER_EXPORT DataSource
{
public:
    DataSource() = default;
    virtual ~DataSource() = default;
    virtual bool isFull() const {return false;}
    virtual bool needsToWrite() const {return false;}
    virtual size_t dataAvailable() const = 0;
    virtual size_t read(char *pBuffer, size_t count) = 0;
};

class KOURIER_EXPORT DataSink
{
public:
    DataSink() = default;
    virtual ~DataSink() = default;
    virtual bool hasPendingData() const {return false;}
    virtual bool needsToRead() const {return false;}
    virtual size_t write(const char *pData, size_t count) = 0;
};

class KOURIER_EXPORT RingBuffer
{
public:
    RingBuffer(size_t capacity = 0);
    ~RingBuffer();
    RingBuffer(const RingBuffer &) = delete;
    RingBuffer(RingBuffer &&) = delete;
    RingBuffer &operator=(const RingBuffer &) = delete;
    size_t read(char *pData, size_t maxSize);
    size_t read(DataSink &dataSink);
    size_t write(const char *pData, size_t maxSize);
    inline size_t write(std::string_view data) {return !data.empty() ? RingBuffer::write(data.data(), data.size()) : 0;}
    size_t write(DataSource &dataSource);
    inline char peekChar(size_t index) const {return (index < m_rightBlockSize) ? m_pData[index] : m_pBuffer[index - m_rightBlockSize];}
    std::string_view slice(size_t pos, size_t count);
    inline std::string_view peekAll() {return (!isEmpty() ? slice(0, size()) : std::string_view{});}
    inline std::string_view readAll()
    {
        auto data = peekAll();
        m_pData = m_pBuffer;
        m_rightBlockSize = 0;
        m_spaceAvailableAtRightSide = m_currentCapacity;
        m_leftBlockSize = 0;
        return data;
    }
    size_t popFront(size_t maxSize);
    inline bool isEmpty() const {return size() == 0;}
    inline bool isFull() const {return (m_capacity > 0) && (size() == m_capacity);}
    inline size_t size() const {return m_rightBlockSize + m_leftBlockSize;}
    inline size_t capacity() const {return m_capacity;}
    inline size_t availableFreeSize() const {return m_currentCapacity - size();}
    bool setCapacity(size_t capacity);
    void clear();
    bool reset();
    static constexpr size_t defaultCapacity() {return 128;}

private:
    bool tryToEnlargeBuffer(size_t count);

private:
    char *m_pBuffer = nullptr;
    char *m_pData = nullptr;
    size_t m_rightBlockSize = 0;
    size_t m_spaceAvailableAtRightSide = 0;
    size_t m_leftBlockSize = 0;
    size_t m_currentCapacity = 0;
    size_t m_capacity = 0;
    friend class SimdIterator;
};

}

#endif // KOURIER_RING_BUFFER_H
