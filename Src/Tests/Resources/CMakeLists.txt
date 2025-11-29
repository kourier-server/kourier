#
# Copyright (C) 2024 Glauco Pacheco <glauco@kourier.io>
# All rights reserved.
#
qt_add_library(TestResources STATIC
    TcpServer.cpp
    TcpServer.h
    TlsServer.cpp
    TlsServer.h
    TlsTestCertificates.cpp
    TlsTestCertificates.h)

find_package(Qt6 COMPONENTS Core Network REQUIRED)
target_link_libraries(TestResources PRIVATE Qt::Core Qt::Network)
target_precompile_headers(TestResources PRIVATE <QtCore> <QtNetwork>)
