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

#include "IOChannel.h"
#include <QRandomGenerator64>
#include <Spectator.h>


using Kourier::RingBuffer;
using Kourier::DataSource;
using Kourier::DataSink;
using Kourier::IOChannel;
using Kourier::Object;


namespace Test::IOChannel
{

class DataSourceTest : public DataSource
{
public:
    DataSourceTest() = default;
    ~DataSourceTest() override = default;
    size_t dataAvailable() const override {return m_data.size() - m_fetchedSize;}
    size_t read(char *pBuffer, size_t count) override
    {
        const auto acceptableSize = std::min<size_t>(m_data.size() - m_fetchedSize, count);
        std::memcpy(pBuffer, m_data.data() + m_fetchedSize, acceptableSize);
        m_fetchedSize += acceptableSize;
        return acceptableSize;
    }
    size_t fetchedSize() const {return m_fetchedSize;}
    void addDataToChannel(std::string_view channelData) {m_data.append(channelData);}
    std::string_view data() const {return ((!m_data.empty() && m_fetchedSize < m_data.size()) ? std::string_view(m_data.data() + m_fetchedSize, m_data.size() - m_fetchedSize) : std::string_view{});}

private:
    size_t m_fetchedSize = 0;
    std::string m_data;
};

class DataSinkTest : public DataSink
{
public:
    DataSinkTest() = default;
    ~DataSinkTest() override = default;
    size_t write(const char *pData, size_t count) override
    {
        const auto acceptableSize = std::min<size_t>(m_capacity, count);
        m_capacity -= acceptableSize;
        m_data.append(pData, acceptableSize);
        return acceptableSize;
    }
    std::string_view data() const {return m_data;}
    size_t capacity() const {return m_capacity;}
    void addCapacity(size_t capacity) {m_capacity += capacity;}

private:
    size_t m_capacity = 0;
    std::string m_data;
};


class IOChannelTest : public Kourier::IOChannel
{
public:
    IOChannelTest() = default;
    ~IOChannelTest() override = default;
    RingBuffer &readBuffer() {return m_readBuffer;}
    RingBuffer &writeBuffer() {return m_writeBuffer;}
    bool &isReadNotificationEnabled() {return m_isReadNotificationEnabled;}
    bool &isWriteNotificationEnabled() {return m_isWriteNotificationEnabled;}
    DataSourceTest &dataSourceTest() {return m_dataSource;}
    DataSinkTest &dataSinkTest() {return m_dataSink;}
    void writeToChannel() {IOChannel::writeDataToChannel();}
    void readFromChannel() {IOChannel::readDataFromChannel();}

private:
    DataSource &dataSource() override {return m_dataSource;}
    DataSink &dataSink() override {return m_dataSink;}
    void onReadNotificationChanged() override {}
    void onWriteNotificationChanged() override {}

private:
    DataSourceTest m_dataSource;
    DataSinkTest m_dataSink;
};

}

using namespace Test::IOChannel;


