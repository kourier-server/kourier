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

#include "RingBuffer.h"
#include <Spectator.h>
#include <QRandomGenerator>
#include <string_view>
#include <cstring>


using Kourier::RingBuffer;
using Kourier::DataSource;
using Kourier::DataSink;


SCENARIO("RingBuffer uses unlimited capacity by default")
{
    GIVEN("a default-constructed RingBuffer")
    {
        RingBuffer ringBuffer;

        WHEN("buffer's capacity is queried")
        {
            const auto capacity = ringBuffer.capacity();

            THEN("buffer has inital capacity equal to zero and available free size equal to default capacity")
            {
                REQUIRE(capacity == 0);
                REQUIRE(ringBuffer.availableFreeSize() == RingBuffer::defaultCapacity());
                REQUIRE(ringBuffer.isEmpty());
                REQUIRE(ringBuffer.size() == 0);
            }
        }
    }

    GIVEN("a capacity")
    {
        const auto capacity = GENERATE(AS(size_t), 0, 1, 15, 32, 128, 256);

        WHEN("buffer is constructed with given capacity")
        {
            RingBuffer ringBuffer(capacity);

            THEN("buffer has capacity given in constructor and expected available free size")
            {
                REQUIRE(ringBuffer.capacity() == capacity);
                const size_t expectedAvailableFreeSize = (capacity == 0) ? RingBuffer::defaultCapacity() : std::min<size_t>(capacity, RingBuffer::defaultCapacity());
                REQUIRE(ringBuffer.availableFreeSize() == expectedAvailableFreeSize);
                REQUIRE(ringBuffer.isEmpty());
                REQUIRE(ringBuffer.size() == 0);
            }
        }
    }
}


SCENARIO("RingBuffer allows setting capacity after creation")
{
    GIVEN("an empty ring buffer with unlimited capacity")
    {
        RingBuffer ringBuffer;
        REQUIRE(ringBuffer.capacity() == 0);
        REQUIRE(ringBuffer.isEmpty());
        REQUIRE(ringBuffer.size() == 0);

        WHEN("capacity is set to a value")
        {
            const auto newCapacity = GENERATE(AS(size_t), 0, 1, 8, 1024);
            bool succedded = ringBuffer.setCapacity(newCapacity);

            THEN("capacity is successfully set")
            {
                REQUIRE(succedded);
                REQUIRE(ringBuffer.capacity() == newCapacity);
                REQUIRE(ringBuffer.isEmpty());
                REQUIRE(ringBuffer.size() == 0);
            }
        }
    }

    GIVEN("a non-empty ring buffer with unlimited capacity")
    {
        static constexpr size_t initialDataSize = 20;
        RingBuffer ringBuffer;
        REQUIRE(ringBuffer.isEmpty());
        REQUIRE(ringBuffer.size() == 0);
        REQUIRE(ringBuffer.capacity() == 0);
        REQUIRE(ringBuffer.availableFreeSize() == RingBuffer::defaultCapacity());
        const auto initialData = []() -> std::string
        {
            std::string tmp(initialDataSize, ' ');
            for (auto &ch : tmp)
                ch = QRandomGenerator64::global()->bounded(0, 256);
            return tmp;
        }();
        enum class DataDisposition {Start, Middle, End, Splitted};
        const auto dataDisposition = GENERATE(AS(DataDisposition),
                                              DataDisposition::Start,
                                              DataDisposition::Middle,
                                              DataDisposition::End,
                                              DataDisposition::Splitted);
        switch (dataDisposition)
        {
            case DataDisposition::Start:
                REQUIRE(initialDataSize == ringBuffer.write(initialData.data(), initialData.size()));
                break;
            case DataDisposition::Middle:
            {
                REQUIRE(RingBuffer::defaultCapacity() > initialDataSize);
                std::string tmp((RingBuffer::defaultCapacity() - initialDataSize) >> 1, ' ');
                REQUIRE(tmp.size() == ringBuffer.write(tmp.data(), tmp.size()));
                REQUIRE(initialDataSize == ringBuffer.write(initialData.data(), initialData.size()));
                REQUIRE(tmp.size() == ringBuffer.popFront(tmp.size()));
                break;
            }
            case DataDisposition::End:
            {
                REQUIRE(RingBuffer::defaultCapacity() > initialDataSize);
                std::string tmp(RingBuffer::defaultCapacity() - initialDataSize, ' ');
                REQUIRE(tmp.size() == ringBuffer.write(tmp.data(), tmp.size()));
                REQUIRE(initialDataSize == ringBuffer.write(initialData.data(), initialData.size()));
                REQUIRE(tmp.size() == ringBuffer.popFront(tmp.size()));
                break;
            }
            case DataDisposition::Splitted:
            {
                REQUIRE(RingBuffer::defaultCapacity() > initialDataSize);
                std::string tmp(RingBuffer::defaultCapacity() - initialDataSize/2, ' ');
                REQUIRE(tmp.size() == ringBuffer.write(tmp.data(), tmp.size()));
                REQUIRE((tmp.size() - 1) == ringBuffer.popFront(tmp.size() - 1));
                REQUIRE(!ringBuffer.isEmpty());
                REQUIRE(ringBuffer.size() == 1);
                REQUIRE(initialDataSize == ringBuffer.write(initialData.data(), initialData.size()));
                REQUIRE(1 == ringBuffer.popFront(1));
                break;
            }
        }
        REQUIRE(!ringBuffer.isEmpty());
        REQUIRE(ringBuffer.size() == initialDataSize);
        REQUIRE(ringBuffer.capacity() == 0);
        REQUIRE(ringBuffer.availableFreeSize() == (RingBuffer::defaultCapacity() - initialDataSize));
        REQUIRE(initialData == ringBuffer.peekAll());

        WHEN("capacity is set to zero")
        {
            const bool succeeded = ringBuffer.setCapacity(0);

            THEN("capacity is succesfully set to zero")
            {
                REQUIRE(succeeded);
                REQUIRE(!ringBuffer.isEmpty());
                REQUIRE(ringBuffer.size() == initialDataSize);
                REQUIRE(ringBuffer.capacity() == 0);
                REQUIRE(ringBuffer.availableFreeSize() == (RingBuffer::defaultCapacity() - initialDataSize));
                REQUIRE(initialData == ringBuffer.peekAll());
                REQUIRE(initialData == ringBuffer.readAll());
                REQUIRE(ringBuffer.isEmpty());
            }
        }

        WHEN("capacity is set to a positive value smaller than buffer's size")
        {
            const auto newCapacity = GENERATE_RANGE(AS(size_t), 1, initialDataSize - 1);
            const bool succeeded = ringBuffer.setCapacity(newCapacity);

            THEN("buffer fails to change its capacity")
            {
                REQUIRE(!succeeded);
                REQUIRE(!ringBuffer.isEmpty());
                REQUIRE(ringBuffer.size() == initialDataSize);
                REQUIRE(ringBuffer.capacity() == 0);
                REQUIRE(ringBuffer.availableFreeSize() == (RingBuffer::defaultCapacity() - initialDataSize));
                REQUIRE(initialData == ringBuffer.peekAll());
                REQUIRE(initialData == ringBuffer.readAll());
                REQUIRE(ringBuffer.isEmpty());
            }
        }

        WHEN("capacity is set to a positive value equal to or greater than buffer's size but smaller than buffer's default capacity")
        {
            const auto newCapacity = GENERATE_RANGE(AS(size_t), initialDataSize, RingBuffer::defaultCapacity() - 1);
            const bool succeeded = ringBuffer.setCapacity(newCapacity);

            THEN("buffer changes its capacity and adjust its availableFreeSize")
            {
                REQUIRE(succeeded)
                REQUIRE(!ringBuffer.isEmpty());
                REQUIRE(ringBuffer.size() == initialDataSize);
                REQUIRE(ringBuffer.capacity() == newCapacity);
                REQUIRE(ringBuffer.availableFreeSize() < (RingBuffer::defaultCapacity() - initialDataSize));
                REQUIRE(ringBuffer.availableFreeSize() == (newCapacity - initialDataSize));
                REQUIRE(initialData == ringBuffer.peekAll());
                REQUIRE(initialData == ringBuffer.readAll());
                REQUIRE(ringBuffer.isEmpty());
            }
        }

        WHEN("capacity is set to a positive value equal to or greater than buffer's default capacity")
        {
            const auto newCapacity = GENERATE_RANGE(AS(size_t), RingBuffer::defaultCapacity(), RingBuffer::defaultCapacity() + 16);
            const bool succeeded = ringBuffer.setCapacity(newCapacity);

            THEN("buffer changes its capacity keep its availableFreeSize")
            {
                REQUIRE(succeeded)
                REQUIRE(!ringBuffer.isEmpty());
                REQUIRE(ringBuffer.size() == initialDataSize);
                REQUIRE(ringBuffer.capacity() == newCapacity);
                REQUIRE(ringBuffer.availableFreeSize() == (RingBuffer::defaultCapacity() - initialDataSize));
                REQUIRE(initialData == ringBuffer.peekAll());
                REQUIRE(initialData == ringBuffer.readAll());
                REQUIRE(ringBuffer.isEmpty());
            }
        }
    }

    GIVEN("a full ring buffer with unlimited capacity")
    {
        static constexpr size_t initialDataSize = RingBuffer::defaultCapacity();
        RingBuffer ringBuffer;
        REQUIRE(ringBuffer.isEmpty());
        REQUIRE(ringBuffer.size() == 0);
        REQUIRE(ringBuffer.capacity() == 0);
        REQUIRE(ringBuffer.availableFreeSize() == RingBuffer::defaultCapacity());
        const auto initialData = []() -> std::string
        {
            std::string tmp(initialDataSize, ' ');
            for (auto &ch : tmp)
                ch = QRandomGenerator64::global()->bounded(0, 256);
            return tmp;
        }();
        REQUIRE(initialDataSize == ringBuffer.write(initialData.data(), initialData.size()));
        REQUIRE(!ringBuffer.isEmpty());
        REQUIRE(ringBuffer.size() == initialDataSize);
        REQUIRE(ringBuffer.capacity() == 0);
        REQUIRE(ringBuffer.availableFreeSize() == 0);
        REQUIRE(initialData == ringBuffer.peekAll());

        WHEN("capacity is set to zero")
        {
            const bool succeeded = ringBuffer.setCapacity(0);

            THEN("capacity is succesfully set to zero")
            {
                REQUIRE(succeeded);
                REQUIRE(!ringBuffer.isEmpty());
                REQUIRE(ringBuffer.size() == initialDataSize);
                REQUIRE(ringBuffer.capacity() == 0);
                REQUIRE(ringBuffer.availableFreeSize() == 0);
                REQUIRE(initialData == ringBuffer.peekAll());
                REQUIRE(initialData == ringBuffer.readAll());
                REQUIRE(ringBuffer.isEmpty());
            }
        }

        WHEN("capacity is set to a positive value smaller than buffer's size")
        {
            const auto newCapacity = GENERATE_RANGE(AS(size_t), 1, initialDataSize - 1);
            const bool succeeded = ringBuffer.setCapacity(newCapacity);

            THEN("buffer fails to change its capacity")
            {
                REQUIRE(!succeeded);
                REQUIRE(!ringBuffer.isEmpty());
                REQUIRE(ringBuffer.size() == initialDataSize);
                REQUIRE(ringBuffer.capacity() == 0);
                REQUIRE(ringBuffer.availableFreeSize() == 0);
                REQUIRE(initialData == ringBuffer.peekAll());
                REQUIRE(initialData == ringBuffer.readAll());
                REQUIRE(ringBuffer.isEmpty());
            }
        }

        WHEN("capacity is set to a positive value equal to or greater than buffer's default capacity")
        {
            const auto newCapacity = GENERATE_RANGE(AS(size_t), RingBuffer::defaultCapacity(), RingBuffer::defaultCapacity() + 16);
            const bool succeeded = ringBuffer.setCapacity(newCapacity);

            THEN("buffer changes its capacity keep its availableFreeSize")
            {
                REQUIRE(succeeded)
                REQUIRE(!ringBuffer.isEmpty());
                REQUIRE(ringBuffer.size() == initialDataSize);
                REQUIRE(ringBuffer.capacity() == newCapacity);
                REQUIRE(ringBuffer.availableFreeSize() == 0);
                REQUIRE(initialData == ringBuffer.peekAll());
                REQUIRE(initialData == ringBuffer.readAll());
                REQUIRE(ringBuffer.isEmpty());
            }
        }
    }
}


