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
#include <Spectator.h>


using Kourier::HttpChunkMetadataParser;
using Kourier::IOChannel;
using Kourier::RingBuffer;
using Kourier::DataSource;
using Kourier::DataSink;


namespace Test::HttpChunkMetadataParser
{

class IOChannelTest : public IOChannel
{
public:
    IOChannelTest(std::string_view data) {m_readBuffer.write(data);}
    ~IOChannelTest() override = default;
    RingBuffer &readBuffer() {return m_readBuffer;}
    RingBuffer &writeBuffer() {return m_writeBuffer;}
    bool &isReadNotificationEnabled() {return m_isReadNotificationEnabled;}
    bool &isWriteNotificationEnabled() {return m_isWriteNotificationEnabled;}

private:
    DataSource &dataSource() override {std::abort();}
    DataSink &dataSink() override {std::abort();}
    void onReadNotificationChanged() override {}
    void onWriteNotificationChanged() override {}
};

}

using namespace Test::HttpChunkMetadataParser;


SCENARIO("HttpChunkMetadataParser fetches positive chunk data size for chunk metadata without extension")
{
    GIVEN("chunk metadata without extension")
    {
        const auto chunkMetadata = GENERATE(AS(std::pair<size_t, std::string_view>),
                                            {1, "1\r\n"},
                                            {0xFF, "FF\r\n"},
                                            {0x37ABFF, "37ABFF\r\n"},
                                            {0x12345, "12345\r\n"});

        WHEN("chunk metadata is parsed at once")
        {
            IOChannelTest ioChannel(chunkMetadata.second);
            size_t chunkDataSize = 0;
            size_t chunkMetadataSize = 0;
            const auto parserStatus = HttpChunkMetadataParser::parse(ioChannel, chunkDataSize, chunkMetadataSize);

            THEN("parser parses metadata and informs that chunk data is expected")
            {
                REQUIRE(parserStatus == HttpChunkMetadataParser::ChunkMetadataParserStatus::ExpectingChunkData);
                REQUIRE(chunkDataSize == chunkMetadata.first);
                REQUIRE(chunkMetadataSize == chunkMetadata.second.size());
            }
        }

        WHEN("chunk metadata is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            size_t chunkDataSize = 0;
            size_t chunkMetadataSize = 0;
            for (auto i = 0; i < (chunkMetadata.second.size() - 1); ++i)
            {
                ioChannel.readBuffer().write(&chunkMetadata.second[i], 1);
                REQUIRE(HttpChunkMetadataParser::parse(ioChannel, chunkDataSize, chunkMetadataSize) == HttpChunkMetadataParser::ChunkMetadataParserStatus::NeedsMoreData);
            }
            ioChannel.readBuffer().write(&chunkMetadata.second[chunkMetadata.second.size() - 1], 1);
            const auto parserStatus = HttpChunkMetadataParser::parse(ioChannel, chunkDataSize, chunkMetadataSize);

            THEN("parser parses metadata and informs that chunk data is expected")
            {
                REQUIRE(parserStatus == HttpChunkMetadataParser::ChunkMetadataParserStatus::ExpectingChunkData);
                REQUIRE(chunkDataSize == chunkMetadata.first);
                REQUIRE(chunkMetadataSize == chunkMetadata.second.size());
            }
        }
    }
}


