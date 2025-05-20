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

#ifndef KOURIER_IO_CHANNEL_H
#define KOURIER_IO_CHANNEL_H

#include "RingBuffer.h"
#include "Object.h"


namespace Kourier
{

class KOURIER_EXPORT IOChannel : public Object
{
KOURIER_OBJECT(Kourier::IOChannel)
public:
    IOChannel(const size_t readBufferCapacity = 0);
    ~IOChannel() override = default;
    virtual size_t dataAvailable() const {return m_readBuffer.size();}
    virtual size_t dataToWrite() const {return m_writeBuffer.size();}
    inline char peekChar(size_t index) const {return m_readBuffer.peekChar(index);}
    inline std::string_view slice(size_t pos, size_t count) {return m_readBuffer.slice(pos, count);}
    inline std::string_view peekAll() {return m_readBuffer.peekAll();}
    virtual std::string_view readAll()
    {
        const bool hasToEnableReadNotification = m_readBuffer.isFull();
        auto data = m_readBuffer.readAll();
        if (hasToEnableReadNotification)
            setReadChannelNotificationEnabled(true);
        return data;
    }
    virtual size_t skip(size_t maxSize)
    {
        const bool isFull = m_readBuffer.isFull();
        const auto poppedBytes = m_readBuffer.popFront(maxSize);
        setReadChannelNotificationEnabled((poppedBytes > 0) || !isFull);
        return poppedBytes;
    }
    virtual size_t read(char *pBuffer, size_t maxSize)
    {
        assert(pBuffer != nullptr && maxSize >= 0);
        const bool isFull = m_readBuffer.isFull();
        const auto bytesRead = m_readBuffer.read(pBuffer, maxSize);
        setReadChannelNotificationEnabled((bytesRead > 0) || !isFull);
        return bytesRead;
    }
    virtual size_t write(const char *pData, size_t count)
    {
        assert(pData != nullptr);
        size_t bytesWritten = 0;
        if (m_writeBuffer.isEmpty())
            bytesWritten = dataSink().write(pData, count);
        if (bytesWritten < count)
            m_writeBuffer.write(pData + bytesWritten, count - bytesWritten);
        setWriteChannelNotificationEnabled(!m_writeBuffer.isEmpty());
        return count;
    }
    inline size_t write(std::string_view data) {return !data.empty() ? write(data.data(), data.size()) : 0;}
    inline size_t readBufferCapacity() const {return m_readBuffer.capacity();}
    inline bool setReadBufferCapacity(size_t capacity) {return m_readBuffer.setCapacity(capacity);}
    inline void clear() {m_readBuffer.clear(); m_writeBuffer.clear(); m_isReadNotificationEnabled = true; m_isWriteNotificationEnabled = true;}
    inline bool reset() {return m_readBuffer.reset() && m_writeBuffer.reset();}
    Signal sentData(size_t count);
    Signal receivedData();

protected:
    virtual size_t readDataFromChannel();
    virtual size_t writeDataToChannel();
    virtual DataSource &dataSource() = 0;
    virtual DataSink &dataSink() = 0;
    virtual void onReadNotificationChanged() = 0;
    inline void setReadChannelNotificationEnabled(bool enabled)
    {
        if (m_isReadNotificationEnabled != enabled)
        {
            m_isReadNotificationEnabled = enabled;
            onReadNotificationChanged();
        }
    }
    inline bool isReadNotificationEnabled() const {return m_isReadNotificationEnabled;}
    virtual void onWriteNotificationChanged() = 0;
    inline void setWriteChannelNotificationEnabled(bool enabled)
    {
        if (m_isWriteNotificationEnabled != enabled)
        {
            m_isWriteNotificationEnabled = enabled;
            onWriteNotificationChanged();
        }
    }
    inline bool isWriteNotificationEnabled() const {return m_isWriteNotificationEnabled;}

protected:
    RingBuffer m_readBuffer;
    RingBuffer m_writeBuffer;
    bool m_isReadNotificationEnabled = true;
    bool m_isWriteNotificationEnabled = true;
    friend class SimdIterator;
};

}

#endif // KOURIER_IO_CHANNEL_H