SCENARIO("Ring buffers can be cleared at any time")
{
    GIVEN("an empty ring buffer with original available free space")
    {
        RingBuffer ringBuffer;
        REQUIRE(ringBuffer.capacity() == 0);
        REQUIRE(ringBuffer.isEmpty());
        REQUIRE(ringBuffer.size() == 0);
        REQUIRE(ringBuffer.availableFreeSize() == RingBuffer::defaultCapacity());

        WHEN("ring buffer is clear")
        {
            ringBuffer.clear();

            THEN("ring buffer's available free size is kept the same")
            {
                REQUIRE(ringBuffer.capacity() == 0);
                REQUIRE(ringBuffer.isEmpty());
                REQUIRE(ringBuffer.size() == 0);
                REQUIRE(ringBuffer.availableFreeSize() == RingBuffer::defaultCapacity());
                REQUIRE(ringBuffer.peekAll().empty());
                REQUIRE(ringBuffer.readAll().empty());
            }
        }
    }

    GIVEN("a non-empty ring buffer with original available free space")
    {
        RingBuffer ringBuffer;
        REQUIRE(ringBuffer.capacity() == 0);
        REQUIRE(ringBuffer.isEmpty());
        REQUIRE(ringBuffer.size() == 0);
        REQUIRE(ringBuffer.availableFreeSize() == RingBuffer::defaultCapacity());
        const auto initialDataSize = GENERATE_RANGE(AS(size_t), 1, RingBuffer::defaultCapacity());
        static const auto initialData = []() -> std::string
        {
            std::string tmp(RingBuffer::defaultCapacity(), ' ');
            for (auto &ch : tmp)
                ch = QRandomGenerator64::global()->bounded(0, 256);
            return tmp;
        }();
        REQUIRE(initialDataSize == ringBuffer.write(initialData.data(), initialDataSize));
        REQUIRE(ringBuffer.capacity() == 0);
        REQUIRE(!ringBuffer.isEmpty());
        REQUIRE(ringBuffer.size() == initialDataSize);
        REQUIRE(ringBuffer.availableFreeSize() == (RingBuffer::defaultCapacity() - initialDataSize));
        REQUIRE(std::string_view(initialData.data(), initialDataSize) == ringBuffer.peekAll());

        WHEN("ring buffer cleared")
        {
            ringBuffer.clear();

            THEN("ring buffer goes back to its creation state")
            {
                REQUIRE(ringBuffer.capacity() == 0);
                REQUIRE(ringBuffer.isEmpty());
                REQUIRE(ringBuffer.size() == 0);
                REQUIRE(ringBuffer.availableFreeSize() == RingBuffer::defaultCapacity());
                REQUIRE(ringBuffer.peekAll().empty());
                REQUIRE(ringBuffer.readAll().empty());
            }
        }
    }

    GIVEN("an empty ring buffer with available free space larger than original free space")
    {
        RingBuffer ringBuffer;
        REQUIRE(ringBuffer.capacity() == 0);
        REQUIRE(ringBuffer.isEmpty());
        REQUIRE(ringBuffer.size() == 0);
        REQUIRE(ringBuffer.availableFreeSize() == RingBuffer::defaultCapacity());
        const auto initialDataSize = GENERATE_RANGE(AS(size_t), RingBuffer::defaultCapacity() + 1, 2 * RingBuffer::defaultCapacity());
        std::string initialData(initialDataSize, ' ');
        for (auto &ch : initialData)
            ch = QRandomGenerator64::global()->bounded(0, 256);
        REQUIRE(initialDataSize == ringBuffer.write(initialData.data(), initialDataSize));
        REQUIRE(ringBuffer.capacity() == 0);
        REQUIRE(!ringBuffer.isEmpty());
        REQUIRE(ringBuffer.size() == initialDataSize);
        REQUIRE(initialData == ringBuffer.peekAll());
        REQUIRE(initialDataSize == ringBuffer.popFront(initialDataSize));
        REQUIRE(ringBuffer.capacity() == 0);
        REQUIRE(ringBuffer.isEmpty());
        REQUIRE(ringBuffer.size() == 0);
        REQUIRE(ringBuffer.availableFreeSize() > RingBuffer::defaultCapacity());

        WHEN("ring buffer cleared")
        {
            ringBuffer.clear();

            THEN("ring buffer is cleared back to its creation state")
            {
                REQUIRE(ringBuffer.capacity() == 0);
                REQUIRE(ringBuffer.isEmpty());
                REQUIRE(ringBuffer.size() == 0);
                REQUIRE(ringBuffer.availableFreeSize() == RingBuffer::defaultCapacity());
                REQUIRE(ringBuffer.peekAll().empty());
                REQUIRE(ringBuffer.readAll().empty());
            }
        }
    }

    GIVEN("a non-empty ring buffer with available free space twice the size of the original free space")
    {
        RingBuffer ringBuffer;
        REQUIRE(ringBuffer.capacity() == 0);
        REQUIRE(ringBuffer.isEmpty());
        REQUIRE(ringBuffer.size() == 0);
        REQUIRE(ringBuffer.availableFreeSize() == RingBuffer::defaultCapacity());
        std::string tmpData(2*RingBuffer::defaultCapacity(), ' ');
        for (auto &ch : tmpData)
            ch = QRandomGenerator64::global()->bounded(0, 256);
        REQUIRE(tmpData.size() == ringBuffer.write(tmpData.data(), tmpData.size()));
        REQUIRE(ringBuffer.capacity() == 0);
        REQUIRE(!ringBuffer.isEmpty());
        REQUIRE(ringBuffer.size() == tmpData.size());
        REQUIRE(tmpData == ringBuffer.peekAll());
        REQUIRE(tmpData.size() == ringBuffer.popFront(tmpData.size()));
        REQUIRE(ringBuffer.capacity() == 0);
        REQUIRE(ringBuffer.isEmpty());
        REQUIRE(ringBuffer.size() == 0);
        REQUIRE(ringBuffer.availableFreeSize() == (2 * RingBuffer::defaultCapacity()));
        const auto initialDataSize = GENERATE_RANGE(AS(size_t), 1, 2 * RingBuffer::defaultCapacity());
        static const auto initialData = []() -> std::string
        {
            std::string tmp(2 * RingBuffer::defaultCapacity(), ' ');
            for (auto &ch : tmp)
                ch = QRandomGenerator64::global()->bounded(0, 256);
            return tmp;
        }();
        REQUIRE(initialDataSize == ringBuffer.write(initialData.data(), initialDataSize));
        REQUIRE(ringBuffer.capacity() == 0);
        REQUIRE(!ringBuffer.isEmpty());
        REQUIRE(ringBuffer.size() == initialDataSize);
        REQUIRE(ringBuffer.availableFreeSize() == (2 * RingBuffer::defaultCapacity() - initialDataSize));
        REQUIRE(std::string_view(initialData.data(), initialDataSize) == ringBuffer.peekAll());

        WHEN("ring buffer cleared")
        {
            ringBuffer.clear();

            THEN("ring buffer is cleared back to its creation state")
            {
                REQUIRE(ringBuffer.capacity() == 0);
                REQUIRE(ringBuffer.isEmpty());
                REQUIRE(ringBuffer.size() == 0);
                REQUIRE(ringBuffer.availableFreeSize() == RingBuffer::defaultCapacity());
                REQUIRE(ringBuffer.peekAll().empty());
                REQUIRE(ringBuffer.readAll().empty());
            }
        }
    }
}


