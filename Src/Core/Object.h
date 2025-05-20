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

#ifndef KOURIER_OBJECT_H
#define KOURIER_OBJECT_H

#include "MetaTypeSystem.h"
#include "EpollEventNotifier.h"
#include <QtGlobal>


namespace Kourier
{

using Signal = void;


class KOURIER_EXPORT Object
{
public:
    Object();
    virtual ~Object();
    void scheduleForDeletion() {EpollEventNotifier::current()->scheduleForDeletion(this);}

protected:
    using _kourier_T_Prev = std::nullptr_t;
    using _kourier_T_Head = Object;

public:
    inline static quint64 metaTypeId() {return ::Kourier::MetaTypeSystem::metaTypeId<::Kourier::Object>("Kourier::Object");}

    virtual bool inherits(quint64 typeId) const
    {
        return typeId == Object::metaTypeId();
    }

private:
    template <class T>
    static constexpr bool hasQualifiers()
    {
        return std::is_const_v<T> || std::is_volatile_v<T> || std::is_reference_v<T>
               || std::is_pointer_v<T> || std::is_array_v<T>
               || std::is_function_v<T> || std::is_member_pointer_v<T>;
    }
    void addEmitter(Object &emitter);
    void removeEmitter(Object &emitter);
    void removeReceiver(Object &receiver) {disconnect(this, nullptr, &receiver, nullptr);}

public:
    template <class T_PtrToSignal, class T_PtrToSlot>
    inline static void connect(MetaInvocable<T_PtrToSignal>::T_Class *pSender, T_PtrToSignal pSignal, T_PtrToSlot pSlot)
    {
        static_assert(MetaSignalSlotConnection::checkSignalSlotCompatibility<T_PtrToSignal, T_PtrToSlot>(),
                      "Signal and slot arguments are not compatible");
        static_assert(!std::is_member_function_pointer_v<T_PtrToSlot>);
        connect(pSender, pSignal, nullptr, pSlot);
    }

    template <class T_PtrToSignal, class T_PtrToSlot>
    inline static void connect(MetaInvocable<T_PtrToSignal>::T_Class *pSender, T_PtrToSignal pSignal,
        MetaInvocable<T_PtrToSlot>::T_Class *pReceiver, T_PtrToSlot pSlot)
    {
        static_assert(MetaInvocable<T_PtrToSlot>::type == InvocableType::Method);
        static_assert(MetaSignalSlotConnection::checkSignalSlotCompatibility<T_PtrToSignal, T_PtrToSlot>(),
                      "Signal and slot arguments are not compatible");
        static_assert(std::is_base_of_v<Object, typename MetaInvocable<T_PtrToSignal>::T_Class>
                      && std::is_base_of_v<Object, typename MetaInvocable<T_PtrToSlot>::T_Class>);
        assert(pSender && pReceiver);
        if (pSender && pReceiver)
        {
            pSender->m_pSignalSlotConnections.push_front(SignalSlotConnectionData{.pReceiver = pReceiver, .pMetaConnection = MetaSignalSlotConnectionT<T_PtrToSignal, T_PtrToSlot>::create(pSignal, pSlot)});
            pReceiver->addEmitter(*pSender);
        }
    }

    template <class T_PtrToSignal, class T_PtrToSlot>
    inline static void connect(MetaInvocable<T_PtrToSignal>::T_Class *pSender, T_PtrToSignal pSignal,
        Object *pReceiver, T_PtrToSlot pSlot)
    {
        static_assert(MetaInvocable<T_PtrToSlot>::type != InvocableType::Method
                      && !std::is_same_v<std::nullptr_t, T_PtrToSlot>);
        static_assert(MetaSignalSlotConnection::checkSignalSlotCompatibility<T_PtrToSignal, T_PtrToSlot>(),
                      "Signal and slot arguments are not compatible");
        static_assert(std::is_base_of_v<Object, typename MetaInvocable<T_PtrToSignal>::T_Class>);
        assert(pSender);
        if (pSender)
        {
            pSender->m_pSignalSlotConnections.push_front(SignalSlotConnectionData{.pReceiver = pReceiver, .pMetaConnection = MetaSignalSlotConnectionT<T_PtrToSignal, T_PtrToSlot>::create(pSignal, pSlot)});
            if (pReceiver)
                pReceiver->addEmitter(*pSender);
        }
    }

