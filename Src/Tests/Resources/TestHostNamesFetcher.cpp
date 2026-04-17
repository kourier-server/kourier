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
#include <Spectator>

using namespace Spectator;

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
                127.10.20.50 ipv4-ipv6-addresses.test.local
                127.10.20.60 ipv4-ipv6-addresses.test.local
                127.10.20.70 ipv4-ipv6-addresses.test.local
                127.10.20.80 ipv4-ipv6-addresses.test.local
                127.10.20.90 ipv4-ipv6-addresses.test.local
                ::1 ipv4-ipv6-addresses.test.local
                127.10.20.100 ipv4-address.test.local
                ::1 ipv6-address.test.local
                127.127.127.10 ipv4-addresses.test.local
                127.127.127.20 ipv4-addresses.test.local
                127.127.127.30 ipv4-addresses.test.local
                127.127.127.40 ipv4-addresses.test.local
                127.127.127.50 ipv4-addresses.test.local
                127.127.127.60 ipv4-addresses.test.local
                127.127.127.70 ipv4-addresses.test.local
                127.127.127.80 ipv4-addresses.test.local
                127.127.127.90 ipv4-addresses.test.local
                127.127.127.100 ipv4-addresses.test.local
            )");
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
    static HostAddressesFetcher hostAddressesFetcher("ipv4-ipv6-addresses.test.local");
    return {hostAddressesFetcher.hostName(), hostAddressesFetcher.hostAddresses()};
}

std::pair<std::string_view, QList<QHostAddress>> TestHostNamesFetcher::hostNameWithIpv4Address()
{
    static HostAddressesFetcher hostAddressesFetcher("ipv4-address.test.local");
    return {hostAddressesFetcher.hostName(), hostAddressesFetcher.hostAddresses()};
}

std::pair<std::string_view, QList<QHostAddress>> TestHostNamesFetcher::hostNameWithIpv6Address()
{
    static HostAddressesFetcher hostAddressesFetcher("ipv6-address.test.local");
    return {hostAddressesFetcher.hostName(), hostAddressesFetcher.hostAddresses()};
}

std::pair<std::string_view, QList<QHostAddress>> TestHostNamesFetcher::hostNameWithIpv4Addresses()
{
    static HostAddressesFetcher hostAddressesFetcher("ipv4-addresses.test.local");
    return {hostAddressesFetcher.hostName(), hostAddressesFetcher.hostAddresses()};
}

}
