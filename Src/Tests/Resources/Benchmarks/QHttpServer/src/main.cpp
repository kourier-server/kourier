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

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QtLogging>
#include <QHostAddress>
#include <QTcpServer>
#include <QSslServer>
#include <QSslConfiguration>
#include <QSslCipher>
#include <QSslCertificate>
#include <QFile>
#include <QSslKey>
#include <QHttpServer>


int main(int argc, char ** argv)
{
    QCoreApplication app(argc, argv);
    QCommandLineParser cmdLineParser;
    cmdLineParser.addHelpOption();
    cmdLineParser.addOption({"a", "Tests server listening on <ip>.", "ip"});
    cmdLineParser.addOption({"p", "Tests server listening on <port>.", "port"});
    cmdLineParser.addOption({"enable-tls", "Server enables TLS if this option is set. This option does not accept any value."});
    cmdLineParser.process(app);
    const QHostAddress address(cmdLineParser.value("a"));
    if (address.isNull())
        cmdLineParser.showHelp(1);
    bool conversionSucceeded = false;
    const uint16_t port = cmdLineParser.value("p").toUShort(&conversionSucceeded);
    if (!conversionSucceeded)
        cmdLineParser.showHelp(1);
    const bool enableTls = cmdLineParser.isSet("enable-tls");
    QHttpServer server;
    server.setMissingHandler(&server, [](const QHttpServerRequest &, QHttpServerResponder &responder) -> void
    {
        responder.write("Hello World!", QHttpHeaders{});
    });
    QTcpServer *pTcpServer = nullptr;
    if (enableTls)
    {
        qInfo("Enabling TLS.");
        QSslConfiguration tlsConfiguration;
        tlsConfiguration.setProtocol(QSsl::TlsV1_2);
        QSslCipher cipher("ECDHE-ECDSA-AES128-GCM-SHA256");
        if (cipher.isNull())
            qFatal("Failed to set cipher.");
        tlsConfiguration.setCiphers(QList<QSslCipher>() << cipher);
        const auto caCerts = QSslCertificate::fromPath("/QHttpServer/certs/ca.crt");
        if (caCerts.isEmpty())
            qFatal("Failed to read ca certificate.");
        tlsConfiguration.setCaCertificates(caCerts);
        const auto certs = QSslCertificate::fromPath("/QHttpServer/certs/cert.crt");
        if (certs.isEmpty())
            qFatal("Failed to read certificate.");
        tlsConfiguration.setLocalCertificateChain(certs);
        QFile privateKeyFile("/QHttpServer/certs/cert.key");
        if (!privateKeyFile.open(QIODevice::ReadOnly))
            qFatal("Failed to open private key file.");
        const auto privateKeyContents = privateKeyFile.readAll();
        if (privateKeyContents.isEmpty())
            qFatal("Failed to read private key contents.");
        QSslKey privateKey(privateKeyContents, QSsl::Ec);
        if (privateKey.isNull())
            qFatal("Failed to read private key.");
        tlsConfiguration.setPrivateKey(privateKey);
        auto *pSslServer = new QSslServer;
        pSslServer->setSslConfiguration(tlsConfiguration);
        pSslServer->setHandshakeTimeout(60000);
        pTcpServer = pSslServer;
    }
    else
        pTcpServer = new QTcpServer;
    if (!pTcpServer->listen(address, port) || !server.bind(pTcpServer))
        qFatal("%s%s%s%d%s%s%s", "Failed to listen to ", qUtf8Printable(address.toString()), ":", port, ".", qUtf8Printable(pTcpServer->errorString()), ".");
    return app.exec();
}