SCENARIO("HttpChunkMetadataParser ignores chunk extension up to CRLF")
{
    GIVEN("chunk metadata with extension")
    {
        const auto chunkMetadata = GENERATE(AS(std::pair<size_t, std::string_view>),
                                            {1, "1 ; name1 = value1; name2 = \"value 2\"; q=1.000\r\n"},
                                            {0xFF, "FF;n1;n2;n3\r\n"},
                                            {0x37ABFF, "37ABFF;key=value q=0.450\r\n"},
                                            {0x12345, "12345 ; token = \"quoted text\"\t \t; atoken\r\n"});

        WHEN("chunk metadata is parsed at once")
        {
            IOChannelTest ioChannel(chunkMetadata.second);
            size_t chunkDataSize = 0;
            size_t chunkMetadataSize = 0;
            const auto parserStatus = HttpChunkMetadataParser::parse(ioChannel, chunkDataSize, chunkMetadataSize);

            THEN("parser parses metadata and informs that chunk data is expected")
            {
                REQUIRE(parserStatus == HttpChunkMetadataParser::ChunkMetadataParserStatus::ExpectingChunkData);
                REQUIRE(chunkDataSize == chunkMetadata.first);
                REQUIRE(chunkMetadataSize == chunkMetadata.second.size());
            }
        }

        WHEN("chunk metadata is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            size_t chunkDataSize = 0;
            size_t chunkMetadataSize = 0;
            for (auto i = 0; i < (chunkMetadata.second.size() - 1); ++i)
            {
                ioChannel.readBuffer().write(&chunkMetadata.second[i], 1);
                REQUIRE(HttpChunkMetadataParser::parse(ioChannel, chunkDataSize, chunkMetadataSize) == HttpChunkMetadataParser::ChunkMetadataParserStatus::NeedsMoreData);
            }
            ioChannel.readBuffer().write(&chunkMetadata.second[chunkMetadata.second.size() - 1], 1);
            const auto parserStatus = HttpChunkMetadataParser::parse(ioChannel, chunkDataSize, chunkMetadataSize);

            THEN("parser parses metadata and informs that chunk data is expected")
            {
                REQUIRE(parserStatus == HttpChunkMetadataParser::ChunkMetadataParserStatus::ExpectingChunkData);
                REQUIRE(chunkDataSize == chunkMetadata.first);
                REQUIRE(chunkMetadataSize == chunkMetadata.second.size());
            }
        }
    }
}


SCENARIO("HttpChunkMetadataParser fails if chunk metadata does not have a LF after the CR")
{
    GIVEN("chunk metadata without LF after CR")
    {
        const auto chunkMetadata = GENERATE(AS(std::string_view),
                                            "1\r\r",
                                            "FF\r\t",
                                            "37ABFF\r ",
                                            "12345\ra",
                                            "1 ; name1 = value1; name2 = \"value 2\"; q=1.000\rq",
                                            "FF;n1;n2;n3\r\1",
                                            "37ABFF;key=value q=0.450\r0",
                                            "12345 ; token = \"quoted text\"\t \t; atoken\ry");

        WHEN("chunk metadata is parsed at once")
        {
            IOChannelTest ioChannel(chunkMetadata);
            size_t chunkDataSize = 0;
            size_t chunkMetadataSize = 0;
            const auto parserStatus = HttpChunkMetadataParser::parse(ioChannel, chunkDataSize, chunkMetadataSize);

            THEN("parser fails to parse chunk metadata")
            {
                REQUIRE(parserStatus == HttpChunkMetadataParser::ChunkMetadataParserStatus::Failed);
            }
        }

        WHEN("chunk metadata is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            size_t chunkDataSize = 0;
            size_t chunkMetadataSize = 0;
            for (auto i = 0; i < (chunkMetadata.size() - 1); ++i)
            {
                ioChannel.readBuffer().write(&chunkMetadata[i], 1);
                REQUIRE(HttpChunkMetadataParser::parse(ioChannel, chunkDataSize, chunkMetadataSize) == HttpChunkMetadataParser::ChunkMetadataParserStatus::NeedsMoreData);
            }
            ioChannel.readBuffer().write(&chunkMetadata[chunkMetadata.size() - 1], 1);
            const auto parserStatus = HttpChunkMetadataParser::parse(ioChannel, chunkDataSize, chunkMetadataSize);

            THEN("parser fails to parse chunk metadata")
            {
                REQUIRE(parserStatus == HttpChunkMetadataParser::ChunkMetadataParserStatus::Failed);
            }
        }
    }
}


