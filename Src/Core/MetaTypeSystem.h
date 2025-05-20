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

#ifndef KOURIER_META_TYPE_SYSTEM_H
#define KOURIER_META_TYPE_SYSTEM_H

#include "SDK.h"
#include "NoDestroy.h"
#include <QMutex>
#include <QtLogging>
#include <type_traits>
#include <forward_list>
#include <string_view>
#include <tuple>
#include <map>
#include <memory>


namespace Kourier
{

template <class T_Functor>
struct MetaInvocable;

enum class InvocableType {Method, Function, Functor};

template <class TRet, class F, class... Args>
struct MetaInvocable<TRet(F::*)(Args...)>
{
    using T_ArgsAsTuple = std::tuple<Args...>;
    using T_ArgsAsTupleRef = std::tuple<Args&&...>;
    using T_Function = TRet(*)(Args...);
    using T_Class = F;
    using T_Invocable = TRet(F::*)(Args...);
    static constexpr std::size_t ArgsSize = sizeof...(Args);
    static constexpr InvocableType type = InvocableType::Method;
};

template <class TRet, class F, class... Args>
struct MetaInvocable<TRet(F::*)(Args...) const>
{
    using T_ArgsAsTuple = std::tuple<Args...>;
    using T_ArgsAsTupleRef = std::tuple<Args&&...>;
    using T_Function = TRet(*)(Args...);
    using T_Class = F;
    using T_Invocable = TRet(F::*)(Args...) const;
    static constexpr std::size_t ArgsSize = sizeof...(Args);
    static constexpr InvocableType type = InvocableType::Method;
};

template <class TRet, class... Args>
struct MetaInvocable<TRet(*)(Args...)>
{
    using T_ArgsAsTuple = std::tuple<Args...>;
    using T_ArgsAsTupleRef = std::tuple<Args&&...>;
    using T_Function = TRet(*)(Args...);
    using T_Class = void;
    using T_Invocable = TRet(*)(Args...);
    static constexpr std::size_t ArgsSize = sizeof...(Args);
    static constexpr InvocableType type = InvocableType::Function;
};

template <class T_Functor>
struct MetaInvocable
{
    static_assert(std::is_member_function_pointer_v<decltype(&T_Functor::operator())>);
    using T_ArgsAsTuple = typename MetaInvocable<decltype(&T_Functor::operator())>::T_ArgsAsTuple;
    using T_ArgsAsTupleRef = typename MetaInvocable<decltype(&T_Functor::operator())>::T_ArgsAsTupleRef;
    using T_Function = typename MetaInvocable<decltype(&T_Functor::operator())>::T_Function;
    using T_Class = void;
    static constexpr std::size_t ArgsSize = MetaInvocable<decltype(&T_Functor::operator())>::ArgsSize;
private:
    template <bool condition, auto trueValue, auto falseValue>
    static constexpr auto conditional_value()
    {
        if constexpr (condition)
        {
            return trueValue;
        }
        else
        {
            return falseValue;
        }
    }

public:
    static constexpr InvocableType type = conditional_value<std::is_convertible_v<T_Functor, T_Function>, InvocableType::Function, InvocableType::Functor>();
    using T_Invocable = std::conditional_t<std::is_convertible_v<T_Functor, T_Function>, T_Function, T_Functor>;
};

class Object;

class KOURIER_EXPORT MetaTypeSystem
{
public:
    using T_GENERIC_METHOD_PTR = void (MetaTypeSystem::*)(); // see C++20 spec 7.6.1.9 (10.1)
    using T_GENERIC_FUNCTION_PTR = void*;
    template <class T_Class>
    static quint64 metaTypeId(std::string_view className)
    {
        static_assert(std::is_base_of_v<Object, T_Class>);
        if (className.empty())
            qFatal("Failed to fetch class meta type. Given class name is empty.");
        QMutexLocker locker(&m_metaTypesIdsLock());
        auto it = m_metaTypesIds().find(className);
        if (it != m_metaTypesIds().end())
            return it->second;
        else
            return m_metaTypesIds().emplace(className, createUniqueId()).first->second;
    }
    template <class T_Invocable>
    static quint64 metaInvocableId(T_Invocable invocable)
    {
        if constexpr (std::is_same_v<std::nullptr_t, T_Invocable>)
        {
            return 0;
        }
        else if constexpr (MetaInvocable<T_Invocable>::type == InvocableType::Method)
        {
            if (invocable == nullptr)
                qFatal("Failed to fetch invocable meta type. Given pointer to method is null.");
            QMutexLocker locker(&m_metaMethodsIdsLock());
            for (const auto &it : m_metaMethodsIds())
            {
                if (reinterpret_cast<T_Invocable>(it.first) == invocable) // see C++20 spec 7.6.1.9 (10.1)
                    return it.second;
            }
            return m_metaMethodsIds().emplace_front(reinterpret_cast<T_GENERIC_METHOD_PTR>(invocable), createUniqueId()).second;
        }
        else if constexpr (MetaInvocable<T_Invocable>::type == InvocableType::Function)
        {
            if (invocable == nullptr)
                qFatal("Failed to fetch invocable meta type. Given pointer to function is null.");
            QMutexLocker locker(&m_metaFunctionsIdsLock());
            for (const auto &it : m_metaFunctionsIds())
            {
                if (reinterpret_cast<MetaInvocable<T_Invocable>::T_Function>(it.first) == invocable)
                    return it.second;
            }
            return m_metaFunctionsIds().emplace_front(reinterpret_cast<T_GENERIC_FUNCTION_PTR>(static_cast<MetaInvocable<T_Invocable>::T_Function>(invocable)), createUniqueId()).second;
        }
        else
        {
            return 0;
        }
    }

private:
    static quint64 createUniqueId();
    static NoDestroy<QMutex> m_metaTypesIdsLock;
    static NoDestroy<std::map<std::string_view, quint64>> m_metaTypesIds;
    static NoDestroy<QMutex> m_metaMethodsIdsLock;
    static NoDestroy<std::forward_list<std::pair<T_GENERIC_METHOD_PTR, quint64>>> m_metaMethodsIds;
    static NoDestroy<QMutex> m_metaFunctionsIdsLock;
    static NoDestroy<std::forward_list<std::pair<T_GENERIC_FUNCTION_PTR, quint64>>> m_metaFunctionsIds;
};

class KOURIER_EXPORT MetaSignalSlotConnection
{
public:
    virtual ~MetaSignalSlotConnection() = default;
    virtual void callSlot(Object *pObject, void *pPackedArgs) = 0;
    quint64 signalId() {return m_signalId;}
    quint64 slotId() {return m_slotId;}
    virtual bool isManaged() {return (m_slotId > 0);}
    template <class T_PtrToSignal, class T_PtrToSlot>
    static constexpr bool checkSignalSlotCompatibility()
    {return checkSignalSlotArgsCompatibility<typename MetaInvocable<T_PtrToSignal>::T_ArgsAsTuple, typename MetaInvocable<T_PtrToSlot>::T_ArgsAsTuple>();}

protected:
    MetaSignalSlotConnection(quint64 signalId, quint64 slotId) : m_signalId(signalId), m_slotId(slotId) {}