SCENARIO("IOChannel supports data writing")
{
    GIVEN("a sink with unlimited capacity and an IOChannel without any data in its write buffer")
    {
        IOChannelTest ioChannel;
        REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
        REQUIRE(ioChannel.isReadNotificationEnabled() == true);
        REQUIRE(ioChannel.dataToWrite() == 0);
        REQUIRE(ioChannel.dataAvailable() == 0);
        REQUIRE(ioChannel.readBufferCapacity() == 0)
        REQUIRE(ioChannel.writeBuffer().isEmpty());
        ioChannel.dataSinkTest().addCapacity(std::numeric_limits<size_t>::max()/2);

        WHEN("data is written to IOChannel")
        {
            const auto dataSize = GENERATE(AS(size_t), 0, 1, 16, 23);
            std::string data(dataSize, ' ');
            for (auto &ch : data)
                ch = QRandomGenerator64::global()->bounded(0, 256);
            const auto isWriteNotificationEnabled = GENERATE(AS(bool), true, false);
            ioChannel.isWriteNotificationEnabled() = isWriteNotificationEnabled;
            const auto dataWritten = ioChannel.write(data.data(), data.size());

            THEN("IOChannel writes all data to sink and disables write notification")
            {
                REQUIRE(dataWritten == dataSize);
                REQUIRE(ioChannel.dataSinkTest().data() == data);
                REQUIRE(ioChannel.writeBuffer().isEmpty());
                REQUIRE(ioChannel.dataToWrite() == 0);
                REQUIRE(ioChannel.isWriteNotificationEnabled() == false);
            }
        }
    }

    GIVEN("a sink with no capacity and an IOChannel without any data in its write buffer")
    {
        IOChannelTest ioChannel;
        REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
        REQUIRE(ioChannel.isReadNotificationEnabled() == true);
        REQUIRE(ioChannel.dataToWrite() == 0);
        REQUIRE(ioChannel.dataAvailable() == 0);
        REQUIRE(ioChannel.readBufferCapacity() == 0)
        REQUIRE(ioChannel.writeBuffer().isEmpty());

        WHEN("data is written to IOChannel")
        {
            const auto dataSize = GENERATE(AS(size_t), 1, 16, 23);
            std::string data(dataSize, ' ');
            for (auto &ch : data)
                ch = QRandomGenerator64::global()->bounded(0, 256);
            const auto isWriteNotificationEnabled = GENERATE(AS(bool), true, false);
            ioChannel.isWriteNotificationEnabled() = isWriteNotificationEnabled;
            const auto dataWritten = ioChannel.write(data.data(), data.size());

            THEN("IOChannel writes data to write buffer and enables write notification")
            {
                REQUIRE(dataWritten == dataSize);
                REQUIRE(ioChannel.dataSinkTest().data().empty());
                REQUIRE(!ioChannel.writeBuffer().isEmpty());
                REQUIRE(ioChannel.writeBuffer().size() == dataSize);
                REQUIRE(ioChannel.writeBuffer().peekAll() == data);
                REQUIRE(ioChannel.dataToWrite() == dataSize);
                REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
            }
        }

        WHEN("data with zero size is written to IOChannel")
        {
            const size_t dataSize = 0;
            std::string data(dataSize, ' ');
            for (auto &ch : data)
                ch = QRandomGenerator64::global()->bounded(0, 256);
            const auto isWriteNotificationEnabled = GENERATE(AS(bool), true, false);
            ioChannel.isWriteNotificationEnabled() = isWriteNotificationEnabled;
            const auto dataWritten = ioChannel.write(data.data(), data.size());

            THEN("IOChannel writes no data to write buffer and disables write notification")
            {
                REQUIRE(dataWritten == dataSize);
                REQUIRE(ioChannel.dataSinkTest().data().empty());
                REQUIRE(ioChannel.writeBuffer().isEmpty());
                REQUIRE(ioChannel.dataToWrite() == 0);
                REQUIRE(ioChannel.isWriteNotificationEnabled() == false);
            }
        }
    }

    GIVEN("a sink with smaller capacity than the data to be written and an IOChannel without any data in its write buffer")
    {
        IOChannelTest ioChannel;
        REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
        REQUIRE(ioChannel.isReadNotificationEnabled() == true);
        REQUIRE(ioChannel.dataToWrite() == 0);
        REQUIRE(ioChannel.dataAvailable() == 0);
        REQUIRE(ioChannel.readBufferCapacity() == 0)
        REQUIRE(ioChannel.writeBuffer().isEmpty());

        WHEN("data is written to IOChannel")
        {
            const auto dataSize = GENERATE(AS(size_t), 16, 23, 35);
            const auto lackingCapacityInSink = GENERATE(AS(size_t), 1, 4, 7);
            REQUIRE(dataSize > lackingCapacityInSink);
            const auto initialSinkCapacity = dataSize - lackingCapacityInSink;
            REQUIRE(ioChannel.dataSinkTest().capacity() == 0);
            ioChannel.dataSinkTest().addCapacity(initialSinkCapacity);
            REQUIRE(ioChannel.dataSinkTest().capacity() == initialSinkCapacity);
            std::string data(dataSize, ' ');
            for (auto &ch : data)
                ch = QRandomGenerator64::global()->bounded(0, 256);
            const auto isWriteNotificationEnabled = GENERATE(AS(bool), true, false);
            ioChannel.isWriteNotificationEnabled() = isWriteNotificationEnabled;
            const auto dataWritten = ioChannel.write(data.data(), data.size());

            THEN("IOChannel fills sink up to its capacity, writes exceeding data to write buffer and enables write notification")
            {
                REQUIRE(dataWritten == dataSize);
                REQUIRE(ioChannel.dataSinkTest().capacity() == 0);
                REQUIRE(ioChannel.dataSinkTest().data() == std::string(data.data(), initialSinkCapacity));
                REQUIRE(!ioChannel.writeBuffer().isEmpty());
                REQUIRE(ioChannel.writeBuffer().peekAll() == std::string_view(data.data() + initialSinkCapacity, data.size() - initialSinkCapacity));
                REQUIRE(ioChannel.dataToWrite() == lackingCapacityInSink);
                REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
            }
        }
    }

    GIVEN("a sink with unlimited capacity and an IOChannel with some data in its write buffer")
    {
        IOChannelTest ioChannel;
        REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
        REQUIRE(ioChannel.isReadNotificationEnabled() == true);
        REQUIRE(ioChannel.dataToWrite() == 0);
        REQUIRE(ioChannel.dataAvailable() == 0);
        REQUIRE(ioChannel.readBufferCapacity() == 0)
        REQUIRE(ioChannel.writeBuffer().isEmpty());
        const auto initialDataInWriteBuffer = GENERATE(AS(std::string_view), "aeiou", "1234", "a");
        REQUIRE(initialDataInWriteBuffer.size() == ioChannel.writeBuffer().write(initialDataInWriteBuffer.data(), initialDataInWriteBuffer.size()));
        ioChannel.dataSinkTest().addCapacity(std::numeric_limits<size_t>::max()/2);

        WHEN("data is written to IOChannel")
        {
            const auto dataSize = GENERATE(AS(size_t), 0, 1, 16, 23);
            std::string data(dataSize, ' ');
            for (auto &ch : data)
                ch = QRandomGenerator64::global()->bounded(0, 256);
            const auto isWriteNotificationEnabled = GENERATE(AS(bool), true, false);
            ioChannel.isWriteNotificationEnabled() = isWriteNotificationEnabled;
            const auto dataWritten = ioChannel.write(data.data(), data.size());

            THEN("IOChannel appends written data to write buffer and enables write notification")
            {
                REQUIRE(dataWritten == dataSize);
                REQUIRE(ioChannel.dataSinkTest().data().empty());
                REQUIRE(!ioChannel.writeBuffer().isEmpty());
                REQUIRE(ioChannel.writeBuffer().peekAll() == (std::string(initialDataInWriteBuffer) + data));
                REQUIRE(ioChannel.dataToWrite() == (initialDataInWriteBuffer.size() + dataSize));
                REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
            }
        }
    }

    GIVEN("a sink with no capacity and an IOChannel with some data in its write buffer")
    {
        IOChannelTest ioChannel;
        REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
        REQUIRE(ioChannel.isReadNotificationEnabled() == true);
        REQUIRE(ioChannel.dataToWrite() == 0);
        REQUIRE(ioChannel.dataAvailable() == 0);
        REQUIRE(ioChannel.readBufferCapacity() == 0)
        REQUIRE(ioChannel.writeBuffer().isEmpty());
        const auto initialDataInWriteBuffer = GENERATE(AS(std::string_view), "aeiou", "1234", "a");
        REQUIRE(initialDataInWriteBuffer.size() == ioChannel.writeBuffer().write(initialDataInWriteBuffer.data(), initialDataInWriteBuffer.size()));

        WHEN("data is written to IOChannel")
        {
            const auto dataSize = GENERATE(AS(size_t), 1, 16, 23);
            std::string data(dataSize, ' ');
            for (auto &ch : data)
                ch = QRandomGenerator64::global()->bounded(0, 256);
            const auto isWriteNotificationEnabled = GENERATE(AS(bool), true, false);
            ioChannel.isWriteNotificationEnabled() = isWriteNotificationEnabled;
            const auto dataWritten = ioChannel.write(data.data(), data.size());

            THEN("IOChannel appends written data to write buffer and enables write notification")
            {
                REQUIRE(dataWritten == dataSize);
                REQUIRE(ioChannel.dataSinkTest().data().empty());
                REQUIRE(!ioChannel.writeBuffer().isEmpty());
                REQUIRE(ioChannel.writeBuffer().size() == (initialDataInWriteBuffer.size() + dataSize));
                REQUIRE(ioChannel.writeBuffer().peekAll() == (std::string(initialDataInWriteBuffer) + data));
                REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
            }
        }

        WHEN("data with zero size is written to IOChannel")
        {
            const size_t dataSize = 0;
            std::string data(dataSize, ' ');
            for (auto &ch : data)
                ch = QRandomGenerator64::global()->bounded(0, 256);
            const auto isWriteNotificationEnabled = GENERATE(AS(bool), true, false);
            ioChannel.isWriteNotificationEnabled() = isWriteNotificationEnabled;
            const auto dataWritten = ioChannel.write(data.data(), data.size());

            THEN("IOChannel appends no data to write buffer and enables write notification")
            {
                REQUIRE(dataWritten == dataSize);
                REQUIRE(ioChannel.dataSinkTest().data().empty());
                REQUIRE(!ioChannel.writeBuffer().isEmpty());
                REQUIRE(ioChannel.writeBuffer().size() == initialDataInWriteBuffer.size());
                REQUIRE(ioChannel.writeBuffer().peekAll() == initialDataInWriteBuffer);
                REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
            }
        }
    }

    GIVEN("a sink with smaller capacity than the data to be written and an IOChannel with some data in its write buffer")
    {
        IOChannelTest ioChannel;
        REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
        REQUIRE(ioChannel.isReadNotificationEnabled() == true);
        REQUIRE(ioChannel.dataToWrite() == 0);
        REQUIRE(ioChannel.dataAvailable() == 0);
        REQUIRE(ioChannel.readBufferCapacity() == 0)
        REQUIRE(ioChannel.writeBuffer().isEmpty());
        const auto initialDataInWriteBuffer = GENERATE(AS(std::string_view), "aeiou", "1234", "a");
        REQUIRE(initialDataInWriteBuffer.size() == ioChannel.writeBuffer().write(initialDataInWriteBuffer.data(), initialDataInWriteBuffer.size()));

        WHEN("data is written to IOChannel")
        {
            const auto dataSize = GENERATE(AS(size_t), 16, 23, 35);
            const auto lackingCapacityInSink = GENERATE(AS(size_t), 1, 4, 7);
            const auto initialSinkCapacity = dataSize - lackingCapacityInSink;
            REQUIRE(ioChannel.dataSinkTest().capacity() == 0);
            ioChannel.dataSinkTest().addCapacity(initialSinkCapacity);
            REQUIRE(ioChannel.dataSinkTest().capacity() == initialSinkCapacity);
            std::string data(dataSize, ' ');
            for (auto &ch : data)
                ch = QRandomGenerator64::global()->bounded(0, 256);
            const auto isWriteNotificationEnabled = GENERATE(AS(bool), true, false);
            ioChannel.isWriteNotificationEnabled() = isWriteNotificationEnabled;
            const auto dataWritten = ioChannel.write(data.data(), data.size());

            THEN("IOChannel appends written data to write buffer and enables write notification")
            {
                REQUIRE(dataWritten == dataSize);
                REQUIRE(ioChannel.dataSinkTest().data().empty());
                REQUIRE(!ioChannel.writeBuffer().isEmpty());
                REQUIRE(ioChannel.writeBuffer().size() == (initialDataInWriteBuffer.size() + dataSize));
                REQUIRE(ioChannel.writeBuffer().peekAll() == (std::string(initialDataInWriteBuffer) + data));
                REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
            }
        }
    }
}