SCENARIO("HttpChunkMetadataParser parses chunk sizes up to 12 hex digits")
{
    GIVEN("chunk metadata without chunk data sizes with more than 12 digits")
    {
        const auto chunkMetadata = GENERATE(AS(std::string_view),
                                            "0000000000001\r\n",
                                            "FFAABBCCDDEEF\r\n",
                                            "37ABFF37ABFF37ABFF37ABFF37ABFF37ABFF\r\n",
                                            "0000000000000\r\n",
                                            "11111111111111111111 ; name1 = value1; name2 = \"value 2\"; q=1.000\r\n",
                                            "FFFFFFFFFFFFF;n1;n2;n3\r\n");

        WHEN("chunk metadata is parsed at once")
        {
            IOChannelTest ioChannel(chunkMetadata);
            size_t chunkDataSize = 0;
            size_t chunkMetadataSize = 0;
            const auto parserStatus = HttpChunkMetadataParser::parse(ioChannel, chunkDataSize, chunkMetadataSize);

            THEN("parser fails to parse chunk metadata")
            {
                REQUIRE(parserStatus == HttpChunkMetadataParser::ChunkMetadataParserStatus::Failed);
            }
        }

        WHEN("chunk metadata is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            size_t chunkDataSize = 0;
            size_t chunkMetadataSize = 0;
            auto parserStatus = HttpChunkMetadataParser::ChunkMetadataParserStatus::NeedsMoreData;
            size_t index = 0;
            while (parserStatus == HttpChunkMetadataParser::ChunkMetadataParserStatus::NeedsMoreData)
            for (auto i = 0; i < (chunkMetadata.size() - 1); ++i)
            {
                ioChannel.readBuffer().write(&chunkMetadata[index++], 1);
                parserStatus = HttpChunkMetadataParser::parse(ioChannel, chunkDataSize, chunkMetadataSize);
            }

            THEN("parser fails to parse chunk metadata")
            {
                REQUIRE(parserStatus == HttpChunkMetadataParser::ChunkMetadataParserStatus::Failed);
            }
        }
    }

    GIVEN("chunk metadata with sizes up to 12 digits")
    {
        const auto chunkMetadata = GENERATE(AS(std::pair<size_t, std::string_view>),
                                            {1, "1\r\n"},
                                            {0xFFFFFFFFFFFF, "FFFFFFFFFFFF\r\n"},
                                            {0x37ABFF, "37ABFF\r\n"},
                                            {0x1234554321, "1234554321\r\n"},
                                            {0x111111111111, "111111111111 ; name1 = value1; name2 = \"value 2\"; q=1.000\r\n"},
                                            {0xFFAABBCCDDEE, "FFAABBCCDDEE;n1;n2;n3\r\n"},
                                            {0x37ABFF7ABFF, "37ABFF7ABFF;key=value q=0.450\r\n"},
                                            {0x12345, "12345 ; token = \"quoted text\"\t \t; atoken\r\n"});

        WHEN("chunk metadata is parsed at once")
        {
            IOChannelTest ioChannel(chunkMetadata.second);
            size_t chunkDataSize = 0;
            size_t chunkMetadataSize = 0;
            const auto parserStatus = HttpChunkMetadataParser::parse(ioChannel, chunkDataSize, chunkMetadataSize);

            THEN("parser parses metadata and informs that chunk data is expected")
            {
                REQUIRE(parserStatus == HttpChunkMetadataParser::ChunkMetadataParserStatus::ExpectingChunkData);
                REQUIRE(chunkDataSize == chunkMetadata.first);
                REQUIRE(chunkMetadataSize == chunkMetadata.second.size());
            }
        }

        WHEN("chunk metadata is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            size_t chunkDataSize = 0;
            size_t chunkMetadataSize = 0;
            for (auto i = 0; i < (chunkMetadata.second.size() - 1); ++i)
            {
                ioChannel.readBuffer().write(&chunkMetadata.second[i], 1);
                REQUIRE(HttpChunkMetadataParser::parse(ioChannel, chunkDataSize, chunkMetadataSize) == HttpChunkMetadataParser::ChunkMetadataParserStatus::NeedsMoreData);
            }
            ioChannel.readBuffer().write(&chunkMetadata.second[chunkMetadata.second.size() - 1], 1);
            const auto parserStatus = HttpChunkMetadataParser::parse(ioChannel, chunkDataSize, chunkMetadataSize);

            THEN("parser parses metadata and informs that chunk data is expected")
            {
                REQUIRE(parserStatus == HttpChunkMetadataParser::ChunkMetadataParserStatus::ExpectingChunkData);
                REQUIRE(chunkDataSize == chunkMetadata.first);
                REQUIRE(chunkMetadataSize == chunkMetadata.second.size());
            }
        }
    }
}


