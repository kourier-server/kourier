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

#ifndef KOURIER_HOST_ADDRESS_FETCHER_H
#define KOURIER_HOST_ADDRESS_FETCHER_H

#include <QObject>
#include <QString>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <utility>


namespace Kourier
{

using HostAddressFetcherCallback = void (*)(const std::vector<std::string> &, void *);

class HostAddressFetcher : public QObject
{
Q_OBJECT
public:
    ~HostAddressFetcher() override = default;
    static void addHostLookup(std::string_view hostName, HostAddressFetcherCallback callback, void *pData);
    static void removeHostLookup(std::string_view hostName, HostAddressFetcherCallback callback, void *pData);
    static size_t receiverCount(std::string_view hostName);

private:
    HostAddressFetcher() = default;
    static HostAddressFetcher *current();
    void lookupHost(std::string_view hostName, HostAddressFetcherCallback callback, void *pData);
    void removeLookupHostReceiver(std::string_view hostName, HostAddressFetcherCallback callback, void *pData);
    size_t lookupReceiverCount(std::string_view hostName) const;

private slots:
    void onHostFound(const QHostInfo &hostInfo);

private:
    std::map<std::string, std::set<std::pair<HostAddressFetcherCallback, void*>>> m_addedReceivers;
    std::string m_hostNameBeingInformed;
    bool m_isInformingReceivers = false;
};

}

#endif // KOURIER_HOST_ADDRESS_FETCHER_H