SCENARIO("IOChannel writes data to channel")
{
    GIVEN("a sink with capacity to ingest all the data in IOChannel write buffer")
    {
        IOChannelTest ioChannel;
        REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
        REQUIRE(ioChannel.isReadNotificationEnabled() == true);
        REQUIRE(ioChannel.dataToWrite() == 0);
        REQUIRE(ioChannel.dataAvailable() == 0);
        REQUIRE(ioChannel.readBufferCapacity() == 0)
        REQUIRE(ioChannel.writeBuffer().isEmpty());
        const auto initialDataInWriteBuffer = GENERATE(AS(std::string_view), "aeiou", "1234", "a");
        REQUIRE(initialDataInWriteBuffer.size() == ioChannel.writeBuffer().write(initialDataInWriteBuffer.data(), initialDataInWriteBuffer.size()));
        REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
        REQUIRE(ioChannel.isReadNotificationEnabled() == true);
        REQUIRE(ioChannel.dataToWrite() == initialDataInWriteBuffer.size());
        REQUIRE(ioChannel.dataAvailable() == 0);
        REQUIRE(ioChannel.readBufferCapacity() == 0)
        REQUIRE(ioChannel.writeBuffer().peekAll() == initialDataInWriteBuffer);
        const auto extraCapacityInSink = GENERATE(AS(size_t), 0, 3, 18);
        ioChannel.dataSinkTest().addCapacity(initialDataInWriteBuffer.size() + extraCapacityInSink);

        WHEN("IOChannel writes data to channel")
        {
            const auto isWriteNotificationEnabled = GENERATE(AS(bool), true, false);
            ioChannel.isWriteNotificationEnabled() = isWriteNotificationEnabled;
            ioChannel.writeToChannel();

            THEN("IOChannel writes all data to channel and disables write notification")
            {
                REQUIRE(ioChannel.isReadNotificationEnabled() == true);
                REQUIRE(ioChannel.dataToWrite() == 0);
                REQUIRE(ioChannel.dataAvailable() == 0);
                REQUIRE(ioChannel.readBufferCapacity() == 0)
                REQUIRE(ioChannel.writeBuffer().isEmpty());
                REQUIRE(ioChannel.dataSinkTest().data() == initialDataInWriteBuffer);
                REQUIRE(ioChannel.isWriteNotificationEnabled() == false);
            }
        }
    }

    GIVEN("a sink with no capacity and an IOChannel with some data in write buffer")
    {
        IOChannelTest ioChannel;
        REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
        REQUIRE(ioChannel.isReadNotificationEnabled() == true);
        REQUIRE(ioChannel.dataToWrite() == 0);
        REQUIRE(ioChannel.dataAvailable() == 0);
        REQUIRE(ioChannel.readBufferCapacity() == 0)
        REQUIRE(ioChannel.writeBuffer().isEmpty());
        const auto initialDataInWriteBuffer = GENERATE(AS(std::string_view), "aeiou", "1234", "a");
        REQUIRE(initialDataInWriteBuffer.size() == ioChannel.writeBuffer().write(initialDataInWriteBuffer.data(), initialDataInWriteBuffer.size()));
        REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
        REQUIRE(ioChannel.isReadNotificationEnabled() == true);
        REQUIRE(ioChannel.dataToWrite() == initialDataInWriteBuffer.size());
        REQUIRE(ioChannel.dataAvailable() == 0);
        REQUIRE(ioChannel.readBufferCapacity() == 0)
        REQUIRE(ioChannel.writeBuffer().peekAll() == initialDataInWriteBuffer);

        WHEN("IOChannel writes data to channel")
        {
            const auto isWriteNotificationEnabled = GENERATE(AS(bool), true, false);
            ioChannel.isWriteNotificationEnabled() = isWriteNotificationEnabled;
            ioChannel.writeToChannel();

            THEN("IOChannel writes no data to channel and enables write notification")
            {
                REQUIRE(ioChannel.isReadNotificationEnabled() == true);
                REQUIRE(ioChannel.dataToWrite() == initialDataInWriteBuffer.size());
                REQUIRE(ioChannel.dataAvailable() == 0);
                REQUIRE(ioChannel.readBufferCapacity() == 0)
                REQUIRE(ioChannel.writeBuffer().peekAll() == initialDataInWriteBuffer);
                REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
            }
        }
    }

    GIVEN("a sink with smaller capacity than the data in IOChannel write buffer")
    {
        IOChannelTest ioChannel;
        REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
        REQUIRE(ioChannel.isReadNotificationEnabled() == true);
        REQUIRE(ioChannel.dataToWrite() == 0);
        REQUIRE(ioChannel.dataAvailable() == 0);
        REQUIRE(ioChannel.readBufferCapacity() == 0)
        REQUIRE(ioChannel.writeBuffer().isEmpty());
        const auto initialDataInWriteBuffer = GENERATE(AS(std::string_view), "asdf qwer", "1234 5678 9", "Hello World");
        REQUIRE(initialDataInWriteBuffer.size() == ioChannel.writeBuffer().write(initialDataInWriteBuffer.data(), initialDataInWriteBuffer.size()));
        REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
        REQUIRE(ioChannel.isReadNotificationEnabled() == true);
        REQUIRE(ioChannel.dataToWrite() == initialDataInWriteBuffer.size());
        REQUIRE(ioChannel.dataAvailable() == 0);
        REQUIRE(ioChannel.readBufferCapacity() == 0)
        REQUIRE(ioChannel.writeBuffer().peekAll() == initialDataInWriteBuffer);
        const auto lackingCapacityInSink = GENERATE(AS(size_t), 1, 4, 7);
        REQUIRE(initialDataInWriteBuffer.size() > lackingCapacityInSink);
        const auto initialSinkCapacity = initialDataInWriteBuffer.size() - lackingCapacityInSink;
        REQUIRE(ioChannel.dataSinkTest().capacity() == 0);
        ioChannel.dataSinkTest().addCapacity(initialSinkCapacity);
        REQUIRE(ioChannel.dataSinkTest().capacity() == initialSinkCapacity);

        WHEN("IOChannel writes data to channel")
        {
            const auto isWriteNotificationEnabled = GENERATE(AS(bool), true, false);
            ioChannel.isWriteNotificationEnabled() = isWriteNotificationEnabled;
            ioChannel.writeToChannel();

            THEN("IOChannel fills channel up to sink's capacity and enables write notification")
            {
                REQUIRE(ioChannel.dataSinkTest().capacity() == 0);
                REQUIRE(ioChannel.dataSinkTest().data() == std::string_view(initialDataInWriteBuffer.data(), initialSinkCapacity));
                REQUIRE(ioChannel.isReadNotificationEnabled() == true);
                REQUIRE(ioChannel.dataToWrite() == lackingCapacityInSink);
                REQUIRE(ioChannel.writeBuffer().size() == lackingCapacityInSink);
                REQUIRE(ioChannel.dataAvailable() == 0);
                REQUIRE(ioChannel.readBufferCapacity() == 0)
                REQUIRE(!ioChannel.writeBuffer().isEmpty());
                REQUIRE(ioChannel.writeBuffer().peekAll() == std::string_view(initialDataInWriteBuffer.data() + initialSinkCapacity, lackingCapacityInSink));
                REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
            }
        }
    }

    GIVEN("a sink with some capacity and an IOChannel with an empty write buffer")
    {
        IOChannelTest ioChannel;
        REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
        REQUIRE(ioChannel.isReadNotificationEnabled() == true);
        REQUIRE(ioChannel.dataToWrite() == 0);
        REQUIRE(ioChannel.dataAvailable() == 0);
        REQUIRE(ioChannel.readBufferCapacity() == 0)
        REQUIRE(ioChannel.writeBuffer().isEmpty());
        const auto sinkCapacity = GENERATE(AS(size_t), 1, 3, 18);
        ioChannel.dataSinkTest().addCapacity(sinkCapacity);

        WHEN("IOChannel writes data to channel")
        {
            const auto isWriteNotificationEnabled = GENERATE(AS(bool), true, false);
            ioChannel.isWriteNotificationEnabled() = isWriteNotificationEnabled;
            ioChannel.writeToChannel();

            THEN("IOChannel writes no data to channel and disables write notification")
            {
                REQUIRE(ioChannel.dataSinkTest().capacity() == sinkCapacity);
                REQUIRE(ioChannel.dataSinkTest().data().empty());
                REQUIRE(ioChannel.isReadNotificationEnabled() == true);
                REQUIRE(ioChannel.dataToWrite() == 0);
                REQUIRE(ioChannel.dataAvailable() == 0);
                REQUIRE(ioChannel.readBufferCapacity() == 0)
                REQUIRE(ioChannel.writeBuffer().isEmpty());
                REQUIRE(ioChannel.isWriteNotificationEnabled() == false);
            }
        }
    }

    GIVEN("a sink with no capacity and an IOChannel with an empty write buffer")
    {
        IOChannelTest ioChannel;
        REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
        REQUIRE(ioChannel.isReadNotificationEnabled() == true);
        REQUIRE(ioChannel.dataToWrite() == 0);
        REQUIRE(ioChannel.dataAvailable() == 0);
        REQUIRE(ioChannel.readBufferCapacity() == 0)
        REQUIRE(ioChannel.writeBuffer().isEmpty());

        WHEN("IOChannel writes data to channel")
        {
            const auto isWriteNotificationEnabled = GENERATE(AS(bool), true, false);
            ioChannel.isWriteNotificationEnabled() = isWriteNotificationEnabled;
            ioChannel.writeToChannel();

            THEN("IOChannel writes no data to channel and disables write notification")
            {
                REQUIRE(ioChannel.dataSinkTest().capacity() == 0);
                REQUIRE(ioChannel.dataSinkTest().data().empty());
                REQUIRE(ioChannel.isReadNotificationEnabled() == true);
                REQUIRE(ioChannel.dataToWrite() == 0);
                REQUIRE(ioChannel.dataAvailable() == 0);
                REQUIRE(ioChannel.readBufferCapacity() == 0)
                REQUIRE(ioChannel.writeBuffer().isEmpty());
                REQUIRE(ioChannel.isWriteNotificationEnabled() == false);
            }
        }
    }
}