SCENARIO("HttpChunkMetadataParser informs that it needs more data if chunk metadata has zero size and up to one byte after its end")
{
    GIVEN("chunk metadata with zero sizes and without any data after chunk metadata")
    {
        const auto chunkMetadata = GENERATE(AS(std::string_view),
                                            "0\r\n",
                                            "0 ; name1 = value1; name2 = \"value 2\"; q=1.000\r\n",
                                            "0;n1;n2;n3\r\n",
                                            "0;key=value q=0.450\r\n",
                                            "0 ; token = \"quoted text\"\t \t; atoken\r\n",
                                            "0\r\n\r",
                                            "0\r\na",
                                            "0 ; name1 = value1; name2 = \"value 2\"; q=1.000\r\n\r",
                                            "0 ; name1 = value1; name2 = \"value 2\"; q=1.000\r\n ",
                                            "0;n1;n2;n3\r\n\r",
                                            "0;key=value q=0.450\r\n\r",
                                            "0 ; token = \"quoted text\"\t \t; atoken\r\n\r");

        WHEN("chunk metadata is parsed at once")
        {
            IOChannelTest ioChannel(chunkMetadata);
            size_t chunkDataSize = 0;
            size_t chunkMetadataSize = 0;
            const auto parserStatus = HttpChunkMetadataParser::parse(ioChannel, chunkDataSize, chunkMetadataSize);

            THEN("parser informs that it needs more data")
            {
                REQUIRE(parserStatus == HttpChunkMetadataParser::ChunkMetadataParserStatus::NeedsMoreData);
            }
        }

        WHEN("chunk metadata is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            size_t chunkDataSize = 0;
            size_t chunkMetadataSize = 0;
            for (auto i = 0; i < (chunkMetadata.size() - 1); ++i)
            {
                ioChannel.readBuffer().write(&chunkMetadata[i], 1);
                REQUIRE(HttpChunkMetadataParser::parse(ioChannel, chunkDataSize, chunkMetadataSize) == HttpChunkMetadataParser::ChunkMetadataParserStatus::NeedsMoreData);
            }
            ioChannel.readBuffer().write(&chunkMetadata[chunkMetadata.size() - 1], 1);
            const auto parserStatus = HttpChunkMetadataParser::parse(ioChannel, chunkDataSize, chunkMetadataSize);

            THEN("parser informs that it needs more data")
            {
                REQUIRE(parserStatus == HttpChunkMetadataParser::ChunkMetadataParserStatus::NeedsMoreData);
            }
        }
    }
}


