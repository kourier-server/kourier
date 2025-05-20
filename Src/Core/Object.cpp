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

#include "Object.h"


namespace Kourier
{

/*!
\class Kourier::Object
\brief The Object class is the base class for fast and lightweight signal-slot connections in Kourier.
*/

/*!
Creates the Object.
*/
Object::Object()
{
}

/*!
Destroys the Object. All connections having this object as sender, receiver, or context object are automatically disconnected.
*/
Object::~Object()
{
    if (m_isEmitting)
        qFatal("Failed to delete Object. Object is emitting a signal. Use Object::scheduleForDeletion instead.");
    while (!m_pConnectedEmitters.empty())
    {
        auto * const pObject = m_pConnectedEmitters.begin()->first;
        m_pConnectedEmitters.erase(m_pConnectedEmitters.begin());
        pObject->removeReceiver(*this);
    }
    for (auto it = m_pSignalSlotConnections.begin(); it != m_pSignalSlotConnections.end(); ++it)
    {
        if (it->pMetaConnection && !it->pMetaConnection->isManaged())
            delete it->pMetaConnection;
        if (it->pReceiver)
            static_cast<Object*>(it->pReceiver)->removeEmitter(*this);
    }
}

/*!
 \fn void Object::scheduleForDeletion()
 Schedule this object for deletion. This object will be deleted when control returns to the event loop.
 This method can be called more than once, provided the object has not yet been deleted.
*/

/*!
 \fn void Object::connect(MetaInvocable<T_PtrToSignal>::T_Class *pSender,
                          T_PtrToSignal pSignal,
                          T_PtrToSlot pSlot)
 Similar to calling \link Object::connect(MetaInvocable<T_PtrToSignal>::T_Class *pSender, T_PtrToSignal pSignal,
        Object *pReceiver, T_PtrToSlot pSlot) connect\endlink(\a pSender, \a pSignal, \a nullptr, \a pSlot).
 Connects \a pSignal from \a pSender to \a pSlot. \a pSlot cannot be a pointer to a non-static member function, and
 arguments in \a pSignal must be convertible to arguments in \a pSlot. \a pSlot can have fewer arguments than \a pSignal or no arguments at all.
 If \a pSlot is a lambda that captures variables, all captured variables must still exist when the lambda gets called.
 The connection is automatically terminated if \a pSender is destroyed. You can use a context object as receiver
 if you want to disconnect without having to destroy the sender, by calling \link Object::connect(MetaInvocable<T_PtrToSignal>::T_Class *pSender, T_PtrToSignal pSignal,
        Object *pReceiver, T_PtrToSlot pSlot) connect\endlink(\a pSender, \a pSignal, \a pContextObject, \a pSlot), where \a pContextObject is a
non-null pointer to an object containing Object in its inheritance hierarchy. In this case you can disconnect by destroying the given
context object, or you can call \link Object::disconnect(Object *pSender, T_PtrToSignal pSignal, Object *pReceiver, T_PtrToSlot pSlot) disconnect\endlink(\a pSender, \a pSignal, \a pContextObject, nullptr)
 to disconnect.
*/

/*!
 \fn void Object::connect(MetaInvocable<T_PtrToSignal>::T_Class *pSender,
                          T_PtrToSignal pSignal,
                          MetaInvocable<T_PtrToSlot>::T_Class *pReceiver,
                          T_PtrToSlot pSlot)
 Connects \a pSignal from \a pSender to \a pSlot on \a pReceiver.
 Arguments in \a pSignal must be convertible to arguments in \a pSlot, but \a pSlot can have fewer arguments than \a pSignal or no arguments at all.
 \a pReceiver must be non-null, and \a pSlot must be a pointer to a member function of \a pReceiver.
 Call \link Object::disconnect(Object *pSender, T_PtrToSignal pSignal, Object *pReceiver, T_PtrToSlot pSlot) disconnect\endlink(\a pSender, \a pSignal, \a pReceiver, \a pSlot) to disconnect.
 The connection is automatically terminated if either \a pSender or \a pReceiver are destroyed.
*/

/*!
 \fn void Object::connect(MetaInvocable<T_PtrToSignal>::T_Class *pSender, T_PtrToSignal pSignal,
        Object *pContextObject, T_PtrToSlot pSlot)
 Connects \a pSignal from \a pSender to \a pSlot.
 Arguments in \a pSignal must be convertible to arguments in \a pSlot, but \a pSlot can have fewer arguments than \a pSignal or no arguments at all.
 \a pSlot cannot be a pointer to a non-static member function, and \a pContextObject acts as a context object and can be null.
 You can disconnect by destroying the context object or by calling \link Object::disconnect(Object *pSender, T_PtrToSignal pSignal, Object *pContextObject, T_PtrToSlot pSlot) disconnect\endlink(\a pSender, \a pSignal, \a pContextObject, nullptr)
 to disconnect every connection from \a pSignal and \a pSender involving the context object.
 Connection is automatically terminated if either \a pSender or \a pContextObject are destroyed.
*/

/*!
 \fn void Object::disconnect(Object *pSender, T_PtrToSignal pSignal, Object *pReceiver, T_PtrToSlot pSlot)
 Disconnects \a pSignal from \a pSender to \a pSlot.
 \a pSender cannot be null. Null pointers as a signal, receiver, or slot act as wildcards, meaning all of them.
*/

/*!
 \fn void Object::disconnect()
 Disconnects all connections with this object as the sender.
*/

/*!
 \fn void Object::disconnect(T_PtrToReceiverOrSignal pReceiverOrSignal)
 If \a pReceiverOrSignal points to a signal on this object, everything connected to the signal is disconnected.
 Otherwise, it disconnects all connections having this object as the sender and \a pReceiverOrSignal as the receiver.
*/

/*!
 \fn T Object::tryCast()
 Returns a pointer along the inheritance hierarchy of this object if T belongs to the hierarchy. Otherwise, returns null.
*/

/*!
 \fn T Object::tryCast() const
 Returns a pointer along the inheritance hierarchy of this object if T belongs to the hierarchy. Otherwise, returns null.
*/

void Object::addEmitter(Object &emitter)
{
    ++(m_pConnectedEmitters.emplace(&emitter, 0).first->second);
}

void Object::removeEmitter(Object &emitter)
{
    auto it = m_pConnectedEmitters.find(&emitter);
    if (it != m_pConnectedEmitters.end() && ((--(it->second)) == 0))
        m_pConnectedEmitters.erase(it);
}

}
