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

#include "TlsSocket.h"

namespace Kourier
{

/*!
\class Kourier::TlsSocket
\brief The TlsSocket class represents a TLS-encrypted data exchange over a TCP socket.

TlsSocket is a subclass of TcpSocket and represents TLS-encrypted TCP sockets. You can use encrypted
TlsSockets as channels for exchanging stream-oriented data with the connected peer.

All TlsSocket constructors require a TlsConfiguration, which TlsSocket uses to set up TLS encryption. TlsSocket
configures TLS encryption in the first TLS handshake right after the TlsSocket establishes the TCP connection and emits
the [connected](@ref Kourier::TlsSocket::connected) signal.

You can call [connect](@ref Kourier::TlsSocket::connect) to start connecting to the peer. TlsSocket emits the
[connected](@ref Kourier::TlsSocket::connected) signal when it successfully establishes a TCP connection. After connecting
to the peer and emitting the [connected](@ref Kourier::TlsSocket::connected) signal, TlsSocket starts the TLS handshake.
When the TLS handshake finishes, TlsSocket emits the [encrypted](@ref Kourier::TlsSocket::encrypted) signal and can start
encrypting and decrypting data. TlsSocket emits the [error](@ref Kourier::TlsSocket::error) signal if it fails to connect
to the peer or the TLS handshake fails. TlsSocket waits 60 seconds for the connection to be established and 60 seconds for
the TLS handshake to complete before aborting. You can call [errorMessage](@ref Kourier::TlsSocket::errorMessage) to fetch
the last error that occurred in TlsSocket. Before calling [connect](@ref Kourier::TlsSocket::connect) to start a connection,
you can call [setBindAddressAndPort](@ref Kourier::TlsSocket::setBindAddressAndPort) to specify an address and, optionally, a
port that TlsSocket should bind to before connecting to the peer.

You can call [write](@ref Kourier::TlsSocket::write(const char *pData, size_t maxSize)) to write data to a connected TlsSocket.
You can start writing data after TlsSocket emits the [connected](@ref Kourier::TlsSocket::connected) signal. TlsSocket buffers
all data you write to it until the TLS handshake finishes, and TlsSocket emits the [encrypted](@ref Kourier::TlsSocket::encrypted)
signal. TlsSocket emits the [sentData](@ref Kourier::TlsSocket::sentData) signal when it writes encrypted data to the channel. You
can call [dataToWrite](@ref Kourier::TlsSocket::dataToWrite) to know how much data is still waiting to be encrypted and written to
the channel.

TlsSocket emits the [receivedData](@ref Kourier::TlsSocket::receivedData) signal when it decrypts data from the channel. You can call
[read](@ref Kourier::TlsSocket::read) to read unencrypted data from the TlsSocket and [dataAvailable](@ref Kourier::TlsSocket::dataAvailable)
to know how much data has been decrypted from the channel and is available for reading.

An encrypted TlsSocket emits the [disconnected](@ref Kourier::TlsSocket::disconnected) signal when the connection finishes.

You can start a connection anytime by calling [connect](@ref Kourier::TlsSocket::connect), even on slots connected to the
[error](@ref Kourier::TlsSocket::error) signal.

You can call [disconnectFromPeer](@ref Kourier::TlsSocket::disconnectFromPeer) on an encrypted TlsSocket to start disconnecting
from the peer. An encrypted TlsSocket always performs a graceful shutdown when disconnecting by first encrypting and writing all
pending data to the channel, then performing the TLS shutdown by sending a *close_notify* shutdown alert to the peer and waiting
for the peer's *close_notify* shutdown alert. After shutting down TLS, TlsSocket turns off further send operations (by calling
shutdown with *SHUT_WR*) and waits for the peer to close the connection. TlsSocket only emits the [disconnected](@ref Kourier::TlsSocket::disconnected)
signal for graceful shutdowns. If any error occurs while disconnecting, TlsSocket emits the [error](@ref Kourier::TlsSocket::error)
signal. TlsSocket waits 10 seconds for a graceful shutdown before aborting the connection.

TlsSocket uses custom memory BIOs to limit OpenSSL to TLS computations only, while keeping all connection-related work under
TlsSocket's control. Custom memory BIOs enable TlsSocket to provide leading performance on TLS-encrypted connections. Also,
TlsSocket integrates epoll into Qt's event system and uses it to detect when the TlsSocket is available for IO operations.
Kourier can handle millions of sockets even on modest machines, as TlsSocket is very lightweight memory-wise.
*/

/*!
 \fn TlsSocket::TlsSocket(const TlsConfiguration &tlsConfiguration)
 Creates a TlsSocket with the given \a tlsConfiguration, which TlsSocket uses to configure TLS encryption after TlsSocket
establishes the TCP connection.
The socket is created in the \link TcpSocket::State::Unconnected Unconnected\endlink
state. You can call \link TlsSocket::connect connect\endlink to connect to a peer.
*/

/*!
 \fn TlsSocket::TlsSocket(int64_t socketDescriptor, const TlsConfiguration &tlsConfiguration)
 Creates a connected TlsSocket with \a socketDescriptor and uses \a tlsConfiguration to configure TLS encryption.
 TlsSocket aborts and closes the given descriptor if it does not represent a connected socket.
 You can call [state](@ref Kourier::TlsSocket::state) to check if the TlsSocket instance is in the
 [Connected](@ref Kourier::TcpSocket::State::Connected) state.

 Because TlsSocket takes ownership of the given \a socketDescriptor, disregarding whether the connection succeeded,
 you should not close the given descriptor.
*/

/*!
 \fn TlsSocket::~TlsSocket
 Destroys the object and aborts the connection if TlsSocket is not in the \link TcpSocket::State::Unconnected Unconnected\endlink state.
*/

/*
 \fn TlsSocket::connect(std::string_view host, uint16_t port)
 Tries to connect to \a host and \a port. Given \a host can be an IP address
 or a hostname, which TlsSocket will resolve prior to connecting. If TlsSocket is not
 in the \link TcpSocket::State::Unconnected Unconnected\endlink state, it calls abort before initiating the connection.
 TlsSocket emits \link TlsSocket::connected connected\endlink() when
 the connection is successfuly established. Otherwise, TlsSocket emits
 \link TlsSocket::error error\endlink() if an error occurs while trying to
 connect to peer. You can call \link TlsSocket::errorMessage errorMessage\endlink()
 to retrieve the specific error information. TlsSocket waits for 60 seconds for connection to be established before aborting.

 After TlsSocket establishes the TCP connection and emits \link TlsSocket::connected connected\endlink(), it starts the TLS
 handshake. TlsSocket emits \link TlsSocket::encrypted encrypted\endlink() when TLS handshake completes. If TLS handshake fails
 TlsSocket emits \link TlsSocket::error error\endlink(). TlsSocket waits 60 seconds for TLS handshake to complete before aborting.

 You can start writing data to TlsSocket after it emits \link TlsSocket::connected connected\endlink(), as TlsSocket buffers all
 data written to it until TLS encryption get setup.
*/

/*
 \fn TlsSocket::disconnectFromPeer()
 If TlsSocket is encrypted, it starts disconnecting from peer and enters the \link TlsSocket::State::Disconnecting Disconnecting\endlink
 state. In this state, TlsSocket ignores further write operations, but TlsSocket writes encrypts all data in write buffer and writes it
to channel before starting disconnecting from peer.

 After encrypting and writing all pending data in write buffer to channel, TlsSocket performs an orderly shutdown
 by performing the TLS shutdown, in which TlsSocket sends a close_notify shutdown alert to the peer and waits for
 peer's close_notify shutdown alert. Then, TlsSocket disables further send operations (by calling shutdown with SHUT_WR)
 and waits for peer to close the connection. TlsSocket emits \link TlsSocket::disconnected disconnected\endlink() when peer closes
 the connection and disconnection is complete.
 TlsSocket waits up to 10 seconds for a graceful shutdown before aborting.
 You can call \link TlsSocket::abort abort\endlink() to disconnect immediatelly.
*/

/*
 \fn TlsSocket::abort()
 Aborts the connection immediatelly and enters \link TcpSocket::State::Unconnected Unconnected\endlink
 state. Any pending data in write buffer is discarded and is neither encrypted nor sent to peer.
 TlsSocket does not emit \link TlsSocket::disconnected disconnected\endlink() when connection is aborted.
*/

/*!
 \fn TlsSocket::isEncrypted()
 Returns true if TlsSocket has setup TLS encryption and can encrypt and decrypt data.
*/

/*!
 \fn TlsSocket::tlsConfiguration()
Returns the [tlsConfiguration](@ref Kourier::TlsConfiguration) given in the TlsSocket constructor, which will be
used to set up TLS encryption.
*/

/*!
 \fn TlsSocket::encrypted()
 After TlsSocket establishes the TCP connection and emits the [connected](@ref Kourier::TlsSocket::connected) signal, the TLS handshake starts.
 TlsSocket emits the [encrypted](@ref Kourier::TlsSocket::encrypted) signal when the TLS handshake is complete, and the TlsSocket
 can start encrypting and decrypting data.
 TlsSocket waits 60 seconds for the TLS handshake to complete before aborting and emits the [error](@ref Kourier::TlsSocket::error)
 signal if the TLS handshake fails.

 You can start writing data to TlsSocket after it emits the [connected](@ref Kourier::TlsSocket::connected) signal, as TlsSocket
 buffers all data written to it until it configures TLS encryption.
*/

}
