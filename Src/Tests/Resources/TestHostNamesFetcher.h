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

#ifndef KOURIER_TEST_HOST_NAMES_FETCHER_H
#define KOURIER_TEST_HOST_NAMES_FETCHER_H

#include <QHostAddress>
#include <QList>
#include <string_view>
#include <utility>


namespace Kourier
{

class TestHostNamesFetcher
{
public:
    static std::pair<std::string_view, QList<QHostAddress>> hostNameWithIpv4Ipv6Addresses();
    static std::pair<std::string_view, QList<QHostAddress>> hostNameWithIpv4Address();
    static std::pair<std::string_view, QList<QHostAddress>> hostNameWithIpv6Address();
};

}

#endif // KOURIER_TEST_HOST_NAMES_FETCHER_H