SCENARIO("Empty ring buffers can be reset back to their initial default capacity")
{
    GIVEN("an empty ring buffer with original available free space")
    {
        RingBuffer ringBuffer;
        REQUIRE(ringBuffer.capacity() == 0);
        REQUIRE(ringBuffer.isEmpty());
        REQUIRE(ringBuffer.size() == 0);
        REQUIRE(ringBuffer.peekAll().empty());
        REQUIRE(ringBuffer.availableFreeSize() == RingBuffer::defaultCapacity());

        WHEN("ring buffer is reset")
        {
            const auto succeeded = ringBuffer.reset();

            THEN("ring buffer is successfuly reset and its available free size is kept the same")
            {
                REQUIRE(succeeded);
                REQUIRE(ringBuffer.capacity() == 0);
                REQUIRE(ringBuffer.isEmpty());
                REQUIRE(ringBuffer.size() == 0);
                REQUIRE(ringBuffer.availableFreeSize() == RingBuffer::defaultCapacity());
                REQUIRE(ringBuffer.peekAll().empty());
                REQUIRE(ringBuffer.readAll().empty());
            }
        }
    }

    GIVEN("a non-empty ring buffer with original available free space")
    {
        RingBuffer ringBuffer;
        REQUIRE(ringBuffer.capacity() == 0);
        REQUIRE(ringBuffer.isEmpty());
        REQUIRE(ringBuffer.size() == 0);
        REQUIRE(ringBuffer.availableFreeSize() == RingBuffer::defaultCapacity());
        const auto initialDataSize = GENERATE_RANGE(AS(size_t), 1, RingBuffer::defaultCapacity());
        static const auto initialData = []() -> std::string
        {
            std::string tmp(RingBuffer::defaultCapacity(), ' ');
            for (auto &ch : tmp)
                ch = QRandomGenerator64::global()->bounded(0, 256);
            return tmp;
        }();
        REQUIRE(initialDataSize == ringBuffer.write(initialData.data(), initialDataSize));
        REQUIRE(ringBuffer.capacity() == 0);
        REQUIRE(!ringBuffer.isEmpty());
        REQUIRE(ringBuffer.size() == initialDataSize);
        REQUIRE(ringBuffer.availableFreeSize() == (RingBuffer::defaultCapacity() - initialDataSize));
        REQUIRE(std::string_view(initialData.data(), initialDataSize) == ringBuffer.peekAll());

        WHEN("ring buffer is reset")
        {
            const auto succeeded = ringBuffer.reset();

            THEN("ring buffer fails to reset")
            {
                REQUIRE(!succeeded);
                REQUIRE(ringBuffer.capacity() == 0);
                REQUIRE(!ringBuffer.isEmpty());
                REQUIRE(ringBuffer.size() == initialDataSize);
                REQUIRE(ringBuffer.availableFreeSize() == (RingBuffer::defaultCapacity() - initialDataSize));
                REQUIRE(std::string_view(initialData.data(), initialDataSize) == ringBuffer.peekAll());
                REQUIRE(std::string_view(initialData.data(), initialDataSize) == ringBuffer.readAll());
                REQUIRE(ringBuffer.isEmpty());
            }
        }
    }

    GIVEN("an empty ring buffer with available free space larger than original free space")
    {
        RingBuffer ringBuffer;
        REQUIRE(ringBuffer.capacity() == 0);
        REQUIRE(ringBuffer.isEmpty());
        REQUIRE(ringBuffer.size() == 0);
        REQUIRE(ringBuffer.peekAll().empty());
        REQUIRE(ringBuffer.readAll().empty());
        REQUIRE(ringBuffer.peekAll().empty());
        REQUIRE(ringBuffer.availableFreeSize() == RingBuffer::defaultCapacity());
        const auto initialDataSize = GENERATE_RANGE(AS(size_t), RingBuffer::defaultCapacity() + 1, 2 * RingBuffer::defaultCapacity());
        std::string initialData(initialDataSize, ' ');
        for (auto &ch : initialData)
            ch = QRandomGenerator64::global()->bounded(0, 256);
        REQUIRE(initialDataSize == ringBuffer.write(initialData.data(), initialDataSize));
        REQUIRE(ringBuffer.capacity() == 0);
        REQUIRE(!ringBuffer.isEmpty());
        REQUIRE(ringBuffer.size() == initialDataSize);
        REQUIRE(initialData == ringBuffer.peekAll());
        REQUIRE(initialDataSize == ringBuffer.popFront(initialDataSize));
        REQUIRE(ringBuffer.capacity() == 0);
        REQUIRE(ringBuffer.isEmpty());
        REQUIRE(ringBuffer.size() == 0);
        REQUIRE(ringBuffer.availableFreeSize() > RingBuffer::defaultCapacity());

        WHEN("ring buffer is reset")
        {
            const auto succeeded = ringBuffer.reset();

            THEN("ring buffer is successfuly reset and its available free size is reset to default capacity")
            {
                REQUIRE(succeeded);
                REQUIRE(ringBuffer.capacity() == 0);
                REQUIRE(ringBuffer.isEmpty());
                REQUIRE(ringBuffer.size() == 0);
                REQUIRE(ringBuffer.availableFreeSize() == RingBuffer::defaultCapacity());
                REQUIRE(ringBuffer.peekAll().empty());
                REQUIRE(ringBuffer.readAll().empty());
            }
        }
    }

    GIVEN("a non-empty ring buffer with available free space twice the size of the original free space")
    {
        RingBuffer ringBuffer;
        REQUIRE(ringBuffer.capacity() == 0);
        REQUIRE(ringBuffer.isEmpty());
        REQUIRE(ringBuffer.size() == 0);
        REQUIRE(ringBuffer.availableFreeSize() == RingBuffer::defaultCapacity());
        std::string tmpData(2*RingBuffer::defaultCapacity(), ' ');
        for (auto &ch : tmpData)
            ch = QRandomGenerator64::global()->bounded(0, 256);
        REQUIRE(tmpData.size() == ringBuffer.write(tmpData.data(), tmpData.size()));
        REQUIRE(ringBuffer.capacity() == 0);
        REQUIRE(!ringBuffer.isEmpty());
        REQUIRE(ringBuffer.size() == tmpData.size());
        REQUIRE(tmpData == ringBuffer.peekAll());
        REQUIRE(tmpData.size() == ringBuffer.popFront(tmpData.size()));
        REQUIRE(ringBuffer.capacity() == 0);
        REQUIRE(ringBuffer.isEmpty());
        REQUIRE(ringBuffer.size() == 0);
        REQUIRE(ringBuffer.availableFreeSize() == (2 * RingBuffer::defaultCapacity()));
        const auto initialDataSize = GENERATE_RANGE(AS(size_t), 1, 2 * RingBuffer::defaultCapacity());
        static const auto initialData = []() -> std::string
        {
            std::string tmp(2 * RingBuffer::defaultCapacity(), ' ');
            for (auto &ch : tmp)
                ch = QRandomGenerator64::global()->bounded(0, 256);
            return tmp;
        }();
        REQUIRE(initialDataSize == ringBuffer.write(initialData.data(), initialDataSize));
        REQUIRE(ringBuffer.capacity() == 0);
        REQUIRE(!ringBuffer.isEmpty());
        REQUIRE(ringBuffer.size() == initialDataSize);
        REQUIRE(ringBuffer.availableFreeSize() == (2 * RingBuffer::defaultCapacity() - initialDataSize));
        REQUIRE(std::string_view(initialData.data(), initialDataSize) == ringBuffer.peekAll());

        WHEN("ring buffer is reset")
        {
            const auto succeeded = ringBuffer.reset();

            THEN("ring buffer fails to reset")
            {
                REQUIRE(!succeeded);
                REQUIRE(ringBuffer.capacity() == 0);
                REQUIRE(!ringBuffer.isEmpty());
                REQUIRE(ringBuffer.size() == initialDataSize);
                REQUIRE(ringBuffer.availableFreeSize() == (2 * RingBuffer::defaultCapacity() - initialDataSize));
                REQUIRE(std::string_view(initialData.data(), initialDataSize) == ringBuffer.peekAll());
                REQUIRE(std::string_view(initialData.data(), initialDataSize) == ringBuffer.readAll());
                REQUIRE(ringBuffer.isEmpty());
            }
        }
    }
}


