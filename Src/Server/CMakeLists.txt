#
# Copyright (C) 2024 Glauco Pacheco <glauco@kourier.io>
# All rights reserved.
#
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    qt_add_library(KourierServer STATIC
        AsyncServerWorker.h
        ConnectionHandler.cpp
        ConnectionHandler.h
        ConnectionHandlerFactory.h
        ConnectionHandlerRepository.cpp
        ConnectionHandlerRepository.h
        ConnectionListener.cpp
        ConnectionListener.h
        ExecutionState.h
        QTcpServerBasedConnectionListener.cpp
        QTcpServerBasedConnectionListener.h
        QTcpServerBasedConnectionListenerPrivate.cpp
        QTcpServerBasedConnectionListenerPrivate.h
        Server.cpp
        Server.h
        ServerError.h
        ServerWorker.cpp
        ServerWorker.h
        ServerWorkerFactory.h)
    find_package(Qt6 COMPONENTS Core Concurrent Network REQUIRED)
    target_link_libraries(KourierServer PRIVATE
        Qt::Core
        Qt::Concurrent
        Qt::Network)
    target_precompile_headers(KourierServer PRIVATE
        <QtCore>
        <QtConcurrent>
        <QtNetwork>)
endif()
