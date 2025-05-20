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
#include <QTcpSocket>
#include <vector>
#include <utility>

// pchar          = unreserved / pct-encoded / sub-delims / ":" / "@"
// unreserved     = ALPHA / DIGIT / "-" / "." / "_" / "~"
// pct-encoded    = "%" HEXDIG HEXDIG
// sub-delims     = "!" / "$" / "&" / "'" / "(" / ")" / "*" / "+" / "," / ";" / "="
const static auto invalidCharsInUrlAbsolutePath = []() -> std::string
{
    std::string temp;
    temp.reserve(256);
    for (int16_t ascii = std::numeric_limits<char>::min(); ascii <= std::numeric_limits<char>::max(); ++ascii)
    {
        char ch = static_cast<char>(ascii);
        if (('a' <= ch && ch <= 'z')
            || ('A' <= ch && ch <= 'Z')
            || ('0' <= ch && ch <= '9')
            || ch == '-' || ch == '.' || ch == '_' || ch == '~'
            || ch == '!' || ch == '$' || ch == '&' || ch == '\''
            || ch == '(' || ch == ')' || ch == '*' || ch == '+'
            || ch == ',' || ch == ';' || ch == '=' || ch == ':' || ch == '@'
            || ch == '/' || ch == '?')
            continue;
        else
            temp.push_back(ch);
    }
    return temp;
}();

// query          = *( pchar / "/" / "?" )
// pchar          = unreserved / pct-encoded / sub-delims / ":" / "@"
// unreserved     = ALPHA / DIGIT / "-" / "." / "_" / "~"
// pct-encoded    = "%" HEXDIG HEXDIG
// sub-delims     = "!" / "$" / "&" / "'" / "(" / ")" / "*" / "+" / "," / ";" / "="
const static auto invalidCharsInUrlQuery = []() -> std::string
{
    std::string temp;
    temp.reserve(256);
    for (int16_t ascii = std::numeric_limits<char>::min(); ascii <= std::numeric_limits<char>::max(); ++ascii)
    {
        char ch = static_cast<char>(ascii);
        if (('a' <= ch && ch <= 'z')
            || ('A' <= ch && ch <= 'Z')
            || ('0' <= ch && ch <= '9')
            || ch == '-' || ch == '.' || ch == '_' || ch == '~'
            || ch == '!' || ch == '$' || ch == '&' || ch == '\''
            || ch == '(' || ch == ')' || ch == '*' || ch == '+'
            || ch == ',' || ch == ';' || ch == '=' || ch == ':' || ch == '@'
            || ch == '/' || ch == '?')
            continue;
        else
            temp.push_back(ch);
    }
    return temp;
}();

// field-name     = token (RFC9110, section 5.1)
// token          = 1*tchar
// tchar          = "!" / "#" / "$" / "%" / "&" / "'" / "*"
//                  "+" / "-" / "." / "^" / "_" / "`" / "|" / "~" / ":"
//                  DIGIT / ALPHA
const static auto invalidCharsInFieldName = []() -> std::string
{
    std::string temp;
    temp.reserve(256);
    for (int16_t ascii = std::numeric_limits<char>::min(); ascii <= std::numeric_limits<char>::max(); ++ascii)
    {
        char ch = static_cast<char>(ascii);
        if (('a' <= ch && ch <= 'z')
            || ('A' <= ch && ch <= 'Z')
            || ('0' <= ch && ch <= '9')
            || ch == '!' || ch == '#' || ch == '$' || ch == '%' || ch == '&'
            || ch == '\'' || ch == '*' || ch == '+' || ch == '-' || ch == '.'
            || ch == '^' || ch == '_' || ch == '`' || ch == '|' || ch == '~' || ch == ':')
            continue;
        else
            temp.push_back(ch);
    }
    return temp;
}();

// field-value    = *field-content (RFC9110, section 5.5)
// field-content  = field-vchar[ 1*( SP / HTAB / field-vchar ) field-vchar ] (RFC9110, section 5.5)
// field-vchar    = VCHAR / obs-text (RFC9110, section 5.5)
// obs-text       = %x80-FF (RFC9110, section 5.5)
const static auto invalidCharsInFieldValue = []() -> std::string
{
    std::string temp;
    temp.reserve(256);
    for (int16_t ascii = std::numeric_limits<char>::min(); ascii <= std::numeric_limits<char>::max(); ++ascii)
    {
        if ((0 <= ascii && ascii < 32 && ascii != 9 && ascii != 13 && ascii != 10) || ascii == 127)
            temp.push_back(static_cast<char>(ascii));
    }
    return temp;
}();

