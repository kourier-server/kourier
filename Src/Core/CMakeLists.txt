#
# Copyright (C) 2024 Glauco Pacheco <glauco@kourier.io>
# All rights reserved.
#
qt_add_library(KourierCore STATIC
    AsyncQObject.h
    MetaTypeSystem.cpp
    MetaTypeSystem.h
    NoDestroy.h
    Object.cpp
    Object.h
    SDK.h)
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(KourierCore PRIVATE
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
        TimerPrivate_epoll.cpp
        TimerPrivate_epoll.h
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
find_package(Qt6 COMPONENTS Core Concurrent Network REQUIRED)
target_link_libraries(KourierCore PRIVATE
    Qt::Core
    Qt::Concurrent
    Qt::Network)
target_precompile_headers(KourierCore PRIVATE
    <QtCore>
    <QtConcurrent>
    <QtNetwork>)
