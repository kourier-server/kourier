#
# Copyright (C) 2024 Glauco Pacheco <glaucopacheco@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause
#
qt_add_library(TestResources OBJECT
    TcpServer.cpp
    TcpServer.h
    TestHostNamesFetcher.cpp
    TestHostNamesFetcher.h
    TlsServer.cpp
    TlsServer.h
    TlsTestCertificates.cpp
    TlsTestCertificates.h)

find_package(Qt6 COMPONENTS Core Network REQUIRED)
target_link_libraries(TestResources PRIVATE Kourier::Spectator Qt::Core Qt::Network)
target_precompile_headers(TestResources PRIVATE <QtCore> <QtNetwork>)