enum class InvalidCharType {AbsolutePath, Query, FieldName, FieldValue};
const static auto invalidChars = []() -> std::vector<std::pair<InvalidCharType, char>>
{
    std::vector<std::pair<InvalidCharType, char>> tmp;
    tmp.reserve(invalidCharsInUrlAbsolutePath.size() + invalidCharsInUrlQuery.size()
                + invalidCharsInFieldName.size() + invalidCharsInFieldValue.size());
    for (const auto &ch : invalidCharsInUrlAbsolutePath)
        tmp.push_back({InvalidCharType::AbsolutePath, ch});
    for (const auto &ch : invalidCharsInUrlQuery)
        tmp.push_back({InvalidCharType::Query, ch});
    for (const auto &ch : invalidCharsInFieldName)
        tmp.push_back({InvalidCharType::FieldName, ch});
    for (const auto &ch : invalidCharsInFieldValue)
        tmp.push_back({InvalidCharType::FieldValue, ch});
    if (tmp.empty())
        qFatal("Failed to fetch invalid characters for testing HTTP compliance.");
    return tmp;
}();

static const auto invalidPctEncodedDigits = []() -> std::vector<std::pair<char,char>>
{
    std::string invalidHexDigits;
    invalidHexDigits.reserve(256);
    for (int16_t ascii = std::numeric_limits<char>::min(); ascii <= std::numeric_limits<char>::max(); ++ascii)
    {
        if ((48 <= ascii && ascii <= 57) || (65 <= ascii && ascii <= 90) || (97 <= ascii && ascii <= 122))
            continue;
        else
            invalidHexDigits.push_back(static_cast<char>(ascii));
    }
    std::vector<std::pair<char, char>> invalidPctEncodedDigits;
    invalidPctEncodedDigits.reserve(2*256*invalidHexDigits.size());
    for (const auto &invalidHexDigit : invalidHexDigits)
    {
        for (int16_t ascii = std::numeric_limits<char>::min(); ascii <= std::numeric_limits<char>::max(); ++ascii)
            invalidPctEncodedDigits.push_back({invalidHexDigit, static_cast<char>(ascii)});
    }
    for (int16_t ascii = std::numeric_limits<char>::min(); ascii <= std::numeric_limits<char>::max(); ++ascii)
    {
        if ((48 <= ascii && ascii <= 57) || (65 <= ascii && ascii <= 90) || (97 <= ascii && ascii <= 122))
        {
            const char validCh = static_cast<char>(ascii);
            for (const auto &invalidHexDigit : invalidHexDigits)
                    invalidPctEncodedDigits.push_back({validCh, invalidHexDigit});
        }
    }
    return invalidPctEncodedDigits;
}();


