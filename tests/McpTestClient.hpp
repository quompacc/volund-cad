#pragma once
// Minimal HTTP/MCP client for tests of the local MCP server (AI04b/c). The server runs in
// the test's thread, so every wait spins the event loop.
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>

#include <stdexcept>
#include <string>
#include <utility>

namespace voelund::test::mcpclient {

inline void check(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}

struct HttpResponse final {
    int status{0};
    QHash<QByteArray, QByteArray> headers;
    QByteArray body;
    [[nodiscard]] QJsonObject json() const { return QJsonDocument::fromJson(body).object(); }
};

// One keep-alive connection; the server runs in this thread, so we spin its event loop.
class Connection final {
  public:
    explicit Connection(quint16 port) : port_(port) {
        socket_.connectToHost(QHostAddress::LocalHost, port);
        spinUntil([&] { return socket_.state() == QAbstractSocket::ConnectedState; }, "connect");
    }
    HttpResponse send(const QByteArray& method, const QByteArray& body,
                      QList<QPair<QByteArray, QByteArray>> headers) {
        QByteArray request = method + " /mcp HTTP/1.1\r\n";
        for (const auto& [name, value] : headers)
            request += name + ": " + value + "\r\n";
        request += "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body;
        return sendRaw(request, method);
    }
    HttpResponse sendRaw(const QByteArray& request, const QByteArray& method) {
        socket_.write(request);
        QByteArray buffer;
        HttpResponse response;
        spinUntil(
            [&] {
                buffer += socket_.readAll();
                const auto end = buffer.indexOf("\r\n\r\n");
                if (end < 0)
                    return false;
                const auto lines = buffer.left(end).split('\n');
                response.status = lines.front().split(' ').value(1).toInt();
                for (qsizetype i = 1; i < lines.size(); ++i) {
                    const auto colon = lines[i].indexOf(':');
                    response.headers.insert(lines[i].left(colon).trimmed().toLower(),
                                            lines[i].mid(colon + 1).trimmed());
                }
                const auto length = response.headers.value("content-length").toLongLong();
                if (buffer.size() < end + 4 + length)
                    return false;
                response.body = buffer.mid(end + 4, length);
                return true;
            },
            method + " response");
        return response;
    }
    [[nodiscard]] quint16 port() const { return port_; }

  private:
    template <class Done> void spinUntil(Done done, const QByteArray& what) {
        QElapsedTimer timer;
        timer.start();
        while (!done()) {
            check(timer.elapsed() < 5000, "Timeout waiting for the server: " + what.toStdString());
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        }
    }
    QTcpSocket socket_;
    quint16 port_;
};

class Client final {
  public:
    Client(quint16 port, QByteArray token) : connection_(port), token_(std::move(token)) {}
    HttpResponse post(const QJsonObject& message, QList<QPair<QByteArray, QByteArray>> extra = {}) {
        auto headers = base();
        headers.append(extra);
        return connection_.send("POST", QJsonDocument(message).toJson(QJsonDocument::Compact),
                                headers);
    }
    HttpResponse raw(const QByteArray& method, const QByteArray& body,
                     QList<QPair<QByteArray, QByteArray>> headers) {
        return connection_.send(method, body, headers);
    }
    QList<QPair<QByteArray, QByteArray>> base() const {
        QList<QPair<QByteArray, QByteArray>> headers{
            {"Host", "127.0.0.1:" + QByteArray::number(connection_.port())},
            {"Authorization", "Bearer " + token_},
            {"Content-Type", "application/json"},
            {"Accept", "application/json, text/event-stream"}};
        if (!session.isEmpty())
            headers.append({"Mcp-Session-Id", session});
        return headers;
    }
    // initialize + notifications/initialized.
    QJsonObject open() {
        const auto response = post(request(
            "initialize", {{"protocolVersion", "2025-06-18"},
                           {"capabilities", QJsonObject{}},
                           {"clientInfo", QJsonObject{{"name", "test"}, {"version", "1"}}}}));
        check(response.status == 200 && response.headers.contains("mcp-session-id"),
              "initialize opens a session");
        session = response.headers.value("mcp-session-id");
        check(post({{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}}).status == 202,
              "Notifications are accepted without a body");
        return response.json().value("result").toObject();
    }
    QJsonObject call(const QString& tool, const QJsonObject& arguments) {
        const auto response =
            post(request("tools/call", {{"name", tool}, {"arguments", arguments}}),
                 {{"MCP-Protocol-Version", "2025-06-18"}});
        check(response.status == 200, "tools/call answers 200");
        return response.json();
    }
    QJsonObject request(const QString& method, const QJsonObject& params) {
        return {{"jsonrpc", "2.0"}, {"id", ++id_}, {"method", method}, {"params", params}};
    }
    QByteArray session;

  private:
    Connection connection_;
    QByteArray token_;
    int id_{0};
};

inline QJsonObject structured(const QJsonObject& response) {
    return response.value("result").toObject().value("structuredContent").toObject();
}

} // namespace voelund::test::mcpclient
