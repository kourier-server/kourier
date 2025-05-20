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

#include "IOChannel.h"


namespace Kourier
{

/*!
\class Kourier::IOChannel
\brief The IOChannel class represents a buffered input/output channel.

IOChannel is the base class of [TcpSocket](@ref Kourier::TcpSocket) and [TlsSocket](@ref Kourier::TlsSocket).
IOChannel uses internal buffers to store the data that has to be written to the channel and the data that was read from it.

IOChannel emits the [receivedData](@ref Kourier::IOChannel::receivedData) signal whenever it reads a new data payload from the channel.
To prevent reading too much data, in the IOChannel's constructor, you can set the capacity of the internal read buffer that IOChannel
uses to store data from the channel.

Writing to IOChannel never fails, as IOChannel grows the internal write buffer it uses to store data to be written to the channel.
IOChannel emits the [sentData](@ref Kourier::IOChannel::sentData) signal whenever it writes to the channel data from its internal
buffer. You can use the [sentData](@ref Kourier::IOChannel::sentData) signal to adjust data writing according to the channel
bandwidth to prevent too much memory usage.
*/

/*!
Creates the IOChannel. You can use \a readBufferCapacity to limit the capacity of the read buffer IOChannel uses to store
data read from the channel. The read buffer can grow up to \a readBufferCapacity if it is positive. If \a readBufferCapacity
is zero, no limit is set on how much the read buffer can grow.
*/
IOChannel::IOChannel(const size_t readBufferCapacity)
    : m_readBuffer(readBufferCapacity)
{
}

/*!
 \fn IOChannel::~IOChannel
 Destroys the object.
*/

/*!
 \fn IOChannel::dataAvailable()
 returns the size of the data IOChannel received from the channel and is available for reading.
*/

/*!
 \fn IOChannel::dataToWrite()
 returns the size of the data IOChannel has to write to the channel.
*/

/*!
 \fn IOChannel::peekChar(size_t index)
 returns the char at \a index position on the read buffer. \a index must be at most \link IOChannel::dataAvailable() dataAvailable()\endlink - 1.
*/

/*!
 \fn IOChannel::slice(size_t pos, size_t count)
 returns a slice of \a count bytes of data on the read buffer starting at \a pos. Writing to IOChannel after calling this method invalidates the returned data.
*/

/*!
 \fn IOChannel::peekAll()
 returns all data in the read buffer without removing it from the buffer. The returned string view becomes invalid after you write
 to the IOChannel.
*/

/*!
 \fn IOChannel::readAll()
 returns all data in the read buffer. Writing to IOChannel after calling this method invalidates the returned data.
*/

/*!
 \fn IOChannel::skip(size_t maxSize)
 removes up to \a maxSize from the beginning of the read buffer. Returns the number of bytes removed.
*/

/*!
 \fn IOChannel::read(char *pBuffer, size_t maxSize)
 reads up to \a maxSize from the read buffer into the buffer pointed by \a pBuffer. Returns the number of bytes read from the read buffer.
*/

/*!
 \fn IOChannel::write(const char *pData, size_t count)
 writes \a count bytes from the buffer pointed by \a pData into the write buffer.
*/

/*!
 \fn IOChannel::write(std::string_view data)
 writes \a data into the write buffer.
*/

/*!
 \fn IOChannel::readBufferCapacity()
 returns the read buffer capacity. A value of zero means that capacity is not limited. If the returned value is positive,
 the read buffer can grow up to the returned value.
*/

/*!
 \fn IOChannel::setReadBufferCapacity(size_t capacity)
 Sets the read buffer capacity. A value of zero means that capacity is not limited. The read buffer can grow to the given value
 if \a capacity is positive. Returns true if the capacity was successfully changed. Setting capacity can fail because this
 method does not delete data that is available to be read. Thus, this method fails if \a capacity is smaller than the data available
 in the read buffer.
*/

/*!
 \fn IOChannel::clear()
 clears data in read/write buffers and restores them to their initial states. This method preserves the read buffer's
 capacity.
*/

/*!
 \fn IOChannel::reset()
 restores internal read/write buffers to their initial capacity. Buffers must be empty to be restored. Returns true if
both read and write buffers were restored.
*/

/*!
 \fn IOChannel::sentData(size_t count)
 IOChannel emits the sentData signal when data is written to the channel. \a count informs how many bytes were written.
 [dataToWrite](@ref Kourier::IOChannel::dataToWrite) informs how many bytes are still pending to be written to the channel.
*/

/*!
 \fn IOChannel::receivedData()
 IOChannel emits receivedData when data is read from the channel.
 [dataAvailable](@ref Kourier::IOChannel::dataAvailable) informs how many bytes are available for reading.
*/

size_t IOChannel::readDataFromChannel()
{
    const auto dataRead = m_readBuffer.write(dataSource());
    setReadChannelNotificationEnabled(!m_readBuffer.isFull());
    return dataRead;
}

size_t IOChannel::writeDataToChannel()
{
    const auto dataSent = m_writeBuffer.read(dataSink());
    setWriteChannelNotificationEnabled(!m_writeBuffer.isEmpty());
    return dataSent;
}

Signal IOChannel::sentData(size_t count) KOURIER_SIGNAL(&IOChannel::sentData, count)
Signal IOChannel::receivedData() KOURIER_SIGNAL(&IOChannel::receivedData)

}