int main(int argc, char ** argv)
{
    QCoreApplication app(argc, argv);
    QCommandLineParser cmdLineParser;
    cmdLineParser.addHelpOption();
    cmdLineParser.addOption({"a", "Tests server listening on <ip>.", "ip"});
    cmdLineParser.addOption({"p", "Tests server listening on <port>.", "port"});
    cmdLineParser.process(app);
    const QHostAddress address(cmdLineParser.value("a"));
    if (address.isNull())
        cmdLineParser.showHelp(1);
    bool conversionSucceeded = false;
    const uint16_t port = cmdLineParser.value("p").toUShort(&conversionSucceeded);
    if (!conversionSucceeded)
        cmdLineParser.showHelp(1);
    size_t detectedInvalidUrlAbsoluteRequestsCounter = 0;
    size_t undetectedInvalidUrlAbsoluteRequestsCounter = 0;
    size_t detectedInvalidUrlQueryRequestsCounter = 0;
    size_t undetectedInvalidUrlQueryRequestsCounter = 0;
    size_t detectedInvalidFieldNameRequestsCounter = 0;
    size_t undetectedInvalidFieldNameRequestsCounter = 0;
    size_t detectedInvalidFieldValueRequestsCounter = 0;
    size_t undetectedInvalidFieldValueRequestsCounter = 0;
    size_t detectedInvalidPctEncodedDigitInUrlAbsoluteRequestsCounter = 0;
    size_t undetectedInvalidPctEncodedDigitInUrlAbsoluteRequestsCounter = 0;
    size_t detectedInvalidPctEncodedDigitInUrlQueryRequestsCounter = 0;
    size_t undetectedInvalidPctEncodedDigitInUrlQueryRequestsCounter = 0;
    const QByteArray host = address.toString().toUtf8().append(':').append(QByteArray::number(port));
    qInfo("Testing HTTP compliance...");
    QTcpSocket socket;
    static constinit size_t invalidCharIndex = 0;
    QTcpSocket hexDigitInAbsolutePathSocket;
    size_t hexDigitInAbsoluteCurrentIndex = 0;
    QTcpSocket hexDigitInQuerySocket;
    size_t hexDigitInQueryCurrentIndex = 0;
    auto fcnPrintResultsAndExitIfPossible = [&]()
    {
        if (invalidCharIndex == invalidChars.size()
            && hexDigitInAbsoluteCurrentIndex == invalidPctEncodedDigits.size()
            && hexDigitInQueryCurrentIndex == invalidPctEncodedDigits.size())
        {
            qInfo("Finished testing HTTP compliance.");
            qInfo("%10zu%s", detectedInvalidUrlAbsoluteRequestsCounter, " requests with invalid URL absolute path detected.");
            qInfo("%10zu%s", undetectedInvalidUrlAbsoluteRequestsCounter, " requests with invalid URL absolute path undetected.");
            qInfo("%10zu%s", detectedInvalidUrlQueryRequestsCounter, " requests with invalid URL query detected.");
            qInfo("%10zu%s", undetectedInvalidUrlQueryRequestsCounter, " requests with invalid URL query undetected.");
            qInfo("%10zu%s", detectedInvalidFieldNameRequestsCounter, " requests with invalid header name detected.");
            qInfo("%10zu%s", undetectedInvalidFieldNameRequestsCounter, " requests with invalid header name undetected.");
            qInfo("%10zu%s", detectedInvalidFieldValueRequestsCounter, " requests with invalid header value detected.");
            qInfo("%10zu%s", undetectedInvalidFieldValueRequestsCounter, " requests with invalid header value undetected.");
            qInfo("%10zu%s", detectedInvalidPctEncodedDigitInUrlAbsoluteRequestsCounter, " requests with invalid pct-encoded hex digits in URL absolute path detected.");
            qInfo("%10zu%s", undetectedInvalidPctEncodedDigitInUrlAbsoluteRequestsCounter, " requests with invalid pct-encoded hex digits URL absolute path undetected.");
            qInfo("%10zu%s", detectedInvalidPctEncodedDigitInUrlQueryRequestsCounter, " requests with invalid pct-encoded hex digits URL query detected.");
            qInfo("%10zu%s", undetectedInvalidPctEncodedDigitInUrlQueryRequestsCounter, " requests with invalid pct-encoded hex digits URL query undetected.");
            QCoreApplication::quit();
        }
    };
    QObject::connect(&socket, &QTcpSocket::connected, [&]()
    {
        QByteArray request("GET /hello");
        switch (invalidChars[invalidCharIndex].first)
        {
            case InvalidCharType::AbsolutePath:
                request.append(invalidChars[invalidCharIndex].second).append(" HTTP/1.1\r\nHost: ").append(host).append("\r\n\r\n");
                break;
            case InvalidCharType::Query:
                request.append('?').append(invalidChars[invalidCharIndex].second).append(" HTTP/1.1\r\nHost: ").append(host).append("\r\n\r\n");
                break;
            case InvalidCharType::FieldName:
                request.append(" HTTP/1.1\r\nHost: ").append(host).append("\r\n").append(invalidChars[invalidCharIndex].second).append(": value\r\n\r\n");
                break;
            case InvalidCharType::FieldValue:
                request.append(" HTTP/1.1\r\nHost: ").append(host).append("\r\n").append("name: ").append(invalidChars[invalidCharIndex].second).append("\r\n\r\n");
                break;
        }
        socket.write(request);
    });
    QObject::connect(&socket, &QTcpSocket::readyRead, [&]()
    {
        if (socket.bytesAvailable() >= 15)
            socket.disconnectFromHost();
    });
    QObject::connect(&socket, &QTcpSocket::disconnected, [&]()
    {
        const auto response = socket.readAll();
        const bool undetected = response.startsWith("HTTP/1.1 200 OK");
        switch (invalidChars[invalidCharIndex].first)
        {
            case InvalidCharType::AbsolutePath:
                detectedInvalidUrlAbsoluteRequestsCounter += undetected ? 0 : 1;
                undetectedInvalidUrlAbsoluteRequestsCounter += undetected ? 1 : 0;
                break;
            case InvalidCharType::Query:
                detectedInvalidUrlQueryRequestsCounter += undetected ? 0 : 1;
                undetectedInvalidUrlQueryRequestsCounter += undetected ? 1 : 0;
                break;
            case InvalidCharType::FieldName:
                detectedInvalidFieldNameRequestsCounter += undetected ? 0 : 1;
                undetectedInvalidFieldNameRequestsCounter += undetected ? 1 : 0;
                break;
            case InvalidCharType::FieldValue:
                detectedInvalidFieldValueRequestsCounter += undetected ? 0 : 1;
                undetectedInvalidFieldValueRequestsCounter += undetected ? 1 : 0;
                break;
        }
        if (++invalidCharIndex < invalidChars.size())
            socket.connectToHost(address, port);
        else
            fcnPrintResultsAndExitIfPossible();
    });
    QObject::connect(&hexDigitInAbsolutePathSocket, &QTcpSocket::connected, [&]()
    {
        QByteArray request("GET /hello%");
        request.append(invalidPctEncodedDigits[hexDigitInAbsoluteCurrentIndex].first)
            .append(invalidPctEncodedDigits[hexDigitInAbsoluteCurrentIndex].second)
            .append(" HTTP/1.1\r\nHost: ").append(host).append("\r\n\r\n");
        hexDigitInAbsolutePathSocket.write(request);
    });
    QObject::connect(&hexDigitInAbsolutePathSocket, &QTcpSocket::readyRead, [&]()
    {
        if (hexDigitInAbsolutePathSocket.bytesAvailable() >= 15)
            hexDigitInAbsolutePathSocket.disconnectFromHost();
    });
    QObject::connect(&hexDigitInAbsolutePathSocket, &QTcpSocket::disconnected, [&]()
    {
        const auto response = hexDigitInAbsolutePathSocket.readAll();
        const bool undetected = response.startsWith("HTTP/1.1 200 OK");
        detectedInvalidPctEncodedDigitInUrlAbsoluteRequestsCounter += undetected ? 0 : 1;
        undetectedInvalidPctEncodedDigitInUrlAbsoluteRequestsCounter += undetected ? 1 : 0;
        if (++hexDigitInAbsoluteCurrentIndex < invalidPctEncodedDigits.size())
            hexDigitInAbsolutePathSocket.connectToHost(address, port);
        else
            fcnPrintResultsAndExitIfPossible();
    });
    QObject::connect(&hexDigitInQuerySocket, &QTcpSocket::connected, [&]()
    {
        QByteArray request("GET /hello?%");
        request.append(invalidPctEncodedDigits[hexDigitInQueryCurrentIndex].first)
            .append(invalidPctEncodedDigits[hexDigitInQueryCurrentIndex].second)
            .append(" HTTP/1.1\r\nHost: ").append(host).append("\r\n\r\n");
        hexDigitInQuerySocket.write(request);
    });
    QObject::connect(&hexDigitInQuerySocket, &QTcpSocket::readyRead, [&]()
    {
        if (hexDigitInQuerySocket.bytesAvailable() >= 15)
            hexDigitInQuerySocket.disconnectFromHost();
    });
    QObject::connect(&hexDigitInQuerySocket, &QTcpSocket::disconnected, [&]()
    {
        const auto response = hexDigitInQuerySocket.readAll();
        const bool undetected = response.startsWith("HTTP/1.1 200 OK");
        detectedInvalidPctEncodedDigitInUrlQueryRequestsCounter += undetected ? 0 : 1;
        undetectedInvalidPctEncodedDigitInUrlQueryRequestsCounter += undetected ? 1 : 0;
        if (++hexDigitInQueryCurrentIndex < invalidPctEncodedDigits.size())
            hexDigitInQuerySocket.connectToHost(address, port);
        else
            fcnPrintResultsAndExitIfPossible();
    });
    socket.connectToHost(address, port);
    hexDigitInAbsolutePathSocket.connectToHost(address, port);
    hexDigitInQuerySocket.connectToHost(address, port);
    return app.exec();
}
