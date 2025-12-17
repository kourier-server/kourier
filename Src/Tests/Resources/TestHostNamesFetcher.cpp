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

#include "TestHostNamesFetcher.h"
#include <QHostInfo>
#include <QMutex>
#include <atomic>
#include "../Spectator/Spectator.h"


namespace Kourier::TestResources
{

class HostAddressesFetcher
{
public:
    HostAddressesFetcher(std::string_view hostName) : m_hostName(hostName) {}
    ~HostAddressesFetcher() = default;
    std::string_view hostName() const {return m_hostName;}
    QList<QHostAddress> hostAddresses()
    {
        if (!m_firstRun.load(std::memory_order_relaxed))
            return m_hostAddresses;
        QMutexLocker locker(&m_lock);
        if (!m_firstRun.load())
            return m_hostAddresses;
        else
        {
            m_firstRun.store(false);
            m_hostAddresses = getHostAddressesFromHostName(m_hostName);
            return m_hostAddresses;
        }
    }

private:
    static QList<QHostAddress> getHostAddressesFromHostName(std::string_view hostName)
    {
        QHostInfo hostInfo = QHostInfo::fromName(QString::fromLatin1(hostName));
        if (hostInfo.error() != QHostInfo::NoError)
        {
            FAIL(R"(
                Failed to fetch addresses associated with test host name.
                Please, add the following to /etc/hosts:
                127.10.20.50 ipv4_ipv6_addresses.onlocalhost
                127.10.20.60 ipv4_ipv6_addresses.onlocalhost
                127.10.20.70 ipv4_ipv6_addresses.onlocalhost
                127.10.20.80 ipv4_ipv6_addresses.onlocalhost
                127.10.20.90 ipv4_ipv6_addresses.onlocalhost
                ::1 ipv4_ipv6_addresses.onlocalhost
                127.10.20.100 ipv4_address.onlocalhost
                ::1 ipv6_address.onlocalhost)");
        }
        return hostInfo.addresses();
    }

private:
    std::string_view m_hostName;
    std::atomic_bool m_firstRun = true;
    QMutex m_lock;
    QList<QHostAddress> m_hostAddresses;
};

std::pair<std::string_view, QList<QHostAddress>> TestHostNamesFetcher::hostNameWithIpv4Ipv6Addresses()
{
    static HostAddressesFetcher hostAddressesFetcher("ipv4_ipv6_addresses.onlocalhost");
    return {hostAddressesFetcher.hostName(), hostAddressesFetcher.hostAddresses()};
}

std::pair<std::string_view, QList<QHostAddress>> TestHostNamesFetcher::hostNameWithIpv4Address()
{
    static HostAddressesFetcher hostAddressesFetcher("ipv4_address.onlocalhost");
    return {hostAddressesFetcher.hostName(), hostAddressesFetcher.hostAddresses()};
}

std::pair<std::string_view, QList<QHostAddress>> TestHostNamesFetcher::hostNameWithIpv6Address()
{
    static HostAddressesFetcher hostAddressesFetcher("ipv6_address.onlocalhost");
    return {hostAddressesFetcher.hostName(), hostAddressesFetcher.hostAddresses()};
}

}
