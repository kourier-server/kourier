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

#include "HostAddressFetcher.h"
#include "Object.h"
#include <Spectator.h>
#include <QThread>
#include <QCoreApplication>

using Kourier::HostAddressFetcher;
using Spectator::SemaphoreAwaiter;
using Kourier::Object;


static std::vector<std::string> fetchedAddresses;
static std::list<void*> givenCallbackData;
static Qt::HANDLE threadId = nullptr;
static QSemaphore fetchedAddressesSemaphore;

static void onHostFound(const std::vector<std::string> &addresses, void *pData)
{
    givenCallbackData.push_back(pData);
    threadId = QThread::currentThreadId();
    fetchedAddresses = addresses;
    fetchedAddressesSemaphore.release();
}


SCENARIO("HostAddressFetcher fetches addresses for domains on hosts file")
{
    GIVEN("a domain located on the hosts file")
    {
        const std::string_view domain("test.onlocalhost.com");

        WHEN("the addresses associated with the domain are fetched")
        {
            static void * const pStaticData = (void*)(0xab);
            HostAddressFetcher::addHostLookup(domain, &onHostFound, pStaticData);

            THEN("HostAddressFetcher informs the correct addresses")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(fetchedAddressesSemaphore, 10));
                REQUIRE(threadId == QThread::currentThreadId());
                REQUIRE(givenCallbackData.size() == 1);
                REQUIRE(*givenCallbackData.begin() == pStaticData);
                givenCallbackData.clear();
                REQUIRE(fetchedAddresses.size() == 6);
                REQUIRE(std::erase(fetchedAddresses, "127.10.20.50") == 1);
                REQUIRE(std::erase(fetchedAddresses, "127.10.20.60") == 1);
                REQUIRE(std::erase(fetchedAddresses, "127.10.20.70") == 1);
                REQUIRE(std::erase(fetchedAddresses, "127.10.20.80") == 1);
                REQUIRE(std::erase(fetchedAddresses, "127.10.20.90") == 1);
                REQUIRE(std::erase(fetchedAddresses, "::1") == 1);
                REQUIRE(fetchedAddresses.empty());
            }
        }
    }
}

static std::vector<void*> addedReceiversData;
static void onHostFoundWithDeletions(const std::vector<std::string> &addresses, void *pData)
{
    givenCallbackData.push_back(pData);
    threadId = QThread::currentThreadId();
    fetchedAddresses = addresses;
    REQUIRE((addedReceiversData.size() - 1) == HostAddressFetcher::receiverCount("test.onlocalhost.com"));
    for (auto pReceiverData : addedReceiversData)
    {
        if (pReceiverData != pData)
            HostAddressFetcher::removeHostLookup("test.onlocalhost.com", &onHostFoundWithDeletions, pReceiverData);
    }
    fetchedAddressesSemaphore.release();
}


SCENARIO("HostAddressFetcher supports deletions while calling callback")
{
    GIVEN("a domain located on the hosts file")
    {
        const std::string_view domain("test.onlocalhost.com");

        WHEN("the first receiver called removes all other receivers")
        {
            addedReceiversData = {(void*)0xa, (void*)0xb, (void*)0xc, (void*)0xd, (void*)0xe};
            for (auto pReceiverData : addedReceiversData)
                HostAddressFetcher::addHostLookup(domain, &onHostFoundWithDeletions, pReceiverData);

            THEN("HostAddressFetcher informs the correct addresses to the first receiver only")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(fetchedAddressesSemaphore, 10));
                REQUIRE(0 == HostAddressFetcher::receiverCount(domain));
                REQUIRE(threadId == QThread::currentThreadId());
                REQUIRE(givenCallbackData.size() == 1);
                REQUIRE(*givenCallbackData.begin() == addedReceiversData[0]);
                givenCallbackData.clear();
                REQUIRE(fetchedAddresses.size() == 6);
                REQUIRE(std::erase(fetchedAddresses, "127.10.20.50") == 1);
                REQUIRE(std::erase(fetchedAddresses, "127.10.20.60") == 1);
                REQUIRE(std::erase(fetchedAddresses, "127.10.20.70") == 1);
                REQUIRE(std::erase(fetchedAddresses, "127.10.20.80") == 1);
                REQUIRE(std::erase(fetchedAddresses, "127.10.20.90") == 1);
                REQUIRE(std::erase(fetchedAddresses, "::1") == 1);
                REQUIRE(fetchedAddresses.empty());
            }
        }
    }
}


SCENARIO("HostAddressFetcher fetches addresses for internet domains")
{
    GIVEN("an internet domain")
    {
        const std::string_view domain("google.com");

        WHEN("the addresses associated with the domain is fetched")
        {
            static void * const pStaticData = (void*)(0xfa);
            HostAddressFetcher::addHostLookup(domain, &onHostFound, pStaticData);

            THEN("HostAddressFetcher informs the correct addresses")
            {
                REQUIRE(SemaphoreAwaiter::signalSlotAwareWait(fetchedAddressesSemaphore, 10));
                REQUIRE(threadId == QThread::currentThreadId());
                REQUIRE(givenCallbackData.size() == 1);
                REQUIRE(*givenCallbackData.begin() == pStaticData);
                givenCallbackData.clear();
                REQUIRE(!fetchedAddresses.empty());
                const auto directlyFetchedAddresses = QHostInfo::fromName(QString::fromStdString(std::string(domain))).addresses();
                bool hasFoundAtLeastOneMatch = false;
                for (auto &it : directlyFetchedAddresses)
                {
                    for (const auto &fetchedAddress : fetchedAddresses)
                    {
                        if (fetchedAddress == it.toString().toStdString())
                        {
                            hasFoundAtLeastOneMatch = true;
                            break;
                        }
                    }
                    if (hasFoundAtLeastOneMatch)
                        break;
                }
                REQUIRE(hasFoundAtLeastOneMatch);
            }
        }
    }
}