    template <class T_PtrToSignal, class T_PtrToSlot>
    inline static void disconnect(Object *pSender, T_PtrToSignal pSignal, Object *pReceiver, T_PtrToSlot pSlot)
    {
        static_assert((std::is_same_v<std::nullptr_t, T_PtrToSignal> || std::is_member_function_pointer_v<T_PtrToSignal>)
                && (std::is_same_v<std::nullptr_t, T_PtrToSlot> || std::is_member_function_pointer_v<T_PtrToSlot>));
        assert(pSender);
        if (!pSender)
            return;
        const quint64 signalId = MetaTypeSystem::metaInvocableId(pSignal);
        const quint64 slotId = MetaTypeSystem::metaInvocableId(pSlot);
        auto itPrev = pSender->m_pSignalSlotConnections.before_begin();
        for (auto it = pSender->m_pSignalSlotConnections.begin(); it != pSender->m_pSignalSlotConnections.end();)
        {
            if (it->pMetaConnection)
            {
                const bool hasMatchedReceiver = !pReceiver || pReceiver == it->pReceiver;
                const bool hasMatchedSignal = !signalId || signalId == it->pMetaConnection->signalId();
                const bool hasMatchedSlot = !slotId || slotId == it->pMetaConnection->slotId();
                if (hasMatchedReceiver && hasMatchedSignal && hasMatchedSlot)
                {
                    if (!it->pMetaConnection->isManaged())
                        delete it->pMetaConnection;
                    it->pMetaConnection = nullptr;
                    if (it->pReceiver)
                        static_cast<Object*>(it->pReceiver)->removeEmitter(*pSender);
                    if (!pSender->m_isEmitting)
                    {
                        it = pSender->m_pSignalSlotConnections.erase_after(itPrev);
                        continue;
                    }
                    else
                        pSender->m_hasToCleanupConnections = true;
                }
            }
            itPrev = it;
            ++it;
        }
    }
    inline void disconnect() {disconnect(this, nullptr, nullptr, nullptr);}
    template <class T_PtrToReceiverOrSignal>
    inline void disconnect(T_PtrToReceiverOrSignal pReceiverOrSignal)
    {
        static_assert(!std::is_same_v<std::nullptr_t, T_PtrToReceiverOrSignal>
                      && (std::is_member_function_pointer_v<T_PtrToReceiverOrSignal>
                          || !hasQualifiers<std::remove_pointer_t<std::decay_t<T_PtrToReceiverOrSignal>>>()));
        if constexpr (std::is_member_function_pointer_v<T_PtrToReceiverOrSignal>)
           disconnect(this, pReceiverOrSignal, nullptr, nullptr);
        else
        {
            using T_PtrToObject = std::decay_t<T_PtrToReceiverOrSignal>;
            using T_Object = std::remove_pointer_t<T_PtrToObject>;
            static_assert(std::is_base_of_v<Object, T_Object> && std::is_pointer_v<T_PtrToObject>);
            disconnect(this, nullptr, pReceiverOrSignal, nullptr);
        }
    }

    template <class T>
    inline T tryCast()
    {
        using T_Type = std::remove_cv_t<std::remove_pointer_t<std::decay_t<T>>>;
        static_assert(!hasQualifiers<T_Type>());
        static_assert(std::is_base_of_v<Object, T_Type>);
        return inherits(T_Type::metaTypeId()) ? static_cast<T>(this) : nullptr;
    }

    template <class T>
    inline T tryCast() const
    {
        static_assert(std::is_const_v<std::remove_pointer_t<std::decay_t<T>>>);
        using T_Type = std::remove_cv_t<std::remove_pointer_t<std::decay_t<T>>>;
        static_assert(!hasQualifiers<T_Type>());
        static_assert(std::is_base_of_v<Object, T_Type>);
        return inherits(T_Type::metaTypeId()) ? static_cast<T>(this) : nullptr;
    }

protected:
    template <class...SignalArgs>
    inline void emitSignal(quint64 signalId, SignalArgs&&...args)
    {
        bool isFirstToEmit = !m_isEmitting;
        m_isEmitting = true;
        auto packedArgs = std::forward_as_tuple(args...);
        for (auto it = m_pSignalSlotConnections.begin(); it != m_pSignalSlotConnections.end(); ++it)
        {
            if (it->pMetaConnection && it->pMetaConnection->signalId() == signalId)
                it->pMetaConnection->callSlot(static_cast<Object*>(it->pReceiver), &packedArgs);
        }
        if (isFirstToEmit)
        {
            m_isEmitting = false;
            if (m_hasToCleanupConnections)
            {
                m_hasToCleanupConnections = false;
                auto itPrev = m_pSignalSlotConnections.before_begin();
                for (auto it = m_pSignalSlotConnections.begin(); it != m_pSignalSlotConnections.end();)
                {
                    if (!it->pMetaConnection)
                        it = m_pSignalSlotConnections.erase_after(itPrev);
                    else
                    {
                        itPrev = it;
                        ++it;
                    }
                }
            }
        }
    }

private:
    std::map<Object*, size_t> m_pConnectedEmitters;
    struct SignalSlotConnectionData
    {
        void *pReceiver = nullptr;
        MetaSignalSlotConnection *pMetaConnection = nullptr;
    };
    std::forward_list<SignalSlotConnectionData> m_pSignalSlotConnections;
    bool m_isEmitting = false;
    bool m_hasToCleanupConnections = false;
};

}

#define KOURIER_OBJECT(CLASS_NAME) \
    private: \
    using KOURIER_OBJECT_CLASS_NAME = CLASS_NAME; \
    protected: \
        using _kourier_T_Prev = _kourier_T_Head; \
        using _kourier_T_Head = CLASS_NAME; \
    public: \
    inline static quint64 metaTypeId() {return ::Kourier::MetaTypeSystem::metaTypeId<::CLASS_NAME>(#CLASS_NAME);} \
    bool inherits(quint64 typeId) const override \
    { \
        return typeId == CLASS_NAME::metaTypeId() || _kourier_T_Prev::inherits(typeId); \
    } \
    private:
#define KOURIER_SIGNAL(PTR_TO_SIGNAL, ...) \
{ \
    static const ::Kourier::NoDestroy<quint64> id{::Kourier::MetaTypeSystem::metaInvocableId(PTR_TO_SIGNAL)}; \
    ::Kourier::Object::emitSignal(id(),##__VA_ARGS__); \
}
#define KOURIER_OVERLOAD(...) qOverload<__VA_ARGS__>
#define KOURIER_SIGNAL_OVERLOAD(PTR_TO_SIGNAL, TYPES, ...) \
{ \
    static const ::Kourier::NoDestroy<quint64> id{::Kourier::MetaTypeSystem::metaInvocableId(KOURIER_OVERLOAD TYPES(PTR_TO_SIGNAL))}; \
    ::Kourier::Object::emitSignal(id(),##__VA_ARGS__); \
}

#endif // KOURIER_OBJECT_H
