// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/mcp/McpServer.hpp>

#include <QHostAddress>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUuid>

#include <array>
#include <utility>

namespace voelund::mcp {
namespace {
constexpr qsizetype maximumHeaderBytes = 16 * 1024;

QByteArray reason(int status) {
    switch (status) {
    case 200:
        return "OK";
    case 202:
        return "Accepted";
    case 400:
        return "Bad Request";
    case 401:
        return "Unauthorized";
    case 403:
        return "Forbidden";
    case 404:
        return "Not Found";
    case 405:
        return "Method Not Allowed";
    case 413:
        return "Payload Too Large";
    case 415:
        return "Unsupported Media Type";
    case 431:
        return "Request Header Fields Too Large";
    default:
        return "Not Implemented";
    }
}
QByteArray json(const QJsonObject& object) {
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}
QByteArray rpcError(int code, const char* message) {
    return json({{"jsonrpc", "2.0"},
                 {"id", QJsonValue()},
                 {"error", QJsonObject{{"code", code}, {"message", message}}}});
}
// Constant time for equal lengths, so the token cannot be guessed byte by byte.
bool sameSecret(const QByteArray& left, const QByteArray& right) {
    if (left.size() != right.size())
        return false;
    unsigned char difference = 0;
    for (qsizetype index = 0; index < left.size(); ++index)
        difference |= static_cast<unsigned char>(left[index] ^ right[index]);
    return difference == 0;
}
} // namespace

McpHttpServer::McpHttpServer(ToolHost& host, QByteArray token, QObject* parent)
    : QObject(parent), protocol_(host), token_(std::move(token)), server_(new QTcpServer(this)) {
    connect(server_, &QTcpServer::newConnection, this, [this] { accept(); });
}

McpHttpServer::~McpHttpServer() = default;

bool McpHttpServer::listen(quint16 port) { return server_->listen(QHostAddress::LocalHost, port); }
void McpHttpServer::close() {
    server_->close();
    for (auto* socket : buffers_.keys())
        socket->abort();
    buffers_.clear();
    sessions_.clear();
}
bool McpHttpServer::isListening() const { return server_->isListening(); }
quint16 McpHttpServer::port() const { return server_->serverPort(); }
QByteArray McpHttpServer::issueRunToken() {
    std::array<quint32, 8> random{};
    QRandomGenerator::system()->fillRange(random.data(), random.size());
    auto token = QByteArray(reinterpret_cast<const char*>(random.data()),
                            static_cast<qsizetype>(sizeof(random)))
                     .toHex();
    runTokens_.insert(token);
    return token;
}
void McpHttpServer::revokeRunToken(const QByteArray& token) { runTokens_.remove(token); }

void McpHttpServer::setToken(QByteArray token) {
    token_ = std::move(token);
    sessions_.clear();
}
QString McpHttpServer::url() const { return QStringLiteral("http://127.0.0.1:%1/mcp").arg(port()); }

void McpHttpServer::accept() {
    while (auto* socket = server_->nextPendingConnection()) {
        buffers_.insert(socket, {});
        connect(socket, &QTcpSocket::readyRead, this, [this, socket] { read(socket); });
        connect(socket, &QTcpSocket::disconnected, this, [this, socket] {
            buffers_.remove(socket);
            socket->deleteLater();
        });
    }
}

// HTTP/1.1 with Content-Length bodies; several requests may share one connection.
void McpHttpServer::read(QTcpSocket* socket) {
    auto found = buffers_.find(socket);
    if (found == buffers_.end())
        return;
    QByteArray& buffer = found.value();
    buffer += socket->readAll();
    const auto send = [socket](const Response& response, bool close) {
        QByteArray head = "HTTP/1.1 " + QByteArray::number(response.status) + ' ' +
                          reason(response.status) + "\r\n";
        if (!response.body.isEmpty())
            head += "Content-Type: application/json\r\n";
        head += "Content-Length: " + QByteArray::number(response.body.size()) + "\r\n";
        for (const auto& [name, value] : response.headers)
            head += name + ": " + value + "\r\n";
        if (close)
            head += "Connection: close\r\n";
        socket->write(head + "\r\n" + response.body);
        if (close)
            socket->disconnectFromHost();
    };
    while (true) {
        const auto end = buffer.indexOf("\r\n\r\n");
        if (end < 0) {
            if (buffer.size() > maximumHeaderBytes) {
                send({431, {}, {}}, true);
                buffer.clear();
            }
            return;
        }
        const auto lines = buffer.left(end).split('\n');
        const auto requestLine = lines.front().trimmed().split(' ');
        QHash<QByteArray, QByteArray> headers;
        for (qsizetype index = 1; index < lines.size(); ++index) {
            const auto colon = lines[index].indexOf(':');
            if (colon > 0)
                headers.insert(lines[index].left(colon).trimmed().toLower(),
                               lines[index].mid(colon + 1).trimmed());
        }
        if (requestLine.size() != 3 || headers.contains("transfer-encoding")) {
            send({requestLine.size() != 3 ? 400 : 501, {}, {}}, true);
            buffer.clear();
            return;
        }
        bool numeric = true;
        const auto length = headers.value("content-length", "0").toLongLong(&numeric);
        if (!numeric || length < 0 || length > maximumRequestBytes) {
            send({numeric && length > 0 ? 413 : 400, {}, {}}, true);
            buffer.clear();
            return;
        }
        if (buffer.size() < end + 4 + length)
            return; // Wait for the rest of the body.
        const auto body = buffer.mid(end + 4, length);
        buffer.remove(0, end + 4 + length);
        const bool close = headers.value("connection").toLower() == "close";
        send(respond(requestLine[0], requestLine[1], headers, body), close);
        if (close)
            return;
    }
}

McpHttpServer::Response McpHttpServer::respond(const QByteArray& method, const QByteArray& path,
                                               const QHash<QByteArray, QByteArray>& headers,
                                               const QByteArray& body) {
    // DNS rebinding: a browser page must not reach this server through another name.
    if (!trustedHost(headers.value("host")) ||
        (headers.contains("origin") && !trustedOrigin(headers.value("origin"))))
        return {403, {}, {}};
    if (path.split('?').front() != "/mcp")
        return {404, {}, {}};
    const auto authorization = headers.value("authorization");
    bool authorized = mainTokenAccepted_ && sameSecret(authorization, "Bearer " + token_);
    for (const auto& run : runTokens_)
        authorized = sameSecret(authorization, "Bearer " + run) || authorized;
    if (!authorized)
        return {401, {}, {{"WWW-Authenticate", "Bearer"}}};
    if (activity_)
        activity_();
    const auto session = headers.value("mcp-session-id");
    if (method == "DELETE") {
        return sessions_.remove(session) ? Response{200, {}, {}} : Response{404, {}, {}};
    }
    if (method != "POST")
        return {405, {}, {{"Allow", "POST, DELETE"}}};
    if (!headers.value("content-type").toLower().startsWith("application/json"))
        return {415, {}, {}};
    if (headers.contains("mcp-protocol-version") &&
        !McpProtocol::supportedVersions().contains(
            QString::fromLatin1(headers.value("mcp-protocol-version"))))
        return {400, rpcError(-32600, "Unsupported MCP-Protocol-Version"), {}};

    QJsonParseError parsed{};
    const auto document = QJsonDocument::fromJson(body, &parsed);
    if (parsed.error != QJsonParseError::NoError)
        return {400, rpcError(-32700, "Parse error"), {}};
    if (!document.isObject()) // Batches are not part of MCP since 2025-06-18.
        return {400, rpcError(-32600, "Expected one JSON-RPC message"), {}};
    const auto message = document.object();
    const bool initialize = message.value("method") == QLatin1String("initialize");
    // A client that lost its session (e.g. after a restart of Völund) initializes again.
    if (!initialize && session.isEmpty())
        return {400, rpcError(-32600, "Missing Mcp-Session-Id"), {}};
    if (!initialize && !sessions_.contains(session))
        return {404, rpcError(-32600, "Unknown session"), {}};

    const auto response = protocol_.handle(message);
    if (!response)
        return {202, {}, {}};
    Response result{200, json(*response), {}};
    if (initialize && response->contains("result")) {
        const auto created = QUuid::createUuid().toByteArray(QUuid::WithoutBraces);
        sessions_.insert(created);
        result.headers.append({"Mcp-Session-Id", created});
    }
    return result;
}

bool McpHttpServer::trustedHost(const QByteArray& host) const {
    const auto port = QByteArray::number(this->port());
    return host == "127.0.0.1:" + port || host == "localhost:" + port;
}
bool McpHttpServer::trustedOrigin(const QByteArray& origin) const {
    const auto port = QByteArray::number(this->port());
    return origin == "http://127.0.0.1:" + port || origin == "http://localhost:" + port;
}

} // namespace voelund::mcp