SCENARIO("HttpChunkMetadataParser informs that request was parsed if chunk metadata has zero size and has CRLF after it")
{
    GIVEN("chunk metadata with zero sizes and CRLF after metadata")
    {
        const auto chunkMetadata = GENERATE(AS(std::string_view),
                                            "0\r\n\r\n",
                                            "0 ; name1 = value1; name2 = \"value 2\"; q=1.000\r\n\r\n",
                                            "0;n1;n2;n3\r\n\r\n",
                                            "0;key=value q=0.450\r\n\r\n",
                                            "0 ; token = \"quoted text\"\t \t; atoken\r\n\r\n");

        WHEN("chunk metadata is parsed at once")
        {
            IOChannelTest ioChannel(chunkMetadata);
            size_t chunkDataSize = 0;
            size_t chunkMetadataSize = 0;
            const auto parserStatus = HttpChunkMetadataParser::parse(ioChannel, chunkDataSize, chunkMetadataSize);

            THEN("parser informs that chunked request was parsed")
            {
                REQUIRE(parserStatus == HttpChunkMetadataParser::ChunkMetadataParserStatus::ParsedRequest);
                REQUIRE(chunkDataSize == 0);
                REQUIRE(chunkMetadataSize == chunkMetadata.size());
            }
        }

        WHEN("chunk metadata is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            size_t chunkDataSize = 0;
            size_t chunkMetadataSize = 0;
            for (auto i = 0; i < (chunkMetadata.size() - 1); ++i)
            {
                ioChannel.readBuffer().write(&chunkMetadata[i], 1);
                REQUIRE(HttpChunkMetadataParser::parse(ioChannel, chunkDataSize, chunkMetadataSize) == HttpChunkMetadataParser::ChunkMetadataParserStatus::NeedsMoreData);
            }
            ioChannel.readBuffer().write(&chunkMetadata[chunkMetadata.size() - 1], 1);
            const auto parserStatus = HttpChunkMetadataParser::parse(ioChannel, chunkDataSize, chunkMetadataSize);

            THEN("parser informs that chunked request was parsed")
            {
                REQUIRE(parserStatus == HttpChunkMetadataParser::ChunkMetadataParserStatus::ParsedRequest);
                REQUIRE(chunkDataSize == 0);
                REQUIRE(chunkMetadataSize == chunkMetadata.size());
            }
        }
    }
}


SCENARIO("HttpChunkMetadataParser informs that it expects trailers if chunk metadata has zero size and does not have CRLF after it")
{
    GIVEN("chunk metadata with zero sizes and not a CRLF after metadata")
    {
        const auto chunkMetadata = GENERATE(AS(std::string_view),
                                            "0\r\n\r\r",
                                            "0 ; name1 = value1; name2 = \"value 2\"; q=1.000\r\n\n\n",
                                            "0;n1;n2;n3\r\nna",
                                            "0;key=value q=0.450\r\n\n\r",
                                            "0 ; token = \"quoted text\"\t \t; atoken\r\n\t ");

        WHEN("chunk metadata is parsed at once")
        {
            IOChannelTest ioChannel(chunkMetadata);
            size_t chunkDataSize = 0;
            size_t chunkMetadataSize = 0;
            const auto parserStatus = HttpChunkMetadataParser::parse(ioChannel, chunkDataSize, chunkMetadataSize);

            THEN("parser parses metadata and informs that trailer is expected")
            {
                REQUIRE(parserStatus == HttpChunkMetadataParser::ChunkMetadataParserStatus::ExpectingTrailer);
                REQUIRE(chunkDataSize == 0);
                REQUIRE(chunkMetadataSize == (chunkMetadata.size() - 2));
            }
        }

        WHEN("chunk metadata is parsed byte by byte")
        {
            IOChannelTest ioChannel({});
            size_t chunkDataSize = 0;
            size_t chunkMetadataSize = 0;
            for (auto i = 0; i < (chunkMetadata.size() - 1); ++i)
            {
                ioChannel.readBuffer().write(&chunkMetadata[i], 1);
                REQUIRE(HttpChunkMetadataParser::parse(ioChannel, chunkDataSize, chunkMetadataSize) == HttpChunkMetadataParser::ChunkMetadataParserStatus::NeedsMoreData);
            }
            ioChannel.readBuffer().write(&chunkMetadata[chunkMetadata.size() - 1], 1);
            const auto parserStatus = HttpChunkMetadataParser::parse(ioChannel, chunkDataSize, chunkMetadataSize);

            THEN("parser parses metadata and informs that trailer is expected")
            {
                REQUIRE(parserStatus == HttpChunkMetadataParser::ChunkMetadataParserStatus::ExpectingTrailer);
                REQUIRE(chunkDataSize == 0);
                REQUIRE(chunkMetadataSize == (chunkMetadata.size() - 2));
            }
        }
    }
}
