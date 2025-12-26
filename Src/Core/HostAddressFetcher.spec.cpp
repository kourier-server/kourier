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
#include <Tests/Resources/TestHostNamesFetcher.h>
#include <Spectator.h>
#include <QThread>
#include <QCoreApplication>

using Kourier::HostAddressFetcher;
using Spectator::SemaphoreAwaiter;
using Kourier::Object;
using Kourier::TestResources::TestHostNamesFetcher;

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
        const auto [hostName, hostAddresses] = TestHostNamesFetcher::hostNameWithIpv4Ipv6Addresses();
        const std::string_view domain(hostName);

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
                REQUIRE(!hostAddresses.isEmpty() && fetchedAddresses.size() == hostAddresses.size());
                for (const auto &hostAddress : hostAddresses)
                {
                    REQUIRE(std::erase(fetchedAddresses, hostAddress.toString().toStdString()) == 1);
                }
                REQUIRE(fetchedAddresses.empty());
            }
        }
    }
}

static std::vector<void*> addedReceiversData;
static void onHostFoundWithDeletions(const std::vector<std::string> &addresses, void *pData)
{
    const auto hostName = TestHostNamesFetcher::hostNameWithIpv4Ipv6Addresses().first;
    givenCallbackData.push_back(pData);
    threadId = QThread::currentThreadId();
    fetchedAddresses = addresses;
    REQUIRE((addedReceiversData.size() - 1) == HostAddressFetcher::receiverCount(hostName));
    for (auto pReceiverData : addedReceiversData)
    {
        if (pReceiverData != pData)
            HostAddressFetcher::removeHostLookup(hostName, &onHostFoundWithDeletions, pReceiverData);
    }
    fetchedAddressesSemaphore.release();
}


SCENARIO("HostAddressFetcher supports deletions while calling callback")
{
    GIVEN("a domain located on the hosts file")
    {
        const auto [hostName, hostAddresses] = TestHostNamesFetcher::hostNameWithIpv4Ipv6Addresses();
        const std::string_view domain(hostName);

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
                REQUIRE(!hostAddresses.isEmpty() && fetchedAddresses.size() == hostAddresses.size());
                for (const auto &hostAddress : hostAddresses)
                {
                    REQUIRE(std::erase(fetchedAddresses, hostAddress.toString().toStdString()) == 1);
                }
                REQUIRE(fetchedAddresses.empty());
            }
        }
    }
}