SCENARIO("RingBuffer supports data IO")
{
    GIVEN("a ring buffer")
    {
        const auto capacity = GENERATE(AS(size_t), RingBuffer::defaultCapacity(),
                                       2 * RingBuffer::defaultCapacity(),
                                       0,
                                       300,
                                       64);
        RingBuffer ringBuffer(capacity);
        REQUIRE(ringBuffer.isEmpty());
        REQUIRE(ringBuffer.size() == 0);
        REQUIRE(ringBuffer.capacity() == capacity);
        REQUIRE(ringBuffer.availableFreeSize() == ((capacity == 0) ? RingBuffer::defaultCapacity() : std::min(capacity, RingBuffer::defaultCapacity())));
        const auto initialDataSize = GENERATE(AS(size_t), 16, 17, 23, 32);
        const auto dataSizeToChangeAfterEachIteration = GENERATE(AS(size_t), 16, 1, 5);
        const auto popFrontInsteadOfReading = GENERATE(AS(bool), true, false);
        const auto enlargeAvailableFreeSize = GENERATE(AS(bool), true, false);
        if (enlargeAvailableFreeSize && ((capacity == 0) || (capacity >= (RingBuffer::defaultCapacity() + 16))))
        {
            std::string tmpData(RingBuffer::defaultCapacity() + 16, ' ');
            for (auto &ch : tmpData)
                ch = QRandomGenerator64::global()->bounded(0, 256);
            REQUIRE(tmpData.size() == ringBuffer.write(tmpData.data(), tmpData.size()));
            REQUIRE(!ringBuffer.isEmpty());
            REQUIRE(ringBuffer.size() == tmpData.size());
            REQUIRE(ringBuffer.capacity() == capacity);
            REQUIRE(tmpData == ringBuffer.peekAll());
            REQUIRE(tmpData.size() == ringBuffer.popFront(tmpData.size()));
            REQUIRE(ringBuffer.isEmpty());
            REQUIRE(ringBuffer.size() == 0);
            REQUIRE(ringBuffer.capacity() == capacity);
            REQUIRE(ringBuffer.availableFreeSize() > RingBuffer::defaultCapacity());
        }

        WHEN("data is continuously added and removed from buffer")
        {
            THEN("buffer handles data IO correctly")
            {
                std::string expectedDataInBuffer = [initialDataSize]() -> std::string
                {
                    std::string tmp(initialDataSize, ' ');
                    for (auto &ch : tmp)
                        ch = QRandomGenerator::global()->bounded(0, 256);
                    return tmp;
                }();
                expectedDataInBuffer.reserve(10*capacity);
                const auto previousAvailableFreeSize = ringBuffer.availableFreeSize();
                REQUIRE(initialDataSize == ringBuffer.write(expectedDataInBuffer.data(), expectedDataInBuffer.size()));
                REQUIRE(!ringBuffer.isEmpty());
                REQUIRE(ringBuffer.size() == expectedDataInBuffer.size());
                REQUIRE(ringBuffer.capacity() == capacity);
                REQUIRE(ringBuffer.availableFreeSize() == (previousAvailableFreeSize - expectedDataInBuffer.size()));
                REQUIRE(expectedDataInBuffer == ringBuffer.peekAll());
                size_t addedData = expectedDataInBuffer.size();
                const auto sizeToAdd = 10 * ((capacity > 0) ? capacity : std::min<size_t>(RingBuffer::defaultCapacity(), capacity));
                do
                {
                    const auto previousAvailableFreeSize = ringBuffer.availableFreeSize();
                    if (popFrontInsteadOfReading)
                    {
                        REQUIRE(dataSizeToChangeAfterEachIteration == ringBuffer.popFront(dataSizeToChangeAfterEachIteration));
                    }
                    else
                    {
                        std::string tmp(dataSizeToChangeAfterEachIteration, ' ');
                        REQUIRE(tmp.size() == ringBuffer.read(tmp.data(), tmp.size()));
                        REQUIRE(expectedDataInBuffer.starts_with(tmp));
                    }
                    REQUIRE(ringBuffer.isEmpty() == (dataSizeToChangeAfterEachIteration == expectedDataInBuffer.size()));
                    REQUIRE(ringBuffer.size() == (expectedDataInBuffer.size() - dataSizeToChangeAfterEachIteration));
                    REQUIRE(ringBuffer.capacity() == capacity);
                    REQUIRE(ringBuffer.availableFreeSize() == (previousAvailableFreeSize + dataSizeToChangeAfterEachIteration));
                    expectedDataInBuffer.erase(0, dataSizeToChangeAfterEachIteration);
                    std::string randomDataToAdd(dataSizeToChangeAfterEachIteration, ' ');
                    for (auto &ch : randomDataToAdd)
                        ch = QRandomGenerator::global()->bounded(0, 256);
                    expectedDataInBuffer.append(randomDataToAdd);
                    REQUIRE(dataSizeToChangeAfterEachIteration == ringBuffer.write(randomDataToAdd.data(), randomDataToAdd.size()));
                    REQUIRE(!ringBuffer.isEmpty());
                    REQUIRE(ringBuffer.size() == expectedDataInBuffer.size());
                    REQUIRE(ringBuffer.capacity() == capacity);
                    REQUIRE(ringBuffer.availableFreeSize() == previousAvailableFreeSize);
                    REQUIRE(expectedDataInBuffer == ringBuffer.peekAll());
                    for (auto pos = 0; pos < expectedDataInBuffer.size(); ++pos)
                    {
                        for (auto count = (expectedDataInBuffer.size() - pos); count > 0; --count)
                        {
                            REQUIRE(std::string_view(expectedDataInBuffer.data() + pos, count) == ringBuffer.slice(pos, count));
                        }
                    }
                    for (auto i = 0; i < expectedDataInBuffer.size(); ++i)
                    {
                        REQUIRE(expectedDataInBuffer[i] == ringBuffer.peekChar(i));
                    }
                    addedData += dataSizeToChangeAfterEachIteration;
                }
                while (addedData < sizeToAdd);
            }
        }
    }

    GIVEN("a ring buffer with data at the beginning")
    {
        const auto dataSize = GENERATE_RANGE(AS(size_t), 1, RingBuffer::defaultCapacity());

        WHEN("data is peek/read from buffer")
        {
            THEN("read operation fetches data correctly")
            {
                static const std::string dataToWrite = []() -> std::string
                {
                    std::string tmp(RingBuffer::defaultCapacity(), ' ');
                    for (auto &ch : tmp)
                        ch = QRandomGenerator::global()->bounded(0, 256);
                    return tmp;
                }();

                for (auto i = 1; i <= dataSize; ++i)
                {
                    RingBuffer ringBuffer(RingBuffer::defaultCapacity());
                    REQUIRE(ringBuffer.isEmpty());
                    REQUIRE(ringBuffer.size() == 0);
                    REQUIRE(ringBuffer.capacity() == RingBuffer::defaultCapacity());
                    REQUIRE(ringBuffer.availableFreeSize() == RingBuffer::defaultCapacity());
                    REQUIRE(dataSize == ringBuffer.write(dataToWrite.data(), dataSize));
                    REQUIRE(!ringBuffer.isEmpty());
                    REQUIRE(ringBuffer.size() == dataSize);
                    REQUIRE(ringBuffer.capacity() == RingBuffer::defaultCapacity());
                    REQUIRE(ringBuffer.availableFreeSize() == (RingBuffer::defaultCapacity() - dataSize));
                    const auto expectedData = std::string_view(dataToWrite.data(), i);
                    for (auto pos = 0; pos < expectedData.size(); ++pos)
                    {
                        for (auto count = (expectedData.size() - pos); count > 0; --count)
                        {
                            REQUIRE(std::string_view(expectedData.data() + pos, count) == ringBuffer.slice(pos, count));
                        }
                    }
                    for (auto i = 0; i < expectedData.size(); ++i)
                    {
                        REQUIRE(expectedData[i] == ringBuffer.peekChar(i));
                    }
                    std::string readData(i, ' ');
                    REQUIRE(i == ringBuffer.read(readData.data(), readData.size()));
                    REQUIRE(readData == expectedData);
                    REQUIRE(ringBuffer.isEmpty() == (i == dataSize));
                    REQUIRE(ringBuffer.size() == (dataSize - i));
                    REQUIRE(ringBuffer.capacity() == RingBuffer::defaultCapacity());
                    REQUIRE(ringBuffer.availableFreeSize() == (RingBuffer::defaultCapacity() - dataSize + i));
                }
            }
        }
    }

    GIVEN("a ring buffer with data at the end")
    {
        const auto dataGap = GENERATE_RANGE(AS(size_t), 1, RingBuffer::defaultCapacity() - 1);
        const auto dataSize = RingBuffer::defaultCapacity() - dataGap;

        WHEN("data is peek/read from buffer")
        {
            THEN("read operation fetches data correctly")
            {
                static const std::string dataToWrite = []() -> std::string
                {
                    std::string tmp(RingBuffer::defaultCapacity(), ' ');
                    for (auto &ch : tmp)
                        ch = QRandomGenerator::global()->bounded(0, 256);
                    return tmp;
                }();

                for (auto i = 1; i <= dataSize; ++i)
                {
                    RingBuffer ringBuffer(RingBuffer::defaultCapacity());
                    REQUIRE(ringBuffer.isEmpty());
                    REQUIRE(ringBuffer.size() == 0);
                    REQUIRE(ringBuffer.capacity() == RingBuffer::defaultCapacity());
                    REQUIRE(ringBuffer.availableFreeSize() == RingBuffer::defaultCapacity());
                    REQUIRE(dataToWrite.size() == ringBuffer.write(dataToWrite.data(), dataToWrite.size()));
                    REQUIRE(!ringBuffer.isEmpty());
                    REQUIRE(ringBuffer.size() == dataToWrite.size());
                    REQUIRE(ringBuffer.capacity() == RingBuffer::defaultCapacity());
                    REQUIRE(ringBuffer.availableFreeSize() == 0);
                    REQUIRE(dataToWrite == ringBuffer.peekAll());
                    REQUIRE(dataGap == ringBuffer.popFront(dataGap));
                    REQUIRE(!ringBuffer.isEmpty());
                    REQUIRE(ringBuffer.size() == dataSize);
                    REQUIRE(ringBuffer.capacity() == RingBuffer::defaultCapacity());
                    REQUIRE(ringBuffer.availableFreeSize() == dataGap);
                    REQUIRE(std::string_view(dataToWrite.data() + dataGap, dataSize) == ringBuffer.peekAll());
                    const auto expectedData = std::string_view(dataToWrite.data() + dataGap, i);
                    for (auto pos = 0; pos < expectedData.size(); ++pos)
                    {
                        for (auto count = (expectedData.size() - pos); count > 0; --count)
                        {
                            REQUIRE(std::string_view(expectedData.data() + pos, count) == ringBuffer.slice(pos, count));
                        }
                    }
                    for (auto i = 0; i < expectedData.size(); ++i)
                    {
                        REQUIRE(expectedData[i] == ringBuffer.peekChar(i));
                    }
                    std::string readData(i, ' ');
                    REQUIRE(i == ringBuffer.read(readData.data(), readData.size()));
                    REQUIRE(readData == expectedData);
                    REQUIRE(ringBuffer.isEmpty() == (i == dataSize));
                    REQUIRE(ringBuffer.size() == (dataSize - i));
                    REQUIRE(ringBuffer.capacity() == RingBuffer::defaultCapacity());
                    REQUIRE(ringBuffer.availableFreeSize() == (dataGap + i));
                }
            }
        }
    }

    GIVEN("a ring buffer with data in the middle")
    {
        const auto dataGap = GENERATE_RANGE(AS(size_t), 1, 2*RingBuffer::defaultCapacity()/3 - 1);
        const auto dataSize = RingBuffer::defaultCapacity()/3;

        WHEN("data is peek/read from buffer")
        {
            THEN("read operation fetches data correctly")
            {
                static const std::string dataToWrite = []() -> std::string
                {
                    std::string tmp(RingBuffer::defaultCapacity(), ' ');
                    for (auto &ch : tmp)
                        ch = QRandomGenerator::global()->bounded(0, 256);
                    return tmp;
                }();

                for (auto i = 1; i <= dataSize; ++i)
                {
                    RingBuffer ringBuffer(RingBuffer::defaultCapacity());
                    REQUIRE(ringBuffer.isEmpty());
                    REQUIRE(ringBuffer.size() == 0);
                    REQUIRE(ringBuffer.capacity() == RingBuffer::defaultCapacity());
                    REQUIRE(ringBuffer.availableFreeSize() == RingBuffer::defaultCapacity());
                    REQUIRE((dataGap + dataSize) == ringBuffer.write(dataToWrite.data(), dataGap + dataSize));
                    REQUIRE(!ringBuffer.isEmpty());
                    REQUIRE(ringBuffer.size() == (dataGap + dataSize));
                    REQUIRE(ringBuffer.capacity() == RingBuffer::defaultCapacity());
                    REQUIRE(ringBuffer.availableFreeSize() == (RingBuffer::defaultCapacity() - dataGap - dataSize));
                    REQUIRE(dataGap == ringBuffer.popFront(dataGap));
                    REQUIRE(!ringBuffer.isEmpty());
                    REQUIRE(ringBuffer.size() == dataSize);
                    REQUIRE(ringBuffer.capacity() == RingBuffer::defaultCapacity());
                    REQUIRE(ringBuffer.availableFreeSize() == (RingBuffer::defaultCapacity() - dataSize));
                    REQUIRE(std::string_view(dataToWrite.data() + dataGap, dataSize) == ringBuffer.peekAll());
                    const auto expectedData = std::string_view(dataToWrite.data() + dataGap, i);
                    for (auto pos = 0; pos < expectedData.size(); ++pos)
                    {
                        for (auto count = (expectedData.size() - pos); count > 0; --count)
                        {
                            REQUIRE(std::string_view(expectedData.data() + pos, count) == ringBuffer.slice(pos, count));
                        }
                    }
                    for (auto i = 0; i < expectedData.size(); ++i)
                    {
                        REQUIRE(expectedData[i] == ringBuffer.peekChar(i));
                    }
                    std::string readData(i, ' ');
                    REQUIRE(i == ringBuffer.read(readData.data(), readData.size()));
                    REQUIRE(readData == expectedData);
                    REQUIRE(ringBuffer.isEmpty() == (i == dataSize));
                    REQUIRE(ringBuffer.size() == (dataSize - i));
                    REQUIRE(ringBuffer.capacity() == RingBuffer::defaultCapacity());
                    REQUIRE(ringBuffer.availableFreeSize() == (RingBuffer::defaultCapacity() - dataSize + i));
                }
            }
        }
    }

    GIVEN("a ring buffer with splitted data")
    {
        WHEN("data is peek/read from buffer")
        {
            THEN("read operation fetches data correctly")
            {
                static const std::string dataToWrite = []() -> std::string
                {
                    std::string tmp(RingBuffer::defaultCapacity(), ' ');
                    for (auto &ch : tmp)
                        ch = QRandomGenerator::global()->bounded(0, 256);
                    return tmp;
                }();

                for (auto pos = 1; pos < (RingBuffer::defaultCapacity() - 1); ++pos)
                {
                    for (auto size = 1; size < (RingBuffer::defaultCapacity() - pos); ++size)
                    {
                        for (auto dataToRead = 1; dataToRead <= (RingBuffer::defaultCapacity() - size); ++dataToRead)
                        {
                            RingBuffer ringBuffer(RingBuffer::defaultCapacity());
                            REQUIRE(ringBuffer.isEmpty());
                            REQUIRE(ringBuffer.size() == 0);
                            REQUIRE(ringBuffer.capacity() == RingBuffer::defaultCapacity());
                            REQUIRE(ringBuffer.availableFreeSize() == RingBuffer::defaultCapacity());
                            REQUIRE((pos + size) == ringBuffer.write(dataToWrite.data(), pos + size));
                            REQUIRE(!ringBuffer.isEmpty());
                            REQUIRE(ringBuffer.size() == (pos + size));
                            REQUIRE(ringBuffer.capacity() == RingBuffer::defaultCapacity());
                            REQUIRE(ringBuffer.availableFreeSize() == (RingBuffer::defaultCapacity() - pos - size));
                            REQUIRE((dataToWrite.size() - (pos + size)) == ringBuffer.write(dataToWrite.data(), dataToWrite.size() - (pos + size)));
                            REQUIRE(!ringBuffer.isEmpty());
                            REQUIRE(ringBuffer.size() == RingBuffer::defaultCapacity());
                            REQUIRE(ringBuffer.capacity() == RingBuffer::defaultCapacity());
                            REQUIRE(ringBuffer.availableFreeSize() == 0);
                            REQUIRE((pos + size) == ringBuffer.popFront(pos + size));
                            REQUIRE(!ringBuffer.isEmpty());
                            REQUIRE(ringBuffer.size() == (RingBuffer::defaultCapacity() - (pos + size)));
                            REQUIRE(ringBuffer.capacity() == RingBuffer::defaultCapacity());
                            REQUIRE(ringBuffer.availableFreeSize() == (pos + size));
                            REQUIRE(pos == ringBuffer.write(dataToWrite.data() + dataToWrite.size() - (pos + size), pos));
                            REQUIRE(!ringBuffer.isEmpty());
                            REQUIRE(ringBuffer.size() == (RingBuffer::defaultCapacity() - size));
                            REQUIRE(ringBuffer.capacity() == RingBuffer::defaultCapacity());
                            REQUIRE(ringBuffer.availableFreeSize() == size);
                            REQUIRE(std::string_view(dataToWrite.data(), RingBuffer::defaultCapacity() - size) == ringBuffer.peekAll());
                            const auto expectedData = std::string_view(dataToWrite.data(), dataToRead);
                            for (auto pos = 0; pos < expectedData.size(); ++pos)
                            {
                                for (auto count = (expectedData.size() - pos); count > 0; --count)
                                {
                                    REQUIRE(std::string_view(expectedData.data() + pos, count) == ringBuffer.slice(pos, count));
                                }
                            }
                            for (auto i = 0; i < expectedData.size(); ++i)
                            {
                                REQUIRE(expectedData[i] == ringBuffer.peekChar(i));
                            }
                            std::string readData(dataToRead, ' ');
                            REQUIRE(dataToRead == ringBuffer.read(readData.data(), readData.size()));
                            REQUIRE(readData == expectedData);
                            REQUIRE(ringBuffer.isEmpty() == (dataToRead == (RingBuffer::defaultCapacity() - size)));
                            REQUIRE(ringBuffer.size() == (RingBuffer::defaultCapacity() - size - dataToRead));
                            REQUIRE(ringBuffer.capacity() == RingBuffer::defaultCapacity());
                            REQUIRE(ringBuffer.availableFreeSize() == (size + dataToRead));
                        }
                    }
                }
            }
        }
    }
}


