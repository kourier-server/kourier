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

#include "MetaTypeSystem.h"
#include <atomic>


namespace Kourier
{

NoDestroy<QMutex> MetaTypeSystem::m_metaTypesIdsLock;
NoDestroy<std::map<std::string_view, quint64>> MetaTypeSystem::m_metaTypesIds;
NoDestroy<QMutex> MetaTypeSystem::m_metaMethodsIdsLock;
NoDestroy<std::forward_list<std::pair<MetaTypeSystem::T_GENERIC_METHOD_PTR, quint64>>> MetaTypeSystem::m_metaMethodsIds;
NoDestroy<QMutex> MetaTypeSystem::m_metaFunctionsIdsLock;
NoDestroy<std::forward_list<std::pair<MetaTypeSystem::T_GENERIC_FUNCTION_PTR, quint64>>> MetaTypeSystem::m_metaFunctionsIds;

quint64 MetaTypeSystem::createUniqueId()
{
    static constinit NoDestroy<std::atomic_uint64_t> counter{0};
    return ++counter();
}

NoDestroy<QMutex> MetaSignalSlotConnection::m_connectionsLock;
NoDestroy<std::forward_list<std::unique_ptr<MetaSignalSlotConnection>>> MetaSignalSlotConnection::m_connections;

}