    template <size_t N, class T_Tuple>
    constexpr inline auto truncateTuple(T_Tuple &&tuple)
    {
        return truncateTuple(tuple, std::make_index_sequence<N>());
    }
    template <class T_Tuple, std::size_t... Indices>
    constexpr inline auto truncateTuple(T_Tuple &&tuple, std::index_sequence<Indices...>)
    {
        return std::make_tuple(std::get<Indices>(tuple)...);
    }
    template <class SignalArgType, class SlotArgType>
    static constexpr bool checkSignalSlotArgCompatibility()
    {
        return std::is_convertible_v<SignalArgType, SlotArgType>;
    }

    template <class SignalArgsTuple, class SlotArgsTuple, std::size_t... Indices>
    static constexpr bool checkSignalSlotArgsTypesCompatibility(std::index_sequence<Indices...>)
    {
        return (checkSignalSlotArgCompatibility<std::tuple_element_t<Indices, SignalArgsTuple>, std::tuple_element_t<Indices, SlotArgsTuple>>() && ...);
    }
    template <class SignalArgsTuple, class SlotArgsTuple>
    static constexpr bool checkSignalSlotArgsCompatibility()
    {
        if constexpr (std::tuple_size<SlotArgsTuple>::value == 0)
            return true;
        else if constexpr (std::tuple_size<SignalArgsTuple>::value >= std::tuple_size<SlotArgsTuple>::value)
            return checkSignalSlotArgsTypesCompatibility<SignalArgsTuple, SlotArgsTuple>(std::make_index_sequence<std::tuple_size<SlotArgsTuple>::value>{});
        else
            return false;
    }

protected:
    static NoDestroy<QMutex> m_connectionsLock;
    static NoDestroy<std::forward_list<std::unique_ptr<MetaSignalSlotConnection>>> m_connections;

private:
    const quint64 m_signalId;
    const quint64 m_slotId;
};

template <class T_PtrToSignal, class T_Slot>
class MetaSignalSlotConnectionT : public MetaSignalSlotConnection
{
public:
    using SignalInfo = MetaInvocable<T_PtrToSignal>;
    using SlotInfo = MetaInvocable<T_Slot>;
    static_assert(std::is_base_of_v<Object, typename SignalInfo::T_Class>
                  && (std::is_base_of_v<Object, typename SlotInfo::T_Class> || SlotInfo::type != InvocableType::Method));
    static_assert(MetaSignalSlotConnection::checkSignalSlotArgsCompatibility<typename SignalInfo::T_ArgsAsTuple, typename SlotInfo::T_ArgsAsTuple>());
    ~MetaSignalSlotConnectionT() override = default;
    void callSlot(Object *pObject, void *pPackedArgs) override
    {
        // I use a bunch of constexprs for performance reasons. If signal has more than 10 args
        // I use std::apply directly.
        auto &argsAsTuple = *static_cast<SignalInfo::T_ArgsAsTupleRef*>(pPackedArgs);
        if constexpr (SlotInfo::ArgsSize == 0)
        {
            if constexpr (SlotInfo::type == InvocableType::Method)
            {
                auto *pDowncastObject = static_cast<SlotInfo::T_Class*>(pObject);
                (pDowncastObject->*m_slot)();
            }
            else
            {
                m_slot();
            }
        }
        else if constexpr (SlotInfo::ArgsSize == 1)
        {
            if constexpr (SlotInfo::type == InvocableType::Method)
            {
                auto *pDowncastObject = static_cast<SlotInfo::T_Class*>(pObject);
                (pDowncastObject->*m_slot)(std::get<0>(argsAsTuple));
            }
            else
            {
                m_slot(std::get<0>(argsAsTuple));
            }
        }
        else if constexpr (SlotInfo::ArgsSize == 2)
        {
            if constexpr (SlotInfo::type == InvocableType::Method)
            {
                auto *pDowncastObject = static_cast<SlotInfo::T_Class*>(pObject);
                (pDowncastObject->*m_slot)(std::get<0>(argsAsTuple), std::get<1>(argsAsTuple));
            }
            else
            {
                m_slot(std::get<0>(argsAsTuple), std::get<1>(argsAsTuple));
            }
        }
        else if constexpr (SlotInfo::ArgsSize == 3)
        {
            if constexpr (SlotInfo::type == InvocableType::Method)
            {
                auto *pDowncastObject = static_cast<SlotInfo::T_Class*>(pObject);
                (pDowncastObject->*m_slot)(std::get<0>(argsAsTuple), std::get<1>(argsAsTuple), std::get<2>(argsAsTuple));
            }
            else
            {
                m_slot(std::get<0>(argsAsTuple), std::get<1>(argsAsTuple), std::get<2>(argsAsTuple));
            }
        }
        else if constexpr (SlotInfo::ArgsSize == 4)
        {
            if constexpr (SlotInfo::type == InvocableType::Method)
            {
                auto *pDowncastObject = static_cast<SlotInfo::T_Class*>(pObject);
                (pDowncastObject->*m_slot)(std::get<0>(argsAsTuple), std::get<1>(argsAsTuple), std::get<2>(argsAsTuple), std::get<3>(argsAsTuple));
            }
            else
            {
                m_slot(std::get<0>(argsAsTuple), std::get<1>(argsAsTuple), std::get<2>(argsAsTuple), std::get<3>(argsAsTuple));
            }
        }
        else if constexpr (SlotInfo::ArgsSize == 5)
        {
            if constexpr (SlotInfo::type == InvocableType::Method)
            {
                auto *pDowncastObject = static_cast<SlotInfo::T_Class*>(pObject);
                (pDowncastObject->*m_slot)(std::get<0>(argsAsTuple), std::get<1>(argsAsTuple), std::get<2>(argsAsTuple), std::get<3>(argsAsTuple), std::get<4>(argsAsTuple));
            }
            else
            {
                m_slot(std::get<0>(argsAsTuple), std::get<1>(argsAsTuple), std::get<2>(argsAsTuple), std::get<3>(argsAsTuple), std::get<4>(argsAsTuple));
            }
        }
        else if constexpr (SlotInfo::ArgsSize == 6)
        {
            if constexpr (SlotInfo::type == InvocableType::Method)
            {
                auto *pDowncastObject = static_cast<SlotInfo::T_Class*>(pObject);
                (pDowncastObject->*m_slot)(std::get<0>(argsAsTuple), std::get<1>(argsAsTuple), std::get<2>(argsAsTuple), std::get<3>(argsAsTuple), std::get<4>(argsAsTuple), std::get<5>(argsAsTuple));
            }
            else
            {
                m_slot(std::get<0>(argsAsTuple), std::get<1>(argsAsTuple), std::get<2>(argsAsTuple), std::get<3>(argsAsTuple), std::get<4>(argsAsTuple), std::get<5>(argsAsTuple));
            }
        }
        else if constexpr (SlotInfo::ArgsSize == 7)
        {
            if constexpr (SlotInfo::type == InvocableType::Method)
            {
                auto *pDowncastObject = static_cast<SlotInfo::T_Class*>(pObject);
                (pDowncastObject->*m_slot)(std::get<0>(argsAsTuple), std::get<1>(argsAsTuple), std::get<2>(argsAsTuple), std::get<3>(argsAsTuple), std::get<4>(argsAsTuple), std::get<5>(argsAsTuple), std::get<6>(argsAsTuple));
            }
            else
            {
                m_slot(std::get<0>(argsAsTuple), std::get<1>(argsAsTuple), std::get<2>(argsAsTuple), std::get<3>(argsAsTuple), std::get<4>(argsAsTuple), std::get<5>(argsAsTuple), std::get<6>(argsAsTuple));
            }
        }
        else if constexpr (SlotInfo::ArgsSize == 8)
        {
            if constexpr (SlotInfo::type == InvocableType::Method)
            {
                auto *pDowncastObject = static_cast<SlotInfo::T_Class*>(pObject);
                (pDowncastObject->*m_slot)(std::get<0>(argsAsTuple), std::get<1>(argsAsTuple), std::get<2>(argsAsTuple), std::get<3>(argsAsTuple), std::get<4>(argsAsTuple), std::get<5>(argsAsTuple), std::get<6>(argsAsTuple), std::get<7>(argsAsTuple));
            }
            else
            {
                m_slot(std::get<0>(argsAsTuple), std::get<1>(argsAsTuple), std::get<2>(argsAsTuple), std::get<3>(argsAsTuple), std::get<4>(argsAsTuple), std::get<5>(argsAsTuple), std::get<6>(argsAsTuple), std::get<7>(argsAsTuple));
            }
        }
        else if constexpr (SlotInfo::ArgsSize == 9)
        {
            if constexpr (SlotInfo::type == InvocableType::Method)
            {
                auto *pDowncastObject = static_cast<SlotInfo::T_Class*>(pObject);
                (pDowncastObject->*m_slot)(std::get<0>(argsAsTuple), std::get<1>(argsAsTuple), std::get<2>(argsAsTuple), std::get<3>(argsAsTuple), std::get<4>(argsAsTuple), std::get<5>(argsAsTuple), std::get<6>(argsAsTuple), std::get<7>(argsAsTuple), std::get<8>(argsAsTuple));
            }
            else
            {
                m_slot(std::get<0>(argsAsTuple), std::get<1>(argsAsTuple), std::get<2>(argsAsTuple), std::get<3>(argsAsTuple), std::get<4>(argsAsTuple), std::get<5>(argsAsTuple), std::get<6>(argsAsTuple), std::get<7>(argsAsTuple), std::get<8>(argsAsTuple));
            }
        }
        else if constexpr (SlotInfo::ArgsSize == 10)
        {
            if constexpr (SlotInfo::type == InvocableType::Method)
            {
                auto *pDowncastObject = static_cast<SlotInfo::T_Class*>(pObject);
                (pDowncastObject->*m_slot)(std::get<0>(argsAsTuple), std::get<1>(argsAsTuple), std::get<2>(argsAsTuple), std::get<3>(argsAsTuple), std::get<4>(argsAsTuple), std::get<5>(argsAsTuple), std::get<6>(argsAsTuple), std::get<7>(argsAsTuple), std::get<8>(argsAsTuple), std::get<9>(argsAsTuple));
            }
            else
            {
                m_slot(std::get<0>(argsAsTuple), std::get<1>(argsAsTuple), std::get<2>(argsAsTuple), std::get<3>(argsAsTuple), std::get<4>(argsAsTuple), std::get<5>(argsAsTuple), std::get<6>(argsAsTuple), std::get<7>(argsAsTuple), std::get<8>(argsAsTuple), std::get<9>(argsAsTuple));
            }
        }
        else
        {
            if constexpr (SlotInfo::type == InvocableType::Method)
            {
                auto *pDowncastObject = static_cast<SlotInfo::T_Class*>(pObject);
                std::apply(m_slot, std::tuple_cat(std::forward_as_tuple(pDowncastObject), truncateTuple<SlotInfo::ArgsSize>(argsAsTuple)));
            }
            else
            {
                std::apply(m_slot, truncateTuple<SlotInfo::ArgsSize>(argsAsTuple));
            }
        }
    }
    static MetaSignalSlotConnection *create(T_PtrToSignal pSignal, T_Slot pSlot)
    {
        if constexpr (SlotInfo::type == InvocableType::Functor)
        {
            return new MetaSignalSlotConnectionT(pSignal, pSlot);
        }
        else
        {
            QMutexLocker locker(&m_connectionsLock());
            for (auto &it : m_connections())
            {
                if (it->signalId() == MetaTypeSystem::metaInvocableId(pSignal)
                    && it->slotId() == MetaTypeSystem::metaInvocableId(pSlot))
                    return it.get();
            }
            return m_connections().emplace_front(new MetaSignalSlotConnectionT(pSignal, pSlot)).get();
        }
    }

private:
    MetaSignalSlotConnectionT(T_PtrToSignal pSignal, T_Slot pSlot) :
        MetaSignalSlotConnection(MetaTypeSystem::metaInvocableId(pSignal), MetaTypeSystem::metaInvocableId(pSlot)),
        m_pSignal(pSignal),
        m_slot(pSlot)
    {
        assert(m_pSignal);
        if constexpr (SlotInfo::type == InvocableType::Method)
        {
            assert(m_slot);
        }
    }

private:
    T_PtrToSignal m_pSignal;
    SlotInfo::T_Invocable m_slot;
};

}

#endif // KOURIER_META_TYPE_SYSTEM_H