SCENARIO("RingBuffer enlarges buffer when writing data")
{
    GIVEN("a buffer constructed according to an initial data policy")
    {
        enum class InitialDataPolicy {Empty, Full, Start, End, Middle, Splitted};
        auto bufferInitializer = [](InitialDataPolicy dataPolicy, RingBuffer &ringBuffer, std::string &data)
        {
            REQUIRE(ringBuffer.isEmpty());
            const auto currentCapacity = ringBuffer.availableFreeSize();
            auto dataGenerator = [](size_t size) -> std::string
            {
                REQUIRE(size > 0);
                std::string tmp(size, ' ');
                for (auto &ch : tmp)
                    ch = QRandomGenerator::global()->bounded(0, 256);
                return tmp;
            };
            switch (dataPolicy)
            {
                case InitialDataPolicy::Empty:
                    data.clear();
                    break;
                case InitialDataPolicy::Full:
                    data = dataGenerator(currentCapacity);
                    REQUIRE(ringBuffer.write(data.data(), data.size()));
                    break;
                case InitialDataPolicy::Start:
                    data = dataGenerator(currentCapacity/3);
                    REQUIRE(ringBuffer.write(data.data(), data.size()));
                    break;
                case InitialDataPolicy::End:
                    data = dataGenerator((2*currentCapacity)/3);
                    REQUIRE(ringBuffer.write(data.data(), data.size()));
                    data = dataGenerator(currentCapacity - ringBuffer.size());
                    REQUIRE(ringBuffer.write(data.data(), data.size()));
                    ringBuffer.popFront((2*currentCapacity)/3);
                    break;
                case InitialDataPolicy::Middle:
                    data = dataGenerator(currentCapacity/3);
                    REQUIRE(ringBuffer.write(data.data(), data.size()));
                    data = dataGenerator(currentCapacity/3);
                    REQUIRE(ringBuffer.write(data.data(), data.size()));
                    ringBuffer.popFront(currentCapacity/3);
                    break;
                case InitialDataPolicy::Splitted:
                    data = dataGenerator((2*currentCapacity)/3);
                    REQUIRE(ringBuffer.write(data.data(), data.size()));
                    data = dataGenerator(currentCapacity - ringBuffer.size());
                    REQUIRE(ringBuffer.write(data.data(), data.size()));
                    ringBuffer.popFront((2*currentCapacity)/3);
                    {
                        auto tailData = dataGenerator(currentCapacity/3);
                        ringBuffer.write(tailData.data(), tailData.size());
                        data.append(tailData);
                    }
                    break;
            }
        };
        const auto policy = GENERATE(AS(InitialDataPolicy),
                                     InitialDataPolicy::Empty,
                                     InitialDataPolicy::Full,
                                     InitialDataPolicy::Start,
                                     InitialDataPolicy::End,
                                     InitialDataPolicy::Middle,
                                     InitialDataPolicy::Splitted);
        const auto capacity = GENERATE(AS(size_t), 2 * RingBuffer::defaultCapacity());
        RingBuffer ringBuffer(capacity);
        REQUIRE(ringBuffer.isEmpty());
        REQUIRE(ringBuffer.size() == 0);
        REQUIRE(ringBuffer.capacity() == capacity);
        REQUIRE(ringBuffer.availableFreeSize() == RingBuffer::defaultCapacity());
        std::string data;
        bufferInitializer(policy, ringBuffer, data);
        REQUIRE(ringBuffer.isEmpty() == (policy == InitialDataPolicy::Empty));
        REQUIRE(ringBuffer.size() == data.size());
        REQUIRE(ringBuffer.capacity() == capacity);
        REQUIRE(ringBuffer.availableFreeSize() == (RingBuffer::defaultCapacity() - data.size()));

        WHEN("data up to buffer's free space is written")
        {
            auto dataGenerator = [](size_t size) -> std::string
            {
                std::string tmp;
                if (size)
                {
                    tmp.resize(size);
                    for (auto &ch : tmp)
                        ch = QRandomGenerator::global()->bounded(0, 256);
                }
                return tmp;
            };
            const auto extraData = dataGenerator(ringBuffer.availableFreeSize());
            const bool succeeded = extraData.empty() || (extraData.size() == ringBuffer.write(extraData.data(), extraData.size()));

            THEN("write succeeds and buffers does not enlarge it's capacity")
            {
                REQUIRE(succeeded);
                REQUIRE(!ringBuffer.isEmpty());
                REQUIRE(ringBuffer.size() == (data.size() + extraData.size()));
                REQUIRE(ringBuffer.capacity() == capacity);
                REQUIRE(ringBuffer.availableFreeSize() == 0);
                REQUIRE((data + extraData) == ringBuffer.peekAll());

                AND_WHEN("any more data is written to buffer")
                {
                    const auto moreDataSize = GENERATE(AS(size_t), 1, 5, 12);
                    const auto moreData = dataGenerator(moreDataSize);
                    const bool succeededToWriteMoreData = (moreData.size() == ringBuffer.write(moreData.data(), moreData.size()));

                    THEN("write succeeds and buffer enlarges its capacity")
                    {
                        REQUIRE(succeededToWriteMoreData);
                        const auto expectedDataInBuffer = (data + extraData + moreData);
                        REQUIRE(!ringBuffer.isEmpty());
                        REQUIRE(ringBuffer.size() == expectedDataInBuffer.size());
                        REQUIRE(ringBuffer.capacity() == capacity);
                        REQUIRE((ringBuffer.availableFreeSize() + ringBuffer.size()) > RingBuffer::defaultCapacity());
                        REQUIRE(expectedDataInBuffer == ringBuffer.peekAll());
                    }
                }
            }
        }

        WHEN("data larger than buffer's free space is written")
        {
            auto dataGenerator = [](size_t size) -> std::string
            {
                std::string tmp;
                if (size)
                {
                    tmp.resize(size);
                    for (auto &ch : tmp)
                        ch = QRandomGenerator::global()->bounded(0, 256);
                }
                return tmp;
            };
            const auto extraDataSizeBeyondFreeSpace = GENERATE(AS(size_t), 1, 5, 12);
            const auto extraData = dataGenerator(ringBuffer.availableFreeSize() + extraDataSizeBeyondFreeSpace);
            const bool succeeded = extraData.empty() || (extraData.size() == ringBuffer.write(extraData.data(), extraData.size()));

            THEN("write succeeds and buffer enlarges its capacity")
            {
                REQUIRE(succeeded);
                const auto expectedDataInBuffer = (data + extraData);
                REQUIRE(!ringBuffer.isEmpty());
                REQUIRE(ringBuffer.size() == expectedDataInBuffer.size());
                REQUIRE(ringBuffer.capacity() == capacity);
                REQUIRE((ringBuffer.availableFreeSize() + ringBuffer.size()) > RingBuffer::defaultCapacity());
                REQUIRE(expectedDataInBuffer == ringBuffer.peekAll());
            }
        }

        WHEN("data up to buffer's free space is written after buffer's max capacity is set to its current capacity")
        {
            REQUIRE((ringBuffer.availableFreeSize() + ringBuffer.size()) == RingBuffer::defaultCapacity());
            REQUIRE(ringBuffer.setCapacity(RingBuffer::defaultCapacity()));
            auto dataGenerator = [](size_t size) -> std::string
            {
                std::string tmp;
                if (size)
                {
                    tmp.resize(size);
                    for (auto &ch : tmp)
                        ch = QRandomGenerator::global()->bounded(0, 256);
                }
                return tmp;
            };
            const auto extraData = dataGenerator(ringBuffer.availableFreeSize());
            const bool succeeded = extraData.empty() || (extraData.size() == ringBuffer.write(extraData.data(), extraData.size()));

            THEN("write succeeds and buffers does not enlarge it's capacity")
            {
                REQUIRE(succeeded);
                REQUIRE(!ringBuffer.isEmpty());
                REQUIRE(ringBuffer.size() == (data.size() + extraData.size()));
                REQUIRE(ringBuffer.capacity() == RingBuffer::defaultCapacity());
                REQUIRE(ringBuffer.availableFreeSize() == 0);
                REQUIRE((data + extraData) == ringBuffer.peekAll());

                AND_WHEN("any more data is written to buffer")
                {
                    const auto moreDataSize = GENERATE(AS(size_t), 1, 5, 12);
                    const auto moreData = dataGenerator(moreDataSize);
                    const bool succeededToWriteMoreData = (ringBuffer.write(moreData.data(), moreData.size()) > 0);

                    THEN("write fails and buffer does not enlarge its capacity")
                    {
                        REQUIRE(!succeededToWriteMoreData);
                        const auto expectedDataInBuffer = (data + extraData);
                        REQUIRE(!ringBuffer.isEmpty());
                        REQUIRE(ringBuffer.size() == expectedDataInBuffer.size());
                        REQUIRE(ringBuffer.capacity() == RingBuffer::defaultCapacity());
                        REQUIRE(ringBuffer.availableFreeSize() == 0);
                        REQUIRE(expectedDataInBuffer == ringBuffer.peekAll());
                    }
                }
            }
        }
    }
}


class RingBufferDataSourceTest : public DataSource
{
public:
    RingBufferDataSourceTest(size_t count) :
        m_fetchedSize(0)
    {
        m_data.resize(count);
        for (auto i = 0; i < count; ++i)
            m_data[i] = QRandomGenerator::global()->bounded(0, 256);
    }
    ~RingBufferDataSourceTest() override = default;
    size_t dataAvailable() const override {return m_data.size() - m_fetchedSize;}

    size_t read(char *pBuffer, size_t count) override
    {
        const auto acceptableSize = std::min<size_t>(m_data.size() - m_fetchedSize, count);
        std::memcpy(pBuffer, m_data.data() + m_fetchedSize, acceptableSize);
        m_fetchedSize += acceptableSize;
        return acceptableSize;
    }

    std::string_view data() const {return m_data;}
    size_t fetchedSize() const {return m_fetchedSize;}

private:
    size_t m_fetchedSize;
    std::string m_data;
};

