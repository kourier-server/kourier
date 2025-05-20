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
#include "NoDestroy.h"
#include <QHostInfo>


namespace Kourier
{

void HostAddressFetcher::addHostLookup(std::string_view hostName, HostAddressFetcherCallback callback, void *pData)
{
    assert(callback != nullptr);
    auto *pHostAddressFetcher = HostAddressFetcher::current();
    if (pHostAddressFetcher != nullptr)
        pHostAddressFetcher->lookupHost(hostName, callback, pData);
    else
        callback({}, pData);
}

void HostAddressFetcher::removeHostLookup(std::string_view hostName, HostAddressFetcherCallback callback, void *pData)
{
    auto *pHostAddressFetcher = HostAddressFetcher::current();
    if (pHostAddressFetcher != nullptr)
        pHostAddressFetcher->removeLookupHostReceiver(hostName, callback, pData);
}

size_t HostAddressFetcher::receiverCount(std::string_view hostName)
{
    auto *pHostAddressFetcher = HostAddressFetcher::current();
    return (pHostAddressFetcher != nullptr) ? pHostAddressFetcher->lookupReceiverCount(hostName) : 0;
}

HostAddressFetcher *HostAddressFetcher::current()
{
    thread_local NoDestroy<HostAddressFetcher*> pHostAddressFetcher(new HostAddressFetcher);
    thread_local NoDestroyPtrDeleter<HostAddressFetcher*> addressFetcherDeleter(pHostAddressFetcher);
    return pHostAddressFetcher();
}

void HostAddressFetcher::lookupHost(std::string_view hostName, HostAddressFetcherCallback callback, void *pData)
{
    if (hostName.empty())
        return;
    const std::string hostNameStr(hostName);
    auto it = m_addedReceivers.find(hostNameStr);
    if (it != m_addedReceivers.end())
        it->second.insert(std::make_pair(callback, pData));
    else
    {
        m_addedReceivers[hostNameStr].insert(std::make_pair(callback, pData));
        QHostInfo::lookupHost(QString::fromStdString(hostNameStr), this, &HostAddressFetcher::onHostFound);
    }
}

void HostAddressFetcher::removeLookupHostReceiver(std::string_view hostName, HostAddressFetcherCallback callback, void *pData)
{
    const std::string hostNameStr(hostName);
    auto it = m_addedReceivers.find(hostNameStr);
    if (it != m_addedReceivers.end())
    {
        it->second.erase(std::make_pair(callback, pData));
        if (it->second.empty()
            && !(m_isInformingReceivers && m_hostNameBeingInformed == hostName))
            m_addedReceivers.erase(it);
    }
}

size_t HostAddressFetcher::lookupReceiverCount(std::string_view hostName) const
{
    auto it = m_addedReceivers.find(std::string(hostName));
    if (it != m_addedReceivers.end())
        return it->second.size();
    else
        return 0;
}

void HostAddressFetcher::onHostFound(const QHostInfo &hostInfo)
{
    m_isInformingReceivers = true;
    m_hostNameBeingInformed = hostInfo.hostName().toStdString();
    auto it = m_addedReceivers.find(m_hostNameBeingInformed);
    if (it != m_addedReceivers.end())
    {
        std::vector<std::string> addresses(hostInfo.addresses().size());
        const auto addressesList = hostInfo.addresses();
        for (auto i = 0; i < addresses.size(); ++i)
            addresses[i] = addressesList[i].toString().toStdString();
        while (!it->second.empty())
        {
            auto [callback, pData] = *(it->second.begin());
            it->second.erase(it->second.begin());
            if (callback != nullptr)
                callback(addresses, pData);
        }
        m_addedReceivers.erase(it);
    }
    m_hostNameBeingInformed.clear();
    m_isInformingReceivers = false;
}

}
