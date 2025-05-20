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

#include "TcpSocket.h"

namespace Kourier
{

/*!
\class Kourier::TcpSocket
\brief The TcpSocket class represents a TCP socket.

TcpSocket is a subclass of IOChannel and extends it to connection-oriented channels. You can use connected
TcpSockets as channels for exchanging stream-oriented data with the connected peer.

You can call [connect](@ref Kourier::TcpSocket::connect) to start connecting to a peer.
TcpSocket emits [connected](@ref Kourier::TcpSocket::connected) when it successfully establishes a connection.
Otherwise, if TcpSocket fails to connect to the peer, it emits the [error](@ref Kourier::TcpSocket::error) signal.
TcpSocket waits for 60 seconds for a connection to be established before aborting.
You can call [errorMessage](@ref Kourier::TcpSocket::errorMessage) to fetch the last error that occurred.
Before calling [connect](@ref Kourier::TcpSocket::connect) to start a connection, you can call
[setBindAddressAndPort](@ref Kourier::TcpSocket::setBindAddressAndPort) to specify an address and, optionally, a port
that TcpSocket should bind to before connecting to the peer.

You can call [write](@ref Kourier::TcpSocket::write(const char *pData, size_t maxSize)) to write data to a connected TcpSocket.
TcpSocket emits the [sentData](@ref Kourier::TcpSocket::sentData) signal when it writes data to the channel. You can call
[dataToWrite](@ref Kourier::TcpSocket::dataToWrite) to know how much data is still pending to be written to the channel.

TcpSocket emits [receivedData](@ref Kourier::TcpSocket::receivedData) when it reads data from the channel. You can call
[read](@ref Kourier::TcpSocket::read) to read data from TcpSocket and [dataAvailable](@ref Kourier::TcpSocket::dataAvailable)
to know how much data has been read from the channel and is available for reading.

A connected TcpSocket emits [disconnected](@ref Kourier::TcpSocket::disconnected) when the connection finishes. You can
start a connection anytime by calling [connect](@ref Kourier::TcpSocket::connect), even on slots connected to the
[error](@ref Kourier::TcpSocket::error) signal.

You can call [disconnectFromPeer](@ref Kourier::TcpSocket::disconnectFromPeer) on a connected TcpSocket to start disconnecting from the peer.
TcpSockets always perform a graceful shutdown when disconnecting by first writing all pending data to the channel, then disabling
further send operations (by calling shutdown with *SHUT_WR*) and waiting for the peer to close the connection. TcpSocket only emits
[disconnected](@ref Kourier::TcpSocket::disconnected) for graceful shutdowns. If any error occurs while disconnecting, TcpSocket emits
the [error](@ref Kourier::TcpSocket::error) signal. TcpSocket waits 10 seconds for a graceful shutdown before aborting the connection.

TcpSocket integrates epoll into Qt's event system and uses it to detect when TcpSocket is available for IO operations. Kourier can
handle millions of sockets even on modest machines, as TcpSocket is very lightweight memory-wise.
*/

/*!
 \enum TcpSocket::State
 \brief Connection state of TcpSocket
 \var TcpSocket::State::Unconnected
 \brief The socket is not connected. You can call [connect](@ref Kourier::TcpSocket::connect) to initiate a connection.
 \var TcpSocket::State::Connecting
 \brief The socket is connecting. TcpSocket emits the [connected](@ref Kourier::TcpSocket::connected) signal when the connection is established. Otherwise, TcpSocket emits the [error](@ref Kourier::TcpSocket::error) signal if it fails to connect to the peer.
 \var TcpSocket::State::Connected
 \brief The socket is connected to the peer. You can call [peerAddress](@ref Kourier::TcpSocket::peerAddress) and [peerPort](@ref Kourier::TcpSocket::peerPort) to fetch the connected peer's address and port.
 \var TcpSocket::State::Disconnecting
 \brief The socket is disconnecting. TcpSocket emits the [disconnected](@ref Kourier::TcpSocket::disconnected) signal when the connection terminates.
*/

/*!
 \enum TcpSocket::SocketOption
 \brief Specifies the socket options that can be set and retrived.
 \var TcpSocket::SocketOption::LowDelay
 \brief Enables/Disables TCP_NODELAY.
 \var TcpSocket::SocketOption::KeepAlive
 \brief Enables/Disables SO_KEEPALIVE.
 \var TcpSocket::SocketOption::SendBufferSize
 \brief Sets/Retrieves SO_SNDBUF.
 \var TcpSocket::SocketOption::ReceiveBufferSize
 \brief Sets/Retrieves SO_RCVBUF.
*/

/*!
 \fn TcpSocket::TcpSocket()
 Creates a TcpSocket. The socket is created in the \link TcpSocket::State::Unconnected Unconnected\endlink state.
 You can call \link TcpSocket::connect connect\endlink to connect to a peer.
*/

/*!
 \fn TcpSocket::TcpSocket(int64_t socketDescriptor)
 Creates a connected TcpSocket with \a socketDescriptor. TcpSocket aborts and closes the given descriptor if it does
 not represent a connected socket. You can call [state](@ref Kourier::TcpSocket::state) to check if TcpSocket is in the
 \a Connected state.

 Because TcpSocket takes ownership of the given \a socketDescriptor, disregarding whether the connection succeeded,
 you should not close the given descriptor.
*/

/*!
 \fn TcpSocket::~TcpSocket
 Destroys the object and aborts the connection if TcpSocket is not in the \link TcpSocket::State::Unconnected Unconnected\endlink state.
*/

/*!
 \fn TcpSocket::setBindAddressAndPort(std::string_view address, uint16_t port = 0)
 Sets the bind address and port that TcpSocket must bind to before connecting to the peer.
*/

/*!
 \fn TcpSocket::connect(std::string_view host, uint16_t port)
 Tries to connect to the peer at the given \a host and \a port. The host can be an IP address or a hostname,
 which TcpSocket will resolve before connecting. If TcpSocket is not
 in the [Unconnected](@ref Kourier::TcpSocket::State::Unconnected) state, it aborts the previous connection before initiating the new one.
 TcpSocket emits the [connected](@ref Kourier::TcpSocket::connected) signal when the connection is successfully established.
 Otherwise, TcpSocket emits the [error](@ref Kourier::TcpSocket::error) signal if an error occurs while trying to connect to the peer.
 You can call [errorMessage](@ref Kourier::TcpSocket::errorMessage) to retrieve the specific error information. TcpSocket waits for
 60 seconds for a connection to be established before aborting.
*/

/*!
 \fn TcpSocket::disconnectFromPeer()
 If TcpSocket is connected, it starts disconnecting from the peer and enters the
 [Disconnecting](@ref Kourier::TcpSocket::State::Disconnecting) state. In this state, TcpSocket ignores further write operations, but
 TcpSocket writes all data in the write buffer to the channel before disconnecting from the peer.

 After writing all pending data in the write buffer to the channel, TcpSocket performs an orderly shutdown
 by disabling further send operations (by calling shutdown with *SHUT_WR*) and waiting for the peer to close
 the connection. TcpSocket emits the [disconnected](@ref Kourier::TcpSocket::disconnected) signal when the peer closes
 the connection, and the disconnection is complete.
 TcpSocket waits up to 10 seconds for the peer to close the connection before aborting.
 You can call [abort](@ref Kourier::TcpSocket::abort) to disconnect immediately.
*/

/*!
 \fn TcpSocket::abort()
 Aborts the connection immediately and enters the Unconnected state. Any pending data in the write buffer
 is discarded and not sent to the peer. TcpSocket does not emit the [disconnected](@ref Kourier::TcpSocket::disconnected)
 signal when the connection is aborted.
*/

/*!
 \fn TcpSocket::errorMessage()
 Returns the message for the last error that occurred.
*/

/*!
 \fn TcpSocket::localAddress()
 Returns the local address of the connected socket.
*/

/*!
 \fn TcpSocket::localPort()
 Returns the local port of the connected socket.
*/

/*!
 \fn TcpSocket::peerName()
 Returns the name of the connected peer.
*/

/*!
 \fn TcpSocket::peerAddress()
 Returns the address of the connected peer.
*/

/*!
 \fn TcpSocket::peerPort()
 Returns the port of the connected peer.
*/

/*!
 \fn TcpSocket::readBufferCapacity()
 Returns the read buffer capacity. A value of zero means that capacity is not limited. If the returned value is positive,
 the read buffer can grow up to the returned value.
*/

/*!
 \fn TcpSocket::setReadBufferCapacity(size_t capacity)
 Sets the read buffer capacity. A value of zero means that the capacity is not limited. The read buffer can grow to the given value
 if the \a capacity is positive.

 Returns true if the capacity was successfully changed. Setting a capacity can fail because this
 method does not delete data in the buffer. Thus, it is impossible to set a capacity to a value smaller than the data available
 in the read buffer.
*/

/*!
 \fn TcpSocket::state()
 Returns the \link TcpSocket::State state\endlink of the TcpSocket.
*/

/*!
 \fn TcpSocket::getSocketOption(SocketOption option)
 Returns the value for the given \link TcpSocket::SocketOption option\endlink.
*/

/*!
 \fn TcpSocket::setSocketOption(SocketOption option, int value)
 Sets \a value for the given \link TcpSocket::SocketOption option\endlink.
*/

/*!
 \fn TcpSocket::connected()
 TcpSocket emits the [connected](@ref Kourier::TcpSocket::connected) signal when it successfully connects to the peer.
*/

/*!
 \fn TcpSocket::disconnected()
 A connected TcpSocket emits the [disconnected](@ref Kourier::TcpSocket::disconnected) signal when it finishes disconnecting from the peer.
*/

/*!
 \fn TcpSocket::error()
 TcpSocket emits this signal when an error occurs. You can call [errorMessage](@ref Kourier::TcpSocket::errorMessage)
 to fetch the last error that occurred.
*/

}