class RingBufferDataSinkTest : public DataSink
{
public:
    RingBufferDataSinkTest(size_t capacity) : m_capacity(capacity) {m_data.reserve(m_capacity);}
    ~RingBufferDataSinkTest() override = default;
    size_t write(const char *pData, size_t count) override
    {
        const auto acceptableSize = std::min<size_t>(m_capacity, count);
        m_capacity -= acceptableSize;
        m_data.append(pData, acceptableSize);
        return acceptableSize;
    }

    std::string_view data() const {return m_data;}
    size_t capacity() const {return m_capacity;}

private:
    size_t m_capacity;
    std::string m_data;
};


SCENARIO("RingBuffer supports data IO through data source/sink")
{
    GIVEN("a ring buffer")
    {
        const auto capacity = GENERATE(AS(size_t), RingBuffer::defaultCapacity(),
                                       2 * RingBuffer::defaultCapacity(),
                                       0,
                                       300,
                                       64);
        RingBuffer ringBuffer(capacity);
        REQUIRE(ringBuffer.isEmpty());
        REQUIRE(ringBuffer.size() == 0);
        REQUIRE(ringBuffer.capacity() == capacity);
        REQUIRE(ringBuffer.availableFreeSize() == ((capacity == 0) ? RingBuffer::defaultCapacity() : std::min(capacity, RingBuffer::defaultCapacity())));
        const auto initialDataSize = GENERATE(AS(size_t), 16, 17, 23, 32);
        const auto dataSizeToChangeAfterEachIteration = GENERATE(AS(size_t), 16, 1, 5);
        const auto enlargeAvailableFreeSize = GENERATE(AS(bool), true, false);
        if (enlargeAvailableFreeSize && ((capacity == 0) || (capacity >= (RingBuffer::defaultCapacity() + 16))))
        {
            std::string tmpData(RingBuffer::defaultCapacity() + 16, ' ');
            for (auto &ch : tmpData)
                ch = QRandomGenerator64::global()->bounded(0, 256);
            REQUIRE(tmpData.size() == ringBuffer.write(tmpData.data(), tmpData.size()));
            REQUIRE(!ringBuffer.isEmpty());
            REQUIRE(ringBuffer.size() == tmpData.size());
            REQUIRE(ringBuffer.capacity() == capacity);
            REQUIRE(tmpData == ringBuffer.peekAll());
            REQUIRE(tmpData.size() == ringBuffer.popFront(tmpData.size()));
            REQUIRE(ringBuffer.isEmpty());
            REQUIRE(ringBuffer.size() == 0);
            REQUIRE(ringBuffer.capacity() == capacity);
            REQUIRE(ringBuffer.availableFreeSize() > RingBuffer::defaultCapacity());
        }

        WHEN("data is continuously added and removed from buffer through data source/sink")
        {
            THEN("buffer handles data IO correctly")
            {
                RingBufferDataSourceTest initialData(initialDataSize);
                REQUIRE(initialData.dataAvailable() == initialDataSize);
                REQUIRE(initialData.fetchedSize() == 0);
                std::string expectedDataInBuffer;
                expectedDataInBuffer.reserve(10*capacity);
                expectedDataInBuffer.append(initialData.data());
                const auto previousAvailableFreeSize = ringBuffer.availableFreeSize();
                REQUIRE(initialData.dataAvailable() == ringBuffer.write(initialData));
                REQUIRE(initialData.dataAvailable() == 0);
                REQUIRE(initialData.fetchedSize() == initialDataSize);
                REQUIRE(!ringBuffer.isEmpty());
                REQUIRE(ringBuffer.size() == expectedDataInBuffer.size());
                REQUIRE(ringBuffer.capacity() == capacity);
                REQUIRE(ringBuffer.availableFreeSize() == (previousAvailableFreeSize - expectedDataInBuffer.size()));
                REQUIRE(expectedDataInBuffer == ringBuffer.peekAll());
                size_t addedData = expectedDataInBuffer.size();
                const auto sizeToAdd = 10 * ((capacity > 0) ? capacity : std::min<size_t>(RingBuffer::defaultCapacity(), capacity));
                do
                {
                    const auto previousAvailableFreeSize = ringBuffer.availableFreeSize();
                    RingBufferDataSinkTest dataSink(dataSizeToChangeAfterEachIteration);
                    REQUIRE(dataSink.capacity() == dataSizeToChangeAfterEachIteration);
                    REQUIRE(dataSizeToChangeAfterEachIteration == ringBuffer.read(dataSink));
                    REQUIRE(dataSink.capacity() == 0);
                    REQUIRE(!dataSink.data().empty());
                    REQUIRE(expectedDataInBuffer.starts_with(dataSink.data()));
                    REQUIRE(ringBuffer.isEmpty() == (dataSizeToChangeAfterEachIteration == expectedDataInBuffer.size()));
                    REQUIRE(ringBuffer.size() == (expectedDataInBuffer.size() - dataSizeToChangeAfterEachIteration));
                    REQUIRE(ringBuffer.capacity() == capacity);
                    REQUIRE(ringBuffer.availableFreeSize() == (previousAvailableFreeSize + dataSizeToChangeAfterEachIteration));
                    expectedDataInBuffer.erase(0, dataSizeToChangeAfterEachIteration);
                    RingBufferDataSourceTest dataSource(dataSizeToChangeAfterEachIteration);
                    expectedDataInBuffer.append(dataSource.data());
                    REQUIRE(dataSource.dataAvailable() == dataSizeToChangeAfterEachIteration);
                    REQUIRE(dataSource.fetchedSize() == 0);
                    REQUIRE(dataSizeToChangeAfterEachIteration == ringBuffer.write(dataSource));
                    REQUIRE(dataSource.dataAvailable() == 0);
                    REQUIRE(dataSource.fetchedSize() == dataSizeToChangeAfterEachIteration);
                    REQUIRE(!ringBuffer.isEmpty());
                    REQUIRE(ringBuffer.size() == expectedDataInBuffer.size());
                    REQUIRE(ringBuffer.capacity() == capacity);
                    REQUIRE(ringBuffer.availableFreeSize() == previousAvailableFreeSize);
                    REQUIRE(expectedDataInBuffer == ringBuffer.peekAll());
                    for (auto pos = 0; pos < expectedDataInBuffer.size(); ++pos)
                    {
                        for (auto count = (expectedDataInBuffer.size() - pos); count > 0; --count)
                        {
                            REQUIRE(std::string_view(expectedDataInBuffer.data() + pos, count) == ringBuffer.slice(pos, count));
                        }
                    }
                    for (auto i = 0; i < expectedDataInBuffer.size(); ++i)
                    {
                        REQUIRE(expectedDataInBuffer[i] == ringBuffer.peekChar(i));
                    }
                    addedData += dataSizeToChangeAfterEachIteration;
                }
                while (addedData < sizeToAdd);
            }
        }
    }

    GIVEN("a ring buffer with data at the beginning")
    {
        const auto dataSize = GENERATE_RANGE(AS(size_t), 1, RingBuffer::defaultCapacity());

        WHEN("data is read from buffer to data sink")
        {
            THEN("read operation fetches data correctly")
            {
                static const std::string dataToWrite = []() -> std::string
                {
                    std::string tmp(RingBuffer::defaultCapacity(), ' ');
                    for (auto &ch : tmp)
                        ch = QRandomGenerator::global()->bounded(0, 256);
                    return tmp;
                }();

                for (auto i = 1; i <= dataSize; ++i)
                {
                    RingBuffer ringBuffer(RingBuffer::defaultCapacity());
                    REQUIRE(ringBuffer.isEmpty());
                    REQUIRE(ringBuffer.size() == 0);
                    REQUIRE(ringBuffer.capacity() == RingBuffer::defaultCapacity());
                    REQUIRE(ringBuffer.availableFreeSize() == RingBuffer::defaultCapacity());
                    REQUIRE(dataSize == ringBuffer.write(dataToWrite.data(), dataSize));
                    REQUIRE(!ringBuffer.isEmpty());
                    REQUIRE(ringBuffer.size() == dataSize);
                    REQUIRE(ringBuffer.capacity() == RingBuffer::defaultCapacity());
                    REQUIRE(ringBuffer.availableFreeSize() == (RingBuffer::defaultCapacity() - dataSize));
                    const auto expectedData = std::string_view(dataToWrite.data(), i);
                    for (auto pos = 0; pos < expectedData.size(); ++pos)
                    {
                        for (auto count = (expectedData.size() - pos); count > 0; --count)
                        {
                            REQUIRE(std::string_view(expectedData.data() + pos, count) == ringBuffer.slice(pos, count));
                        }
                    }
                    for (auto i = 0; i < expectedData.size(); ++i)
                    {
                        REQUIRE(expectedData[i] == ringBuffer.peekChar(i));
                    }
                    RingBufferDataSinkTest readData(i);
                    REQUIRE(readData.capacity() == i);
                    REQUIRE(readData.capacity() == ringBuffer.read(readData));
                    REQUIRE(readData.capacity() == 0);
                    REQUIRE(readData.data() == expectedData);
                    REQUIRE(ringBuffer.isEmpty() == (i == dataSize));
                    REQUIRE(ringBuffer.size() == (dataSize - i));
                    REQUIRE(ringBuffer.capacity() == RingBuffer::defaultCapacity());
                    REQUIRE(ringBuffer.availableFreeSize() == (RingBuffer::defaultCapacity() - dataSize + i));
                }
            }
        }
    }

    GIVEN("a ring buffer with data at the end")
    {
        const auto dataGap = GENERATE_RANGE(AS(size_t), 1, RingBuffer::defaultCapacity() - 1);
        const auto dataSize = RingBuffer::defaultCapacity() - dataGap;

        WHEN("data is read from buffer to data sink")
        {
            THEN("read operation fetches data correctly")
            {
                static const std::string dataToWrite = []() -> std::string
                {
                    std::string tmp(RingBuffer::defaultCapacity(), ' ');
                    for (auto &ch : tmp)
                        ch = QRandomGenerator::global()->bounded(0, 256);
                    return tmp;
                }();

                for (auto i = 1; i <= dataSize; ++i)
                {
                    RingBuffer ringBuffer(RingBuffer::defaultCapacity());
                    REQUIRE(ringBuffer.isEmpty());
                    REQUIRE(ringBuffer.size() == 0);
                    REQUIRE(ringBuffer.capacity() == RingBuffer::defaultCapacity());
                    REQUIRE(ringBuffer.availableFreeSize() == RingBuffer::defaultCapacity());
                    REQUIRE(dataToWrite.size() == ringBuffer.write(dataToWrite.data(), dataToWrite.size()));
                    REQUIRE(!ringBuffer.isEmpty());
                    REQUIRE(ringBuffer.size() == dataToWrite.size());
                    REQUIRE(ringBuffer.capacity() == RingBuffer::defaultCapacity());
                    REQUIRE(ringBuffer.availableFreeSize() == 0);
                    REQUIRE(dataToWrite == ringBuffer.peekAll());
                    REQUIRE(dataGap == ringBuffer.popFront(dataGap));
                    REQUIRE(!ringBuffer.isEmpty());
                    REQUIRE(ringBuffer.size() == dataSize);
                    REQUIRE(ringBuffer.capacity() == RingBuffer::defaultCapacity());
                    REQUIRE(ringBuffer.availableFreeSize() == dataGap);
                    REQUIRE(std::string_view(dataToWrite.data() + dataGap, dataSize) == ringBuffer.peekAll());
                    const auto expectedData = std::string_view(dataToWrite.data() + dataGap, i);
                    for (auto pos = 0; pos < expectedData.size(); ++pos)
                    {
                        for (auto count = (expectedData.size() - pos); count > 0; --count)
                        {
                            REQUIRE(std::string_view(expectedData.data() + pos, count) == ringBuffer.slice(pos, count));
                        }
                    }
                    for (auto i = 0; i < expectedData.size(); ++i)
                    {
                        REQUIRE(expectedData[i] == ringBuffer.peekChar(i));
                    }
                    RingBufferDataSinkTest readData(i);
                    REQUIRE(readData.capacity() == i);
                    REQUIRE(readData.capacity() == ringBuffer.read(readData));
                    REQUIRE(readData.capacity() == 0);
                    REQUIRE(readData.data() == expectedData);
                    REQUIRE(ringBuffer.isEmpty() == (i == dataSize));
                    REQUIRE(ringBuffer.size() == (dataSize - i));
                    REQUIRE(ringBuffer.capacity() == RingBuffer::defaultCapacity());
                    REQUIRE(ringBuffer.availableFreeSize() == (RingBuffer::defaultCapacity() - dataSize + i));
                }
            }
        }
    }

    GIVEN("a ring buffer with data at the middle")
    {
        const auto dataGap = GENERATE_RANGE(AS(size_t), 1, 2*RingBuffer::defaultCapacity()/3 - 1);
        const auto dataSize = RingBuffer::defaultCapacity()/3;

        WHEN("data is read from buffer to data sink")
        {
            THEN("read operation fetches data correctly")
            {
                static const std::string dataToWrite = []() -> std::string
                {
                    std::string tmp(RingBuffer::defaultCapacity(), ' ');
                    for (auto &ch : tmp)
                        ch = QRandomGenerator::global()->bounded(0, 256);
                    return tmp;
                }();

                for (auto i = 1; i <= dataSize; ++i)
                {
                    RingBuffer ringBuffer(RingBuffer::defaultCapacity());
                    REQUIRE(ringBuffer.isEmpty());
                    REQUIRE(ringBuffer.size() == 0);
                    REQUIRE(ringBuffer.capacity() == RingBuffer::defaultCapacity());
                    REQUIRE(ringBuffer.availableFreeSize() == RingBuffer::defaultCapacity());
                    REQUIRE((dataGap + dataSize) == ringBuffer.write(dataToWrite.data(), dataGap + dataSize));
                    REQUIRE(!ringBuffer.isEmpty());
                    REQUIRE(ringBuffer.size() == (dataGap + dataSize));
                    REQUIRE(ringBuffer.capacity() == RingBuffer::defaultCapacity());
                    REQUIRE(ringBuffer.availableFreeSize() == (RingBuffer::defaultCapacity() - dataGap - dataSize));
                    REQUIRE(dataGap == ringBuffer.popFront(dataGap));
                    REQUIRE(!ringBuffer.isEmpty());
                    REQUIRE(ringBuffer.size() == dataSize);
                    REQUIRE(ringBuffer.capacity() == RingBuffer::defaultCapacity());
                    REQUIRE(ringBuffer.availableFreeSize() == (RingBuffer::defaultCapacity() - dataSize));
                    REQUIRE(std::string_view(dataToWrite.data() + dataGap, dataSize) == ringBuffer.peekAll());
                    const auto expectedData = std::string_view(dataToWrite.data() + dataGap, i);
                    for (auto pos = 0; pos < expectedData.size(); ++pos)
                    {
                        for (auto count = (expectedData.size() - pos); count > 0; --count)
                        {
                            REQUIRE(std::string_view(expectedData.data() + pos, count) == ringBuffer.slice(pos, count));
                        }
                    }
                    for (auto i = 0; i < expectedData.size(); ++i)
                    {
                        REQUIRE(expectedData[i] == ringBuffer.peekChar(i));
                    }
                    RingBufferDataSinkTest readData(i);
                    REQUIRE(readData.capacity() == i);
                    REQUIRE(readData.capacity() == ringBuffer.read(readData));
                    REQUIRE(readData.capacity() == 0);
                    REQUIRE(readData.data() == expectedData);
                    REQUIRE(ringBuffer.isEmpty() == (i == dataSize));
                    REQUIRE(ringBuffer.size() == (dataSize - i));
                    REQUIRE(ringBuffer.capacity() == RingBuffer::defaultCapacity());
                    REQUIRE(ringBuffer.availableFreeSize() == (RingBuffer::defaultCapacity() - dataSize + i));
                }
            }
        }
    }

    GIVEN("a ring buffer with splitted data")
    {
        WHEN("data is read from buffer to data sink")
        {
            THEN("read operation fetches data correctly")
            {
                static const std::string dataToWrite = []() -> std::string
                {
                    std::string tmp(RingBuffer::defaultCapacity(), ' ');
                    for (auto &ch : tmp)
                        ch = QRandomGenerator::global()->bounded(0, 256);
                    return tmp;
                }();

                for (auto pos = 1; pos < (RingBuffer::defaultCapacity() - 1); ++pos)
                {
                    for (auto size = 1; size < (RingBuffer::defaultCapacity() - pos); ++size)
                    {
                        for (auto dataToRead = 1; dataToRead <= (RingBuffer::defaultCapacity() - size); ++dataToRead)
                        {
                            RingBuffer ringBuffer(RingBuffer::defaultCapacity());
                            REQUIRE(ringBuffer.isEmpty());
                            REQUIRE(ringBuffer.size() == 0);
                            REQUIRE(ringBuffer.capacity() == RingBuffer::defaultCapacity());
                            REQUIRE(ringBuffer.availableFreeSize() == RingBuffer::defaultCapacity());
                            REQUIRE((pos + size) == ringBuffer.write(dataToWrite.data(), pos + size));
                            REQUIRE(!ringBuffer.isEmpty());
                            REQUIRE(ringBuffer.size() == (pos + size));
                            REQUIRE(ringBuffer.capacity() == RingBuffer::defaultCapacity());
                            REQUIRE(ringBuffer.availableFreeSize() == (RingBuffer::defaultCapacity() - pos - size));
                            REQUIRE((dataToWrite.size() - (pos + size)) == ringBuffer.write(dataToWrite.data(), dataToWrite.size() - (pos + size)));
                            REQUIRE(!ringBuffer.isEmpty());
                            REQUIRE(ringBuffer.size() == RingBuffer::defaultCapacity());
                            REQUIRE(ringBuffer.capacity() == RingBuffer::defaultCapacity());
                            REQUIRE(ringBuffer.availableFreeSize() == 0);
                            REQUIRE((pos + size) == ringBuffer.popFront(pos + size));
                            REQUIRE(!ringBuffer.isEmpty());
                            REQUIRE(ringBuffer.size() == (RingBuffer::defaultCapacity() - (pos + size)));
                            REQUIRE(ringBuffer.capacity() == RingBuffer::defaultCapacity());
                            REQUIRE(ringBuffer.availableFreeSize() == (pos + size));
                            REQUIRE(pos == ringBuffer.write(dataToWrite.data() + dataToWrite.size() - (pos + size), pos));
                            REQUIRE(!ringBuffer.isEmpty());
                            REQUIRE(ringBuffer.size() == (RingBuffer::defaultCapacity() - size));
                            REQUIRE(ringBuffer.capacity() == RingBuffer::defaultCapacity());
                            REQUIRE(ringBuffer.availableFreeSize() == size);
                            REQUIRE(std::string_view(dataToWrite.data(), RingBuffer::defaultCapacity() - size) == ringBuffer.peekAll());
                            const auto expectedData = std::string_view(dataToWrite.data(), dataToRead);
                            for (auto pos = 0; pos < expectedData.size(); ++pos)
                            {
                                for (auto count = (expectedData.size() - pos); count > 0; --count)
                                {
                                    REQUIRE(std::string_view(expectedData.data() + pos, count) == ringBuffer.slice(pos, count));
                                }
                            }
                            for (auto i = 0; i < expectedData.size(); ++i)
                            {
                                REQUIRE(expectedData[i] == ringBuffer.peekChar(i));
                            }
                            RingBufferDataSinkTest readData(dataToRead);
                            REQUIRE(readData.capacity() == dataToRead);
                            REQUIRE(readData.capacity() == ringBuffer.read(readData));
                            REQUIRE(readData.capacity() == 0);
                            REQUIRE(readData.data() == expectedData);
                            REQUIRE(ringBuffer.isEmpty() == (dataToRead == (RingBuffer::defaultCapacity() - size)));
                            REQUIRE(ringBuffer.size() == (RingBuffer::defaultCapacity() - size - dataToRead));
                            REQUIRE(ringBuffer.capacity() == RingBuffer::defaultCapacity());
                            REQUIRE(ringBuffer.availableFreeSize() == (size + dataToRead));
                        }
                    }
                }
            }
        }
    }
}