SCENARIO("IOChannel supports data reading")
{
    GIVEN("an IOChannel with an empty read buffer")
    {
        IOChannelTest ioChannel;
        REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
        REQUIRE(ioChannel.isReadNotificationEnabled() == true);
        REQUIRE(ioChannel.dataToWrite() == 0);
        REQUIRE(ioChannel.dataAvailable() == 0);
        REQUIRE(ioChannel.readBufferCapacity() == 0)
        REQUIRE(ioChannel.writeBuffer().isEmpty());
        REQUIRE(ioChannel.peekAll().empty());
        REQUIRE(ioChannel.readAll().empty());
        REQUIRE(ioChannel.readBuffer().isEmpty());

        WHEN("we try to read zero bytes from IOChannel")
        {
            const auto isReadNotificationEnabled = GENERATE(AS(bool), true, false);
            ioChannel.isReadNotificationEnabled() = isReadNotificationEnabled;
            char buffer = 0;
            const auto readBytes = ioChannel.read(&buffer, 0);

            THEN("IOChannel reads no data from read buffer and enables read notification")
            {
                REQUIRE(readBytes == 0);
                REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
                REQUIRE(ioChannel.dataToWrite() == 0);
                REQUIRE(ioChannel.dataAvailable() == 0);
                REQUIRE(ioChannel.readBufferCapacity() == 0)
                REQUIRE(ioChannel.writeBuffer().isEmpty());
                REQUIRE(ioChannel.peekAll().empty());
                REQUIRE(ioChannel.readAll().empty());
                REQUIRE(ioChannel.readBuffer().isEmpty());
                REQUIRE(ioChannel.isReadNotificationEnabled() == true);
            }
        }

        WHEN("we try to read some data from IOChannel")
        {
            const auto isReadNotificationEnabled = GENERATE(AS(bool), true, false);
            ioChannel.isReadNotificationEnabled() = isReadNotificationEnabled;
            const auto dataSize = GENERATE(AS(size_t), 1, 3, 12);
            std::vector<char> data(dataSize, ' ');
            const auto readBytes = ioChannel.read(data.data(), data.size());

            THEN("IOChannel reads no data from read buffer and enables read notification")
            {
                REQUIRE(readBytes == 0);
                REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
                REQUIRE(ioChannel.isReadNotificationEnabled() == true);
                REQUIRE(ioChannel.dataToWrite() == 0);
                REQUIRE(ioChannel.dataAvailable() == 0);
                REQUIRE(ioChannel.readBufferCapacity() == 0)
                REQUIRE(ioChannel.writeBuffer().isEmpty());
                REQUIRE(ioChannel.peekAll().empty());
                REQUIRE(ioChannel.readAll().empty());
                REQUIRE(ioChannel.readBuffer().isEmpty());
                REQUIRE(ioChannel.isReadNotificationEnabled() == true);
            }
        }
    }

    GIVEN("an IOChannel with some data in read buffer")
    {
        IOChannelTest ioChannel;
        REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
        REQUIRE(ioChannel.isReadNotificationEnabled() == true);
        REQUIRE(ioChannel.dataToWrite() == 0);
        REQUIRE(ioChannel.dataAvailable() == 0);
        REQUIRE(ioChannel.readBufferCapacity() == 0)
        REQUIRE(ioChannel.readBuffer().isEmpty());
        REQUIRE(ioChannel.writeBuffer().isEmpty());
        REQUIRE(ioChannel.peekAll().empty());
        REQUIRE(ioChannel.readAll().empty());
        REQUIRE(ioChannel.readBuffer().isEmpty());
        const auto initialDataInReadBuffer = GENERATE(AS(std::string_view), "asdf qwer", "1234 5678 9", "Hello World");
        REQUIRE(initialDataInReadBuffer.size() == ioChannel.readBuffer().write(initialDataInReadBuffer));
        REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
        REQUIRE(ioChannel.isReadNotificationEnabled() == true);
        REQUIRE(ioChannel.dataToWrite() == 0);
        REQUIRE(ioChannel.dataAvailable() == initialDataInReadBuffer.size());
        REQUIRE(ioChannel.readBufferCapacity() == 0)
        REQUIRE(ioChannel.readBuffer().peekAll() == initialDataInReadBuffer);
        REQUIRE(!ioChannel.readBuffer().isEmpty() && !ioChannel.readBuffer().isFull());
        const auto isReadNotificationEnabled = GENERATE(AS(bool), true, false);
        ioChannel.isReadNotificationEnabled() = isReadNotificationEnabled;

        WHEN("we try to read zero bytes from IOChannel")
        {
            char buffer = 0;
            const auto readBytes = ioChannel.read(&buffer, 0);

            THEN("IOChannel reads no data from read buffer and enables read notification")
            {
                REQUIRE(readBytes == 0);
                REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
                REQUIRE(ioChannel.dataToWrite() == 0);
                REQUIRE(ioChannel.dataAvailable() == initialDataInReadBuffer.size());
                REQUIRE(ioChannel.readBufferCapacity() == 0)
                REQUIRE(!ioChannel.readBuffer().isEmpty());
                REQUIRE(!ioChannel.readBuffer().isFull());
                REQUIRE(ioChannel.readBuffer().peekAll() == initialDataInReadBuffer);
                REQUIRE(ioChannel.isReadNotificationEnabled() == true);
                REQUIRE(ioChannel.readAll() == initialDataInReadBuffer);
                REQUIRE(ioChannel.readBuffer().isEmpty());
                REQUIRE(!ioChannel.readBuffer().isFull());
                REQUIRE(ioChannel.peekAll().empty());
                REQUIRE(ioChannel.readAll().empty());
            }
        }

        WHEN("some data is read from IOChannel")
        {
            const auto dataSize = GENERATE(AS(size_t), 1, 3, 8);
            std::vector<char> data(dataSize, ' ');
            const auto readBytes = ioChannel.read(data.data(), data.size());

            THEN("IOChannel reads data from read buffer and enables read notification")
            {
                REQUIRE(readBytes == data.size());
                REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
                REQUIRE(ioChannel.dataToWrite() == 0);
                REQUIRE(ioChannel.dataAvailable() == (initialDataInReadBuffer.size() - dataSize));
                REQUIRE(ioChannel.readBufferCapacity() == 0)
                REQUIRE(!ioChannel.readBuffer().isEmpty());
                REQUIRE(!ioChannel.readBuffer().isFull());
                REQUIRE(ioChannel.readBuffer().peekAll() == std::string_view(initialDataInReadBuffer.data() + dataSize, initialDataInReadBuffer.size() - dataSize));
                REQUIRE(ioChannel.isReadNotificationEnabled() == true);
                REQUIRE(ioChannel.readAll() == std::string_view(initialDataInReadBuffer.data() + dataSize, initialDataInReadBuffer.size() - dataSize));
                REQUIRE(ioChannel.readBuffer().isEmpty());
                REQUIRE(!ioChannel.readBuffer().isFull());
                REQUIRE(ioChannel.peekAll().empty());
                REQUIRE(ioChannel.readAll().empty());
            }
        }

        WHEN("all data is read from IOChannel")
        {
            std::vector<char> data(initialDataInReadBuffer.size(), ' ');
            const auto readBytes = ioChannel.read(data.data(), data.size());

            THEN("IOChannel reads all data in read buffer and enables read notification")
            {
                REQUIRE(readBytes == data.size());
                REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
                REQUIRE(ioChannel.dataToWrite() == 0);
                REQUIRE(ioChannel.dataAvailable() == 0);
                REQUIRE(ioChannel.readBufferCapacity() == 0)
                REQUIRE(ioChannel.readBuffer().isEmpty());
                REQUIRE(!ioChannel.readBuffer().isFull());
                REQUIRE(ioChannel.readBuffer().peekAll().empty());
                REQUIRE(ioChannel.isReadNotificationEnabled() == true);
                REQUIRE(ioChannel.readAll().empty());
                REQUIRE(ioChannel.readBuffer().isEmpty());
                REQUIRE(!ioChannel.readBuffer().isFull());
                REQUIRE(ioChannel.peekAll().empty());
                REQUIRE(ioChannel.readAll().empty());
            }
        }

        WHEN("we try to read more data than IOChannel read buffer size")
        {
            std::vector<char> data(initialDataInReadBuffer.size() + 8, ' ');
            const auto readBytes = ioChannel.read(data.data(), data.size());

            THEN("IOChannel reads all data in read buffer and enables read notification")
            {
                REQUIRE(readBytes == initialDataInReadBuffer.size());
                REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
                REQUIRE(ioChannel.dataToWrite() == 0);
                REQUIRE(ioChannel.dataAvailable() == 0);
                REQUIRE(ioChannel.readBufferCapacity() == 0)
                REQUIRE(ioChannel.readBuffer().isEmpty());
                REQUIRE(!ioChannel.readBuffer().isFull());
                REQUIRE(ioChannel.readBuffer().peekAll().empty());
                REQUIRE(ioChannel.isReadNotificationEnabled() == true);
                REQUIRE(ioChannel.readAll().empty());
                REQUIRE(ioChannel.readBuffer().isEmpty());
                REQUIRE(!ioChannel.readBuffer().isFull());
                REQUIRE(ioChannel.peekAll().empty());
                REQUIRE(ioChannel.readAll().empty());
            }
        }
    }

    GIVEN("an IOChannel with a full read buffer")
    {
        IOChannelTest ioChannel;
        REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
        REQUIRE(ioChannel.isReadNotificationEnabled() == true);
        REQUIRE(ioChannel.dataToWrite() == 0);
        REQUIRE(ioChannel.dataAvailable() == 0);
        REQUIRE(ioChannel.readBufferCapacity() == 0)
        REQUIRE(ioChannel.readBuffer().isEmpty());
        REQUIRE(ioChannel.writeBuffer().isEmpty());
        REQUIRE(ioChannel.peekAll().empty());
        REQUIRE(ioChannel.readAll().empty());
        REQUIRE(ioChannel.readBuffer().isEmpty());
        REQUIRE(ioChannel.readBuffer().capacity() == 0);
        ioChannel.setReadBufferCapacity(RingBuffer::defaultCapacity());
        REQUIRE(ioChannel.readBuffer().capacity() == RingBuffer::defaultCapacity());
        std::string initialDataInReadBuffer(RingBuffer::defaultCapacity(), ' ');
        for (auto &ch : initialDataInReadBuffer)
            ch = QRandomGenerator64::global()->bounded(0, 256);
        REQUIRE(initialDataInReadBuffer.size() == ioChannel.readBuffer().write(initialDataInReadBuffer));
        REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
        REQUIRE(ioChannel.isReadNotificationEnabled() == true);
        REQUIRE(ioChannel.dataToWrite() == 0);
        REQUIRE(ioChannel.dataAvailable() == initialDataInReadBuffer.size());
        REQUIRE(ioChannel.readBufferCapacity() == RingBuffer::defaultCapacity())
        REQUIRE(ioChannel.readBuffer().peekAll() == initialDataInReadBuffer);
        REQUIRE(!ioChannel.readBuffer().isEmpty());
        const auto isReadNotificationEnabled = GENERATE(AS(bool), true, false);
        ioChannel.isReadNotificationEnabled() = isReadNotificationEnabled;
        REQUIRE(ioChannel.readBuffer().isFull());

        WHEN("we try to read zero bytes from IOChannel")
        {
            char buffer = 0;
            const auto readBytes = ioChannel.read(&buffer, 0);

            THEN("IOChannel reads no data from read buffer and disables read notification")
            {
                REQUIRE(readBytes == 0);
                REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
                REQUIRE(ioChannel.dataToWrite() == 0);
                REQUIRE(ioChannel.dataAvailable() == initialDataInReadBuffer.size());
                REQUIRE(ioChannel.readBufferCapacity() == RingBuffer::defaultCapacity());
                REQUIRE(!ioChannel.readBuffer().isEmpty());
                REQUIRE(ioChannel.readBuffer().isFull());
                REQUIRE(ioChannel.readBuffer().peekAll() == initialDataInReadBuffer);
                REQUIRE(ioChannel.isReadNotificationEnabled() == false);
                REQUIRE(ioChannel.readAll() == initialDataInReadBuffer);
                REQUIRE(ioChannel.readBuffer().isEmpty());
                REQUIRE(!ioChannel.readBuffer().isFull());
                REQUIRE(ioChannel.peekAll().empty());
                REQUIRE(ioChannel.readAll().empty());
            }
        }

        WHEN("some data is read from IOChannel")
        {
            const auto dataSize = GENERATE(AS(size_t), 1, 3, 8);
            std::vector<char> data(dataSize, ' ');
            const auto readBytes = ioChannel.read(data.data(), data.size());

            THEN("IOChannel reads data from read buffer and enables read notification")
            {
                REQUIRE(readBytes == data.size());
                REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
                REQUIRE(ioChannel.dataToWrite() == 0);
                REQUIRE(ioChannel.dataAvailable() == (initialDataInReadBuffer.size() - dataSize));
                REQUIRE(ioChannel.readBufferCapacity() == RingBuffer::defaultCapacity());
                REQUIRE(!ioChannel.readBuffer().isEmpty());
                REQUIRE(!ioChannel.readBuffer().isFull());
                REQUIRE(ioChannel.readBuffer().peekAll() == std::string_view(initialDataInReadBuffer.data() + dataSize, initialDataInReadBuffer.size() - dataSize));
                REQUIRE(ioChannel.isReadNotificationEnabled() == true);
                REQUIRE(ioChannel.readAll() == std::string_view(initialDataInReadBuffer.data() + dataSize, initialDataInReadBuffer.size() - dataSize));
                REQUIRE(ioChannel.readBuffer().isEmpty());
                REQUIRE(!ioChannel.readBuffer().isFull());
                REQUIRE(ioChannel.peekAll().empty());
                REQUIRE(ioChannel.readAll().empty());

            }
        }

        WHEN("all data is read from IOChannel")
        {
            std::vector<char> data(initialDataInReadBuffer.size(), ' ');
            const auto readBytes = ioChannel.read(data.data(), data.size());

            THEN("IOChannel reads all data in read buffer and enables read notification")
            {
                REQUIRE(readBytes == data.size());
                REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
                REQUIRE(ioChannel.dataToWrite() == 0);
                REQUIRE(ioChannel.dataAvailable() == 0);
                REQUIRE(ioChannel.readBufferCapacity() == RingBuffer::defaultCapacity());
                REQUIRE(ioChannel.readBuffer().isEmpty());
                REQUIRE(!ioChannel.readBuffer().isFull());
                REQUIRE(ioChannel.readBuffer().peekAll().empty());
                REQUIRE(ioChannel.isReadNotificationEnabled() == true);
                REQUIRE(ioChannel.readAll().empty());
                REQUIRE(ioChannel.readBuffer().isEmpty());
                REQUIRE(!ioChannel.readBuffer().isFull());
                REQUIRE(ioChannel.peekAll().empty());
                REQUIRE(ioChannel.readAll().empty());
            }
        }

        WHEN("we try to read more data than IOChannel read buffer size")
        {
            std::vector<char> data(initialDataInReadBuffer.size() + 8, ' ');
            const auto readBytes = ioChannel.read(data.data(), data.size());

            THEN("IOChannel reads all data in read buffer and enables read notification")
            {
                REQUIRE(readBytes == initialDataInReadBuffer.size());
                REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
                REQUIRE(ioChannel.dataToWrite() == 0);
                REQUIRE(ioChannel.dataAvailable() == 0);
                REQUIRE(ioChannel.readBufferCapacity() == RingBuffer::defaultCapacity());
                REQUIRE(ioChannel.readBuffer().isEmpty());
                REQUIRE(!ioChannel.readBuffer().isFull());
                REQUIRE(ioChannel.readBuffer().peekAll().empty());
                REQUIRE(ioChannel.isReadNotificationEnabled() == true);
                REQUIRE(ioChannel.readAll().empty());
                REQUIRE(ioChannel.readBuffer().isEmpty());
                REQUIRE(!ioChannel.readBuffer().isFull());
                REQUIRE(ioChannel.peekAll().empty());
                REQUIRE(ioChannel.readAll().empty());
            }
        }
    }
}


