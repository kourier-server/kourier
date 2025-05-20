//
// Copyright (C) 2024 Glauco Pacheco <glauco@kourier.io>
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

#include <Kourier>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QtLogging>
#include <QSemaphore>
#include <csignal>

using Kourier::HttpServer;
using Kourier::HttpRequest;
using Kourier::HttpBroker;
using Kourier::TlsConfiguration;
using Kourier::UnixSignalListener;

static const bool blockedSignals = [](){UnixSignalListener::blockSignalProcessingForCurrentThread(); return true;}();


int main(int argc, char ** argv)
{
    QCoreApplication app(argc, argv);
    QCommandLineParser cmdLineParser;
    cmdLineParser.addHelpOption();
    cmdLineParser.addOption({"a", "Makes server listen on <ip>.", "ip"});
    cmdLineParser.addOption({"p", "Makes server listen on <port>.", "port"});
    cmdLineParser.addOption({"worker-count", "Makes server use <N> workers. The default value is zero, which makes server create as many workers as available cores.", "N", "0"});
    cmdLineParser.addOption({"request-timeout", "Server responds with HTTP 408 Request Timeout and closes connection if requests are not fully received in <interval> seconds. The default value of 0 disables request timeout.", "interval", "0"});
    cmdLineParser.addOption({"idle-timeout", "Server responds with HTTP 408 Request Timeout and closes connection if connection stays idle for <interval> seconds. The default value of 0 disables idle timeout", "interval", "0"});
    cmdLineParser.addOption({"enable-tls", "Server enables TLS if this option is set. This option does not accept any value."});
    cmdLineParser.process(app);
    const QHostAddress address(cmdLineParser.value("a"));
    if (address.isNull())
        cmdLineParser.showHelp(1);
    bool conversionSucceeded = false;
    const uint16_t port = cmdLineParser.value("p").toUShort(&conversionSucceeded);
    if (!conversionSucceeded)
        cmdLineParser.showHelp(1);
    const int64_t workerCount = cmdLineParser.value("worker-count").toLongLong(&conversionSucceeded);
    if (!conversionSucceeded)
        cmdLineParser.showHelp(1);
    const int64_t requestTimeoutInSecs = cmdLineParser.value("request-timeout").toLongLong(&conversionSucceeded);
    if (!conversionSucceeded)
        cmdLineParser.showHelp(1);
    const int64_t idleTimeoutInSecs = cmdLineParser.value("idle-timeout").toLongLong(&conversionSucceeded);
    if (!conversionSucceeded)
        cmdLineParser.showHelp(1);
    const bool enableTls = cmdLineParser.isSet("enable-tls");
    HttpServer server;
    if (!server.setServerOption(HttpServer::ServerOption::WorkerCount, workerCount))
        qFatal("Failed to set worker count. %s", !server.errorMessage().empty() ? server.errorMessage().data() : "");
    if (!server.setServerOption(HttpServer::ServerOption::RequestTimeoutInSecs, requestTimeoutInSecs))
        qFatal("Failed to set request timeout. %s", !server.errorMessage().empty() ? server.errorMessage().data() : "");
    if (!server.setServerOption(HttpServer::ServerOption::IdleTimeoutInSecs, idleTimeoutInSecs))
        qFatal("Failed to set idle timeout. %s", !server.errorMessage().empty() ? server.errorMessage().data() : "");
    if (!server.addRoute(HttpRequest::Method::GET, "/hello", [](const HttpRequest &, HttpBroker &broker){broker.writeResponse("Hello World!");}))
        qFatal("Failed to add /hello route to server. %s", !server.errorMessage().empty() ? server.errorMessage().data() : "");
    if (enableTls)
    {
        TlsConfiguration tlsConfiguration;
        tlsConfiguration.setTlsVersion(TlsConfiguration::TlsVersion::TLS_1_2);
        tlsConfiguration.setCiphers({TlsConfiguration::Cipher::TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256});
        tlsConfiguration.addCaCertificate("/Kourier/certs/ca.crt");
        tlsConfiguration.setCertificateKeyPair("/Kourier/certs/cert.crt", "/Kourier/certs/cert.key");
        server.setTlsConfiguration(tlsConfiguration);
    }
    QObject::connect(&server, &HttpServer::started, [&](){qInfo("Server started.");});
    QObject::connect(&server, &HttpServer::failed, [&](){qFatal("Server failed to start. %s", !server.errorMessage().empty() ? server.errorMessage().data() : "");});
    if (!blockedSignals)
        qFatal("Failed to block signals.");
    UnixSignalListener unixSignalListener({SIGTERM, SIGINT});
    QObject::connect(&unixSignalListener, &UnixSignalListener::receivedSignal, &server, [&server]()
    {
        qInfo("Received signal. Stopping server.");
        server.stop();
    });
    QObject::connect(&server, &HttpServer::stopped, []()
    {
        qInfo("Server stopped. Exiting.");
        QCoreApplication::exit();
    });
    server.start(address, port);
    return app.exec();
}