SCENARIO("RingBuffer enlarges buffer when writing data from data source")
{
    GIVEN("a buffer constructed according to an initial data policy")
    {
        enum class InitialDataPolicy {Empty, Full, Start, End, Middle, Splitted};
        auto bufferInitializer = [](InitialDataPolicy dataPolicy, RingBuffer &ringBuffer, std::string &data)
        {
            REQUIRE(ringBuffer.isEmpty());
            const auto currentCapacity = ringBuffer.availableFreeSize();
            auto dataGenerator = [](size_t size) -> std::string
            {
                REQUIRE(size > 0);
                std::string tmp(size, ' ');
                for (auto &ch : tmp)
                    ch = QRandomGenerator::global()->bounded(0, 256);
                return tmp;
            };
            switch (dataPolicy)
            {
                case InitialDataPolicy::Empty:
                    data.clear();
                    break;
                case InitialDataPolicy::Full:
                    data = dataGenerator(currentCapacity);
                    REQUIRE(ringBuffer.write(data.data(), data.size()));
                    break;
                case InitialDataPolicy::Start:
                    data = dataGenerator(currentCapacity/3);
                    REQUIRE(ringBuffer.write(data.data(), data.size()));
                    break;
                case InitialDataPolicy::End:
                    data = dataGenerator((2*currentCapacity)/3);
                    REQUIRE(ringBuffer.write(data.data(), data.size()));
                    data = dataGenerator(currentCapacity - ringBuffer.size());
                    REQUIRE(ringBuffer.write(data.data(), data.size()));
                    ringBuffer.popFront((2*currentCapacity)/3);
                    break;
                case InitialDataPolicy::Middle:
                    data = dataGenerator(currentCapacity/3);
                    REQUIRE(ringBuffer.write(data.data(), data.size()));
                    data = dataGenerator(currentCapacity/3);
                    REQUIRE(ringBuffer.write(data.data(), data.size()));
                    ringBuffer.popFront(currentCapacity/3);
                    break;
                case InitialDataPolicy::Splitted:
                    data = dataGenerator((2*currentCapacity)/3);
                    REQUIRE(ringBuffer.write(data.data(), data.size()));
                    data = dataGenerator(currentCapacity - ringBuffer.size());
                    REQUIRE(ringBuffer.write(data.data(), data.size()));
                    ringBuffer.popFront((2*currentCapacity)/3);
                    {
                        auto tailData = dataGenerator(currentCapacity/3);
                        ringBuffer.write(tailData.data(), tailData.size());
                        data.append(tailData);
                    }
                    break;
            }
        };
        const auto policy = GENERATE(AS(InitialDataPolicy),
                                     InitialDataPolicy::Empty,
                                     InitialDataPolicy::Full,
                                     InitialDataPolicy::Start,
                                     InitialDataPolicy::End,
                                     InitialDataPolicy::Middle,
                                     InitialDataPolicy::Splitted);
        const auto capacity = GENERATE(AS(size_t), 2 * RingBuffer::defaultCapacity());
        RingBuffer ringBuffer(capacity);
        REQUIRE(ringBuffer.isEmpty());
        REQUIRE(ringBuffer.size() == 0);
        REQUIRE(ringBuffer.capacity() == capacity);
        REQUIRE(ringBuffer.availableFreeSize() == RingBuffer::defaultCapacity());
        std::string data;
        bufferInitializer(policy, ringBuffer, data);
        REQUIRE(ringBuffer.isEmpty() == (policy == InitialDataPolicy::Empty));
        REQUIRE(ringBuffer.size() == data.size());
        REQUIRE(ringBuffer.capacity() == capacity);
        REQUIRE(ringBuffer.availableFreeSize() == (RingBuffer::defaultCapacity() - data.size()));

        WHEN("data up to buffer's free space is written")
        {
            RingBufferDataSourceTest extraData(ringBuffer.availableFreeSize());
            const auto expectedFetchedData = ringBuffer.availableFreeSize();
            const bool succeeded = (extraData.dataAvailable() == ringBuffer.write(extraData));

            THEN("write succeeds and buffer does not enlarge it's capacity")
            {
                REQUIRE(succeeded);
                REQUIRE(extraData.dataAvailable() == 0);
                REQUIRE(extraData.fetchedSize() == expectedFetchedData);
                REQUIRE(!ringBuffer.isEmpty());
                REQUIRE(ringBuffer.size() == (data.size() + expectedFetchedData));
                REQUIRE(ringBuffer.capacity() == capacity);
                REQUIRE(ringBuffer.availableFreeSize() == 0);
                REQUIRE((data + std::string(extraData.data())) == ringBuffer.peekAll());

                AND_WHEN("any more data is written to buffer")
                {
                    const auto moreDataSize = GENERATE(AS(size_t), 1, 5, 12);
                    RingBufferDataSourceTest moreData(moreDataSize);
                    const bool succeededToWriteMoreData = (moreData.dataAvailable() == ringBuffer.write(moreData));

                    THEN("write succeeds and buffer enlarges its capacity")
                    {
                        REQUIRE(succeededToWriteMoreData);
                        const auto expectedDataInBuffer = (data + std::string(extraData.data()) + std::string(moreData.data()));
                        REQUIRE(!ringBuffer.isEmpty());
                        REQUIRE(ringBuffer.size() == expectedDataInBuffer.size());
                        REQUIRE(ringBuffer.capacity() == capacity);
                        REQUIRE((ringBuffer.availableFreeSize() + ringBuffer.size()) > RingBuffer::defaultCapacity());
                        REQUIRE(expectedDataInBuffer == ringBuffer.peekAll());
                    }
                }
            }
        }

        WHEN("data larger than buffer's free space is written")
        {
            const auto extraDataSizeBeyondFreeSpace = GENERATE(AS(size_t), 1, 5, 12);
            RingBufferDataSourceTest extraData(ringBuffer.availableFreeSize() + extraDataSizeBeyondFreeSpace);
            const bool succeeded = (extraData.dataAvailable() == ringBuffer.write(extraData));

            THEN("write succeeds and buffer enlarges its capacity")
            {
                REQUIRE(succeeded);
                const auto expectedDataInBuffer = (data + std::string(extraData.data()));
                REQUIRE(!ringBuffer.isEmpty());
                REQUIRE(ringBuffer.size() == expectedDataInBuffer.size());
                REQUIRE(ringBuffer.capacity() == capacity);
                REQUIRE((ringBuffer.availableFreeSize() + ringBuffer.size()) > RingBuffer::defaultCapacity());
                REQUIRE(expectedDataInBuffer == ringBuffer.peekAll());
            }
        }

        WHEN("data up to buffer's free space is written after buffer's max capacity is set to its current capacity")
        {
            REQUIRE((ringBuffer.availableFreeSize() + ringBuffer.size()) == RingBuffer::defaultCapacity());
            REQUIRE(ringBuffer.setCapacity(RingBuffer::defaultCapacity()));
            RingBufferDataSourceTest extraData(ringBuffer.availableFreeSize());
            const bool succeeded = (extraData.dataAvailable() == ringBuffer.write(extraData));

            THEN("write succeeds and buffers does not enlarge it's capacity")
            {
                REQUIRE(succeeded);
                REQUIRE(!ringBuffer.isEmpty());
                REQUIRE(ringBuffer.size() == RingBuffer::defaultCapacity());
                REQUIRE(ringBuffer.capacity() == RingBuffer::defaultCapacity());
                REQUIRE(ringBuffer.availableFreeSize() == 0);
                REQUIRE((data + std::string(extraData.data())) == ringBuffer.peekAll());

                AND_WHEN("any more data is written to buffer")
                {
                    const auto moreDataSize = GENERATE(AS(size_t), 1, 5, 12);
                    RingBufferDataSourceTest moreData(moreDataSize);
                    const bool succeededToWriteMoreData = (ringBuffer.write(moreData) > 0);

                    THEN("write fails and buffer does not enlarge its capacity")
                    {
                        REQUIRE(!succeededToWriteMoreData);
                        const auto expectedDataInBuffer = (data + std::string(extraData.data()));
                        REQUIRE(!ringBuffer.isEmpty());
                        REQUIRE(ringBuffer.size() == expectedDataInBuffer.size());
                        REQUIRE(ringBuffer.capacity() == RingBuffer::defaultCapacity());
                        REQUIRE(ringBuffer.availableFreeSize() == 0);
                        REQUIRE(expectedDataInBuffer == ringBuffer.peekAll());
                    }
                }
            }
        }
    }
}