SCENARIO("IOChannel reads data from channel")
{
    // Rules:
    //     IOChannel writes data in source to read buffer up to buffer's capacity
    //     IOChannel disables read notification if read buffer becomes full and enables it otherwise
    //     IOChannel does not emit receivedData if data can be fetched from source
    //
    // States:
    //     IOChannel with empty read buffer and empty source
    //     IOChannel with empty read buffer and source that does not make read buffer full
    //     IOChannel with empty read buffer and source that makes read buffer full
    //     IOChannel with empty read buffer and source with more data than read buffer capacity
    //     IOChannel with some data in read buffer and empty source
    //     IOChannel with some data in read buffer and source that does not make read buffer full
    //     IOChannel with some data in read buffer and source that makes read buffer full
    //     IOChannel with some data in read buffer and source with more data than read buffer capacity
    //     IOChannel with full read buffer and empty source
    //     IOChannel with full read buffer and source with some data

    GIVEN("an IOChannel with an empty read buffer")
    {
        IOChannelTest ioChannel;
        REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
        REQUIRE(ioChannel.isReadNotificationEnabled() == true);
        REQUIRE(ioChannel.dataToWrite() == 0);
        REQUIRE(ioChannel.dataAvailable() == 0);
        REQUIRE(ioChannel.readBufferCapacity() == 0)
        REQUIRE(ioChannel.writeBuffer().isEmpty());
        REQUIRE(ioChannel.peekAll().empty());
        REQUIRE(ioChannel.readAll().empty());
        REQUIRE(ioChannel.readBuffer().isEmpty());
        const auto isReadNotificationEnabled = GENERATE(AS(bool), true, false);
        ioChannel.isReadNotificationEnabled() = isReadNotificationEnabled;
        bool emittedReceivedDataSignal = false;
        Object::connect(&ioChannel, &IOChannel::receivedData, [&emittedReceivedDataSignal](){emittedReceivedDataSignal = true;});

        WHEN("data is read from channel with an empty source")
        {
            ioChannel.readFromChannel();

            THEN("IOChannel reads no data into read buffer, enables read notification and does not emit receivedData")
            {
                REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
                REQUIRE(ioChannel.dataToWrite() == 0);
                REQUIRE(ioChannel.dataAvailable() == 0);
                REQUIRE(ioChannel.readBufferCapacity() == 0)
                REQUIRE(ioChannel.writeBuffer().isEmpty());
                REQUIRE(ioChannel.peekAll().empty());
                REQUIRE(ioChannel.readAll().empty());
                REQUIRE(ioChannel.readBuffer().isEmpty());
                REQUIRE(!ioChannel.readBuffer().isFull());
                REQUIRE(ioChannel.isReadNotificationEnabled() == true);
                REQUIRE(!emittedReceivedDataSignal);
            }
        }

        WHEN("data is read from channel with source containing some data")
        {
            REQUIRE(ioChannel.dataSourceTest().fetchedSize() == 0);
            REQUIRE(ioChannel.dataSourceTest().dataAvailable() == 0);
            REQUIRE(ioChannel.dataSourceTest().data().empty());
            const auto initialDataInSource = GENERATE(AS(std::string_view), "asdf qwer", "1234 5678 9", "Hello World", "a");
            ioChannel.dataSourceTest().addDataToChannel(initialDataInSource);
            REQUIRE(ioChannel.dataSourceTest().fetchedSize() == 0);
            REQUIRE(ioChannel.dataSourceTest().dataAvailable() == initialDataInSource.size());
            REQUIRE(ioChannel.dataSourceTest().data() == initialDataInSource);
            ioChannel.readFromChannel();

            THEN("IOChannel writes all source data into read buffer, enables read notification and does not emit receivedData")
            {
                REQUIRE(ioChannel.dataSourceTest().fetchedSize() == initialDataInSource.size());
                REQUIRE(ioChannel.dataSourceTest().dataAvailable() == 0);
                REQUIRE(ioChannel.dataSourceTest().data().empty());
                REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
                REQUIRE(ioChannel.dataToWrite() == 0);
                REQUIRE(ioChannel.dataAvailable() == initialDataInSource.size());
                REQUIRE(ioChannel.readBufferCapacity() == 0)
                REQUIRE(ioChannel.writeBuffer().isEmpty());
                REQUIRE(ioChannel.peekAll() == initialDataInSource);
                REQUIRE(!ioChannel.readBuffer().isEmpty());
                REQUIRE(!ioChannel.readBuffer().isFull());
                REQUIRE(ioChannel.isReadNotificationEnabled() == true);
                REQUIRE(!emittedReceivedDataSignal);
            }
        }

        WHEN("data is read from channel with source containing data with size equal to buffer capacity")
        {
            REQUIRE(ioChannel.readBufferCapacity() == 0);
            REQUIRE(ioChannel.readBuffer().capacity() == 0);
            ioChannel.setReadBufferCapacity(RingBuffer::defaultCapacity());
            REQUIRE(ioChannel.readBufferCapacity() == RingBuffer::defaultCapacity());
            REQUIRE(ioChannel.readBuffer().capacity() == RingBuffer::defaultCapacity());
            REQUIRE(ioChannel.dataSourceTest().fetchedSize() == 0);
            REQUIRE(ioChannel.dataSourceTest().dataAvailable() == 0);
            REQUIRE(ioChannel.dataSourceTest().data().empty());
            std::string initialDataInSource(ioChannel.readBufferCapacity(), ' ');
            for (auto &ch : initialDataInSource)
                ch = QRandomGenerator64::global()->bounded(0, 256);
            ioChannel.dataSourceTest().addDataToChannel(initialDataInSource);
            REQUIRE(ioChannel.dataSourceTest().fetchedSize() == 0);
            REQUIRE(ioChannel.dataSourceTest().dataAvailable() == initialDataInSource.size());
            REQUIRE(ioChannel.dataSourceTest().data() == initialDataInSource);
            ioChannel.readFromChannel();

            THEN("IOChannel writes all source data into read buffer making it full, disables read notification and does not emit receivedData")
            {
                REQUIRE(ioChannel.dataSourceTest().fetchedSize() == initialDataInSource.size());
                REQUIRE(ioChannel.dataSourceTest().dataAvailable() == 0);
                REQUIRE(ioChannel.dataSourceTest().data().empty());
                REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
                REQUIRE(ioChannel.dataToWrite() == 0);
                REQUIRE(ioChannel.dataAvailable() == initialDataInSource.size());
                REQUIRE(ioChannel.readBufferCapacity() == RingBuffer::defaultCapacity())
                REQUIRE(ioChannel.writeBuffer().isEmpty());
                REQUIRE(ioChannel.peekAll() == initialDataInSource);
                REQUIRE(!ioChannel.readBuffer().isEmpty());
                REQUIRE(ioChannel.readBuffer().isFull());
                REQUIRE(ioChannel.isReadNotificationEnabled() == false);
                REQUIRE(!emittedReceivedDataSignal);
            }
        }

        WHEN("data is read from channel with source containing more data than buffer capacity")
        {
            REQUIRE(ioChannel.readBufferCapacity() == 0);
            REQUIRE(ioChannel.readBuffer().capacity() == 0);
            ioChannel.setReadBufferCapacity(RingBuffer::defaultCapacity());
            REQUIRE(ioChannel.readBufferCapacity() == RingBuffer::defaultCapacity());
            REQUIRE(ioChannel.readBuffer().capacity() == RingBuffer::defaultCapacity());
            REQUIRE(ioChannel.dataSourceTest().fetchedSize() == 0);
            REQUIRE(ioChannel.dataSourceTest().dataAvailable() == 0);
            REQUIRE(ioChannel.dataSourceTest().data().empty());
            const auto extraDataInSource = GENERATE(AS(size_t), 1, 3, 8);
            std::string initialDataInSource(ioChannel.readBufferCapacity() + extraDataInSource, ' ');
            for (auto &ch : initialDataInSource)
                ch = QRandomGenerator64::global()->bounded(0, 256);
            ioChannel.dataSourceTest().addDataToChannel(initialDataInSource);
            REQUIRE(ioChannel.dataSourceTest().fetchedSize() == 0);
            REQUIRE(ioChannel.dataSourceTest().dataAvailable() == initialDataInSource.size());
            REQUIRE(ioChannel.dataSourceTest().data() == initialDataInSource);
            ioChannel.readFromChannel();

            THEN("IOChannel writes all data it can from source to read buffer making it full, disables read notification and does not emit receivedData")
            {
                REQUIRE(ioChannel.dataSourceTest().fetchedSize() == ioChannel.readBufferCapacity());
                REQUIRE(ioChannel.dataSourceTest().dataAvailable() == (initialDataInSource.size() - ioChannel.readBufferCapacity()));
                REQUIRE(ioChannel.dataSourceTest().data() == std::string_view(initialDataInSource.data() + ioChannel.readBufferCapacity(), initialDataInSource.size() - ioChannel.readBufferCapacity()));
                REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
                REQUIRE(ioChannel.dataToWrite() == 0);
                REQUIRE(ioChannel.dataAvailable() == ioChannel.readBufferCapacity());
                REQUIRE(ioChannel.readBufferCapacity() == RingBuffer::defaultCapacity())
                REQUIRE(ioChannel.writeBuffer().isEmpty());
                REQUIRE(ioChannel.peekAll() == std::string_view(initialDataInSource.data(), ioChannel.readBufferCapacity()));
                REQUIRE(!ioChannel.readBuffer().isEmpty());
                REQUIRE(ioChannel.readBuffer().isFull());
                REQUIRE(ioChannel.isReadNotificationEnabled() == false);
                REQUIRE(!emittedReceivedDataSignal);
            }
        }
    }

    GIVEN("an IOChannel with some data in read buffer")
    {
        IOChannelTest ioChannel;
        REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
        REQUIRE(ioChannel.isReadNotificationEnabled() == true);
        REQUIRE(ioChannel.dataToWrite() == 0);
        REQUIRE(ioChannel.dataAvailable() == 0);
        REQUIRE(ioChannel.readBufferCapacity() == 0)
        REQUIRE(ioChannel.readBuffer().isEmpty());
        REQUIRE(ioChannel.writeBuffer().isEmpty());
        REQUIRE(ioChannel.peekAll().empty());
        REQUIRE(ioChannel.readAll().empty());
        REQUIRE(ioChannel.readBuffer().isEmpty());
        const auto initialDataInReadBuffer = GENERATE(AS(std::string_view), "asdf qwer", "1234 5678 9", "Hello World");
        REQUIRE(initialDataInReadBuffer.size() == ioChannel.readBuffer().write(initialDataInReadBuffer));
        REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
        REQUIRE(ioChannel.isReadNotificationEnabled() == true);
        REQUIRE(ioChannel.dataToWrite() == 0);
        REQUIRE(ioChannel.dataAvailable() == initialDataInReadBuffer.size());
        REQUIRE(ioChannel.readBufferCapacity() == 0)
        REQUIRE(ioChannel.readBuffer().peekAll() == initialDataInReadBuffer);
        REQUIRE(!ioChannel.readBuffer().isEmpty() && !ioChannel.readBuffer().isFull());
        const auto isReadNotificationEnabled = GENERATE(AS(bool), true, false);
        ioChannel.isReadNotificationEnabled() = isReadNotificationEnabled;
        bool emittedReceivedDataSignal = false;
        Object::connect(&ioChannel, &IOChannel::receivedData, [&emittedReceivedDataSignal](){emittedReceivedDataSignal = true;});

        WHEN("data is read from channel with an empty source")
        {
            ioChannel.readFromChannel();

            THEN("IOChannel reads no data into read buffer, enables read notification and does not emit receivedData")
            {
                REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
                REQUIRE(ioChannel.dataToWrite() == 0);
                REQUIRE(ioChannel.dataAvailable() == initialDataInReadBuffer.size());
                REQUIRE(ioChannel.readBufferCapacity() == 0)
                REQUIRE(ioChannel.writeBuffer().isEmpty());
                REQUIRE(ioChannel.peekAll() == initialDataInReadBuffer);
                REQUIRE(!ioChannel.readBuffer().isEmpty());
                REQUIRE(!ioChannel.readBuffer().isFull());
                REQUIRE(ioChannel.isReadNotificationEnabled() == true);
                REQUIRE(!emittedReceivedDataSignal);
            }
        }

        WHEN("data is read from channel with source containing some data")
        {
            REQUIRE(ioChannel.dataSourceTest().fetchedSize() == 0);
            REQUIRE(ioChannel.dataSourceTest().dataAvailable() == 0);
            REQUIRE(ioChannel.dataSourceTest().data().empty());
            const auto initialDataSizeInSource = GENERATE(AS(size_t), 1, 3, 8);
            std::string initialDataInSource(initialDataSizeInSource, ' ');
            for (auto &ch : initialDataInSource)
                ch = QRandomGenerator64::global()->bounded(0, 256);
            ioChannel.dataSourceTest().addDataToChannel(initialDataInSource);
            REQUIRE(ioChannel.dataSourceTest().fetchedSize() == 0);
            REQUIRE(ioChannel.dataSourceTest().dataAvailable() == initialDataInSource.size());
            REQUIRE(ioChannel.dataSourceTest().data() == initialDataInSource);
            ioChannel.readFromChannel();

            THEN("IOChannel writes all source data into read buffer, enables read notification and does not emit receivedData")
            {
                REQUIRE(ioChannel.dataSourceTest().fetchedSize() == initialDataInSource.size());
                REQUIRE(ioChannel.dataSourceTest().dataAvailable() == 0);
                REQUIRE(ioChannel.dataSourceTest().data().empty());
                REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
                REQUIRE(ioChannel.dataToWrite() == 0);
                REQUIRE(ioChannel.dataAvailable() == (initialDataInReadBuffer.size() + initialDataInSource.size()));
                REQUIRE(ioChannel.readBufferCapacity() == 0)
                REQUIRE(ioChannel.writeBuffer().isEmpty());
                REQUIRE(ioChannel.peekAll() == (std::string(initialDataInReadBuffer) + initialDataInSource));
                REQUIRE(!ioChannel.readBuffer().isEmpty());
                REQUIRE(!ioChannel.readBuffer().isFull());
                REQUIRE(ioChannel.isReadNotificationEnabled() == true);
                REQUIRE(!emittedReceivedDataSignal);
            }
        }

        WHEN("data is read from channel with source containing data that makes read buffer full")
        {
            REQUIRE(ioChannel.readBufferCapacity() == 0);
            REQUIRE(ioChannel.readBuffer().capacity() == 0);
            ioChannel.setReadBufferCapacity(RingBuffer::defaultCapacity());
            REQUIRE(ioChannel.readBufferCapacity() == RingBuffer::defaultCapacity());
            REQUIRE(ioChannel.readBuffer().capacity() == RingBuffer::defaultCapacity());
            REQUIRE(ioChannel.dataSourceTest().fetchedSize() == 0);
            REQUIRE(ioChannel.dataSourceTest().dataAvailable() == 0);
            REQUIRE(ioChannel.dataSourceTest().data().empty());
            REQUIRE(ioChannel.readBufferCapacity() > initialDataInReadBuffer.size());
            std::string initialDataInSource(ioChannel.readBufferCapacity() - initialDataInReadBuffer.size(), ' ');
            for (auto &ch : initialDataInSource)
                ch = QRandomGenerator64::global()->bounded(0, 256);
            ioChannel.dataSourceTest().addDataToChannel(initialDataInSource);
            REQUIRE(ioChannel.dataSourceTest().fetchedSize() == 0);
            REQUIRE(ioChannel.dataSourceTest().dataAvailable() == initialDataInSource.size());
            REQUIRE(ioChannel.dataSourceTest().data() == initialDataInSource);
            ioChannel.readFromChannel();

            THEN("IOChannel writes all source data into read buffer making it full, disables read notification and does not emit receivedData")
            {
                REQUIRE(ioChannel.dataSourceTest().fetchedSize() == initialDataInSource.size());
                REQUIRE(ioChannel.dataSourceTest().dataAvailable() == 0);
                REQUIRE(ioChannel.dataSourceTest().data().empty());
                REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
                REQUIRE(ioChannel.dataToWrite() == 0);
                REQUIRE(ioChannel.dataAvailable() == (initialDataInReadBuffer.size() + initialDataInSource.size()));
                REQUIRE(ioChannel.dataAvailable() == RingBuffer::defaultCapacity());
                REQUIRE(ioChannel.readBufferCapacity() == RingBuffer::defaultCapacity())
                REQUIRE(ioChannel.writeBuffer().isEmpty());
                REQUIRE(ioChannel.peekAll() == (std::string(initialDataInReadBuffer) + initialDataInSource));
                REQUIRE(!ioChannel.readBuffer().isEmpty());
                REQUIRE(ioChannel.readBuffer().isFull());
                REQUIRE(ioChannel.isReadNotificationEnabled() == false);
                REQUIRE(!emittedReceivedDataSignal);
            }
        }

        WHEN("data is read from channel with source containing more data than what is required to make buffer full")
        {
            REQUIRE(ioChannel.readBufferCapacity() == 0);
            REQUIRE(ioChannel.readBuffer().capacity() == 0);
            ioChannel.setReadBufferCapacity(RingBuffer::defaultCapacity());
            REQUIRE(ioChannel.readBufferCapacity() == RingBuffer::defaultCapacity());
            REQUIRE(ioChannel.readBuffer().capacity() == RingBuffer::defaultCapacity());
            REQUIRE(ioChannel.dataSourceTest().fetchedSize() == 0);
            REQUIRE(ioChannel.dataSourceTest().dataAvailable() == 0);
            REQUIRE(ioChannel.dataSourceTest().data().empty());
            REQUIRE(ioChannel.readBufferCapacity() > initialDataInReadBuffer.size());
            const auto extraDataSizeInSource = GENERATE(AS(size_t), 1, 3, 8);
            std::string initialDataInSource(ioChannel.readBufferCapacity() - initialDataInReadBuffer.size() + extraDataSizeInSource, ' ');
            for (auto &ch : initialDataInSource)
                ch = QRandomGenerator64::global()->bounded(0, 256);
            ioChannel.dataSourceTest().addDataToChannel(initialDataInSource);
            REQUIRE(ioChannel.dataSourceTest().fetchedSize() == 0);
            REQUIRE(ioChannel.dataSourceTest().dataAvailable() == initialDataInSource.size());
            REQUIRE(ioChannel.dataSourceTest().data() == initialDataInSource);
            ioChannel.readFromChannel();

            THEN("IOChannel writes all data it can from source to read buffer making it full, disables read notification and does not emit receivedData")
            {
                REQUIRE(ioChannel.dataSourceTest().fetchedSize() == (initialDataInSource.size() - extraDataSizeInSource));
                REQUIRE(ioChannel.dataSourceTest().dataAvailable() == extraDataSizeInSource);
                REQUIRE(ioChannel.dataSourceTest().data() == std::string_view(initialDataInSource.data() + initialDataInSource.size() - extraDataSizeInSource, extraDataSizeInSource));
                REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
                REQUIRE(ioChannel.dataToWrite() == 0);
                REQUIRE(ioChannel.dataAvailable() == ioChannel.readBufferCapacity());
                REQUIRE(ioChannel.readBufferCapacity() == RingBuffer::defaultCapacity())
                REQUIRE(ioChannel.writeBuffer().isEmpty());
                REQUIRE(ioChannel.peekAll() == (std::string(initialDataInReadBuffer) + std::string(initialDataInSource.data(), initialDataInSource.size() - extraDataSizeInSource)));
                REQUIRE(!ioChannel.readBuffer().isEmpty());
                REQUIRE(ioChannel.readBuffer().isFull());
                REQUIRE(ioChannel.isReadNotificationEnabled() == false);
                REQUIRE(!emittedReceivedDataSignal);
            }
        }
    }

    GIVEN("an IOChannel with a full read buffer")
    {
        IOChannelTest ioChannel;
        REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
        REQUIRE(ioChannel.isReadNotificationEnabled() == true);
        REQUIRE(ioChannel.dataToWrite() == 0);
        REQUIRE(ioChannel.dataAvailable() == 0);
        REQUIRE(ioChannel.readBufferCapacity() == 0)
        REQUIRE(ioChannel.readBuffer().isEmpty());
        REQUIRE(ioChannel.writeBuffer().isEmpty());
        REQUIRE(ioChannel.peekAll().empty());
        REQUIRE(ioChannel.readAll().empty());
        REQUIRE(ioChannel.readBuffer().isEmpty());
        REQUIRE(ioChannel.readBuffer().capacity() == 0);
        ioChannel.setReadBufferCapacity(RingBuffer::defaultCapacity());
        REQUIRE(ioChannel.readBuffer().capacity() == RingBuffer::defaultCapacity());
        std::string initialDataInReadBuffer(RingBuffer::defaultCapacity(), ' ');
        for (auto &ch : initialDataInReadBuffer)
            ch = QRandomGenerator64::global()->bounded(0, 256);
        REQUIRE(initialDataInReadBuffer.size() == ioChannel.readBuffer().write(initialDataInReadBuffer));
        REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
        REQUIRE(ioChannel.isReadNotificationEnabled() == true);
        REQUIRE(ioChannel.dataToWrite() == 0);
        REQUIRE(ioChannel.dataAvailable() == initialDataInReadBuffer.size());
        REQUIRE(ioChannel.readBufferCapacity() == RingBuffer::defaultCapacity())
        REQUIRE(ioChannel.readBuffer().peekAll() == initialDataInReadBuffer);
        REQUIRE(!ioChannel.readBuffer().isEmpty());
        const auto isReadNotificationEnabled = GENERATE(AS(bool), true, false);
        ioChannel.isReadNotificationEnabled() = isReadNotificationEnabled;
        REQUIRE(ioChannel.readBuffer().isFull());
        bool emittedReceivedDataSignal = false;
        Object::connect(&ioChannel, &IOChannel::receivedData, [&emittedReceivedDataSignal](){emittedReceivedDataSignal = true;});

        WHEN("data is read from channel with an empty source")
        {
            ioChannel.readFromChannel();

            THEN("IOChannel reads no data into read buffer, disables read notification and does not emit receivedData")
            {
                REQUIRE(ioChannel.dataSourceTest().fetchedSize() == 0);
                REQUIRE(ioChannel.dataSourceTest().dataAvailable() == 0);
                REQUIRE(ioChannel.dataSourceTest().data().empty());
                REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
                REQUIRE(ioChannel.dataToWrite() == 0);
                REQUIRE(ioChannel.dataAvailable() == initialDataInReadBuffer.size());
                REQUIRE(ioChannel.readBufferCapacity() == RingBuffer::defaultCapacity());
                REQUIRE(ioChannel.writeBuffer().isEmpty());
                REQUIRE(ioChannel.peekAll() == initialDataInReadBuffer);
                REQUIRE(!ioChannel.readBuffer().isEmpty());
                REQUIRE(ioChannel.readBuffer().isFull());
                REQUIRE(ioChannel.isReadNotificationEnabled() == false);
                REQUIRE(!emittedReceivedDataSignal);
            }
        }

        WHEN("data is read from channel with source containing some data")
        {
            REQUIRE(ioChannel.dataSourceTest().fetchedSize() == 0);
            REQUIRE(ioChannel.dataSourceTest().dataAvailable() == 0);
            REQUIRE(ioChannel.dataSourceTest().data().empty());
            const auto initialDataSizeInSource = GENERATE(AS(size_t), 1, 3, 8);
            std::string initialDataInSource(initialDataSizeInSource, ' ');
            for (auto &ch : initialDataInSource)
                ch = QRandomGenerator64::global()->bounded(0, 256);
            ioChannel.dataSourceTest().addDataToChannel(initialDataInSource);
            REQUIRE(ioChannel.dataSourceTest().fetchedSize() == 0);
            REQUIRE(ioChannel.dataSourceTest().dataAvailable() == initialDataInSource.size());
            REQUIRE(ioChannel.dataSourceTest().data() == initialDataInSource);
            ioChannel.readFromChannel();

            THEN("IOChannel reads no data into read buffer, disables read notification and does not emit receivedData")
            {
                REQUIRE(ioChannel.dataSourceTest().fetchedSize() == 0);
                REQUIRE(ioChannel.dataSourceTest().dataAvailable() == initialDataInSource.size());
                REQUIRE(ioChannel.dataSourceTest().data() == initialDataInSource);
                REQUIRE(ioChannel.isWriteNotificationEnabled() == true);
                REQUIRE(ioChannel.dataToWrite() == 0);
                REQUIRE(ioChannel.dataAvailable() == initialDataInReadBuffer.size());
                REQUIRE(ioChannel.readBufferCapacity() == RingBuffer::defaultCapacity());
                REQUIRE(ioChannel.writeBuffer().isEmpty());
                REQUIRE(ioChannel.peekAll() == initialDataInReadBuffer);
                REQUIRE(!ioChannel.readBuffer().isEmpty());
                REQUIRE(ioChannel.readBuffer().isFull());
                REQUIRE(ioChannel.isReadNotificationEnabled() == false);
                REQUIRE(!emittedReceivedDataSignal);
            }
        }
    }
}
