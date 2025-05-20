#
# Copyright (C) 2024 Glauco Pacheco <glauco@kourier.io>
# All rights reserved.
#
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    qt_add_library(KourierHttpServer STATIC
        ErrorHandler.h
        HttpBroker.cpp
        HttpBroker.h
        HttpBrokerPrivate.cpp
        HttpBrokerPrivate.h
        HttpChunkMetadataParser.cpp
        HttpChunkMetadataParser.h
        HttpConnectionHandler.cpp
        HttpConnectionHandler.h
        HttpConnectionHandlerFactory.cpp
        HttpConnectionHandlerFactory.h
        HttpFieldBlock.cpp
        HttpFieldBlock.h
        HttpRequest.cpp
        HttpRequest.h
        HttpRequestBody.h
        HttpRequestLimits.h
        HttpRequestLine.h
        HttpRequestParser.cpp
        HttpRequestParser.h
        HttpRequestPrivate.h
        HttpRequestRouter.cpp
        HttpRequestRouter.h
        HttpServer.cpp
        HttpServer.h
        HttpServerOptions.cpp
        HttpServerOptions.h
        HttpServerPrivate.cpp
        HttpServerPrivate.h
        HttpServerWorker.h
        HttpServerWorkerFactory.cpp
        HttpServerWorkerFactory.h)
    find_package(Qt6 COMPONENTS Core Concurrent Network REQUIRED)
    target_link_libraries(KourierHttpServer PRIVATE
        Qt::Core
        Qt::Concurrent
        Qt::Network)
    target_precompile_headers(KourierHttpServer PRIVATE
        <QtCore>
        <QtConcurrent>
        <QtNetwork>)
endif()