SCENARIO("RingBuffer uses all available size/capacity of data source/sink")
{
    GIVEN("a buffer constructed according to an initial data policy")
    {
        enum class InitialDataPolicy {Empty, Full, Start, End, Middle, Splitted};
        auto bufferInitializer = [](InitialDataPolicy dataPolicy, RingBuffer &ringBuffer, std::string &data)
        {
            REQUIRE(ringBuffer.isEmpty());
            const auto currentCapacity = ringBuffer.availableFreeSize();
            auto dataGenerator = [](size_t size) -> std::string
            {
                REQUIRE(size > 0);
                std::string tmp(size, ' ');
                for (auto &ch : tmp)
                    ch = QRandomGenerator::global()->bounded(0, 256);
                return tmp;
            };
            switch (dataPolicy)
            {
                case InitialDataPolicy::Empty:
                    data.clear();
                    break;
                case InitialDataPolicy::Full:
                    data = dataGenerator(currentCapacity);
                    REQUIRE(ringBuffer.write(data.data(), data.size()));
                    break;
                case InitialDataPolicy::Start:
                    data = dataGenerator(currentCapacity/3);
                    REQUIRE(ringBuffer.write(data.data(), data.size()));
                    break;
                case InitialDataPolicy::End:
                    data = dataGenerator((2*currentCapacity)/3);
                    REQUIRE(ringBuffer.write(data.data(), data.size()));
                    data = dataGenerator(currentCapacity - ringBuffer.size());
                    REQUIRE(ringBuffer.write(data.data(), data.size()));
                    ringBuffer.popFront((2*currentCapacity)/3);
                    break;
                case InitialDataPolicy::Middle:
                    data = dataGenerator(currentCapacity/3);
                    REQUIRE(ringBuffer.write(data.data(), data.size()));
                    data = dataGenerator(currentCapacity/3);
                    REQUIRE(ringBuffer.write(data.data(), data.size()));
                    ringBuffer.popFront(currentCapacity/3);
                    break;
                case InitialDataPolicy::Splitted:
                    data = dataGenerator((2*currentCapacity)/3);
                    REQUIRE(ringBuffer.write(data.data(), data.size()));
                    data = dataGenerator(currentCapacity - ringBuffer.size());
                    REQUIRE(ringBuffer.write(data.data(), data.size()));
                    ringBuffer.popFront((2*currentCapacity)/3);
                    {
                        auto tailData = dataGenerator(currentCapacity/3);
                        ringBuffer.write(tailData.data(), tailData.size());
                        data.append(tailData);
                    }
                    break;
            }
        };
        const auto policy = GENERATE(AS(InitialDataPolicy),
                                     InitialDataPolicy::Empty,
                                     InitialDataPolicy::Full,
                                     InitialDataPolicy::Start,
                                     InitialDataPolicy::End,
                                     InitialDataPolicy::Middle,
                                     InitialDataPolicy::Splitted);
        const auto capacity = GENERATE(AS(size_t), 2 * RingBuffer::defaultCapacity());
        RingBuffer ringBuffer(capacity);
        REQUIRE(ringBuffer.isEmpty());
        REQUIRE(ringBuffer.size() == 0);
        REQUIRE(ringBuffer.capacity() == capacity);
        REQUIRE(ringBuffer.availableFreeSize() == RingBuffer::defaultCapacity());
        std::string data;
        bufferInitializer(policy, ringBuffer, data);
        REQUIRE(ringBuffer.isEmpty() == (policy == InitialDataPolicy::Empty));
        REQUIRE(ringBuffer.size() == data.size());
        REQUIRE(ringBuffer.capacity() == capacity);
        REQUIRE(ringBuffer.availableFreeSize() == (RingBuffer::defaultCapacity() - data.size()));

        WHEN("data is read to sink that can handle all available data in buffer")
        {
            const auto extraCapacityOnSink = GENERATE(AS(size_t), 0, 1, 15);
            const auto initialDataSinkCapacity = ringBuffer.capacity() - ringBuffer.size() + extraCapacityOnSink;
            RingBufferDataSinkTest dataSink(initialDataSinkCapacity);
            const auto readDataSize = ringBuffer.read(dataSink);

            THEN("buffer becomes empty")
            {
                REQUIRE(readDataSize == data.size());
                REQUIRE(ringBuffer.isEmpty());
                REQUIRE(ringBuffer.size() == 0);
                REQUIRE(ringBuffer.capacity() == (2*RingBuffer::defaultCapacity()));
                REQUIRE(ringBuffer.availableFreeSize() == RingBuffer::defaultCapacity());
                REQUIRE(dataSink.capacity() == (initialDataSinkCapacity - data.size()));
            }
        }

        WHEN("data is written from source that has more data that buffer can handle")
        {
            const auto extraDataOnSource = GENERATE(AS(size_t), 0, 1, 15);
            const auto dataSourceSize = ringBuffer.capacity() - ringBuffer.size() + extraDataOnSource;
            RingBufferDataSourceTest dataSource(dataSourceSize);
            const auto spaceAvailableInBuffer = ringBuffer.capacity() - ringBuffer.size();
            const auto writtenDataSize = ringBuffer.write(dataSource);

            THEN("buffer becomes full")
            {
                REQUIRE(writtenDataSize == spaceAvailableInBuffer);
                REQUIRE(dataSource.fetchedSize() == spaceAvailableInBuffer);
                REQUIRE(dataSource.dataAvailable() == (dataSourceSize - spaceAvailableInBuffer));
                REQUIRE(!ringBuffer.isEmpty());
                REQUIRE(ringBuffer.size() == ringBuffer.capacity());
                REQUIRE(ringBuffer.capacity() == (2*RingBuffer::defaultCapacity()));
                REQUIRE(ringBuffer.availableFreeSize() == 0);
                REQUIRE(ringBuffer.peekAll() == (data + std::string(dataSource.data().data(), spaceAvailableInBuffer)));
            }
        }
    }
}
