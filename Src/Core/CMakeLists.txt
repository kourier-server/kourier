#
# Copyright (C) 2024 Glauco Pacheco <glaucopacheco@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause
#
qt_add_library(KourierCore OBJECT
    AsyncQObject.h
    MetaTypeSystem.cpp
    MetaTypeSystem.h
    NoDestroy.h
    Object.cpp
    Object.h
    SDK.h)
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(KourierCore PRIVATE
        ClockTicker.cpp
        ClockTicker.h
        EpollEventNotifier.cpp
        EpollEventNotifier.h
        EpollEventSource.h
        EpollObjectDeleter.cpp
        EpollObjectDeleter.h
        EpollReadyEventSourceRegistrar.cpp
        EpollReadyEventSourceRegistrar.h
        EpollTimerRegistrar.cpp
        EpollTimerRegistrar.h
        HostAddressFetcher.cpp
        HostAddressFetcher.h
        IOChannel.cpp
        IOChannel.h
        RingBuffer.cpp
        RingBuffer.h
        RingBufferBIO.cpp
        RingBufferBIO.h
        RuntimeError.cpp
        RuntimeError.h
        SimdIterator.cpp
        SimdIterator.h
        TcpSocket.cpp
        TcpSocket.h
        TcpSocketDataSink.cpp
        TcpSocketDataSink.h
        TcpSocketDataSource.cpp
        TcpSocketDataSource.h
        TcpSocketPrivate_epoll.cpp
        TcpSocketPrivate_epoll.h
        Timer.h
        Timer_epoll.cpp
        TimerList.cpp
        TimerList.h
        TimerListIterator.h
        TimerListNode.h
        TimerPrivate_epoll.cpp
        TimerPrivate_epoll.h
        TimerWheel.cpp
        TimerWheel.h
        TimerNotifier.cpp
        TimerNotifier.h
        TlsConfiguration.cpp
        TlsConfiguration.h
        TlsContext.cpp
        TlsContext.h
        TlsError.cpp
        TlsError.h
        TlsSocket.cpp
        TlsSocket.h
        TlsSocketDataSink.cpp
        TlsSocketDataSink.h
        TlsSocketDataSource.cpp
        TlsSocketDataSource.h
        TlsSocketPrivate_epoll.cpp
        TlsSocketPrivate_epoll.h
        UnixSignalListener.cpp
        UnixSignalListener.h
        UnixUtils.cpp
        UnixUtils.h)
    find_package(OpenSSL REQUIRED)
    target_link_libraries(KourierCore PRIVATE OpenSSL::SSL)
else()
    target_sources(KourierCore PRIVATE)
endif()
target_compile_definitions(KourierCore PUBLIC KOURIER_LIBRARY)
find_package(Qt6 COMPONENTS Core Concurrent Network REQUIRED)
target_link_libraries(KourierCore PRIVATE
    Qt::Core
    Qt::Concurrent
    Qt::Network)
target_precompile_headers(KourierCore PRIVATE
    <QtCore>
    <QtConcurrent>
    <QtNetwork>)
