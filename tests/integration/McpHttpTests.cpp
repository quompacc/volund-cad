#include <voelund/application/CadSession.hpp>
#include <voelund/mcp/McpServer.hpp>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTcpSocket>

#include <iostream>
#include <stdexcept>
#include <string>

// AI04b: MCP over local HTTP. Handshake, tools, a mutation, a reconnect that retries the
// same request without a second change, and the transport's refusals (token, host, origin,
// session, method, size, malformed JSON) without any change.
namespace {
using namespace voelund;

void require(bool condition, const std::string& message) {
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
        spinUntil([&] {
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
        }, method + " response");
        return response;
    }
    [[nodiscard]] quint16 port() const { return port_; }

  private:
    template <class Done> void spinUntil(Done done, const QByteArray& what) {
        QElapsedTimer timer;
        timer.start();
        while (!done()) {
            require(timer.elapsed() < 5000, "Timeout waiting for the server: " + what.toStdString());
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        }
    }
    QTcpSocket socket_;
    quint16 port_;
};

class Client final {
  public:
    Client(quint16 port, QByteArray token) : connection_(port), token_(std::move(token)) {}
    HttpResponse post(const QJsonObject& message,
                      QList<QPair<QByteArray, QByteArray>> extra = {}) {
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
        const auto response = post(request("initialize", {{"protocolVersion", "2025-06-18"},
                                                          {"capabilities", QJsonObject{}},
                                                          {"clientInfo", QJsonObject{{"name", "test"},
                                                                                     {"version", "1"}}}}));
        require(response.status == 200 && response.headers.contains("mcp-session-id"),
                "initialize opens a session");
        session = response.headers.value("mcp-session-id");
        require(post({{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}}).status == 202,
                "Notifications are accepted without a body");
        return response.json().value("result").toObject();
    }
    QJsonObject call(const QString& tool, const QJsonObject& arguments) {
        const auto response =
            post(request("tools/call", {{"name", tool}, {"arguments", arguments}}),
                 {{"MCP-Protocol-Version", "2025-06-18"}});
        require(response.status == 200, "tools/call answers 200");
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

QJsonObject structured(const QJsonObject& response) {
    return response.value("result").toObject().value("structuredContent").toObject();
}

void run() {
    application::CadSession session;
    session.newDocument();
    int changes = 0;
    mcp::ToolHost host(session, [&] { ++changes; });
    const QByteArray token = "test-token-0123456789";
    mcp::McpHttpServer server(host, token);
    require(server.listen() && server.port() != 0, "Listens on a free local port");
    require(server.url() == QStringLiteral("http://127.0.0.1:%1/mcp").arg(server.port()),
            "URL for claude mcp add");

    Client client(server.port(), token);
    const auto initialized = client.open();
    require(initialized.value("protocolVersion") == QLatin1String("2025-06-18") &&
                initialized.value("capabilities").toObject().contains("tools") &&
                !initialized.value("instructions").toString().isEmpty(),
            "Version negotiated, tools offered, instructions given");
    const auto tools = client.post(client.request("tools/list", {})).json();
    require(tools.value("result").toObject().value("tools").toArray().size() == 16,
            "tools/list returns the catalog");

    const auto context = structured(client.call("get_context", {}));
    const QJsonObject create{{"requestId", "create-A"},
                             {"documentId", context.value("documentId")},
                             {"stamp", context.value("stamp")},
                             {"name", "A"},
                             {"unit", "length"},
                             {"value", QJsonObject{{"value", 8}, {"unit", "mm"}}}};
    auto result = structured(client.call("create_parameter", create));
    require(result.value("status") == QLatin1String("committed") && changes == 1 &&
                session.parameterTable().values.size() == 1,
            "A tool call changes the document");
    const auto stamp = session.sceneStatus().document;

    // Reconnect: a new connection and session retry the request whose answer was lost.
    Client again(server.port(), token);
    static_cast<void>(again.open());
    result = structured(again.call("create_parameter", create));
    require(result.value("repeated").toBool() && changes == 1 &&
                session.sceneStatus().document == stamp &&
                session.parameterTable().values.size() == 1,
            "The retried request is not applied twice");

    // Refusals of the transport; none of them changes anything.
    const auto unchanged = [&](const std::string& message) {
        require(changes == 1 && session.sceneStatus().document == stamp, message + ": no change");
    };
    const auto withHeader = [&](const QByteArray& name, const QByteArray& value) {
        auto headers = client.base();
        for (auto& header : headers)
            if (header.first == name)
                header.second = value;
        return headers;
    };
    const auto body = QJsonDocument(client.request("tools/call", {{"name", "create_parameter"},
                                                                  {"arguments", create}}))
                          .toJson(QJsonDocument::Compact);
    require(client.raw("POST", body, withHeader("Authorization", "Bearer wrong")).status == 401,
            "Wrong token");
    unchanged("Wrong token");
    require(client.raw("POST", body, withHeader("Host", "evil.example:80")).status == 403,
            "Foreign host name (DNS rebinding)");
    auto origin = client.base();
    origin.append(qMakePair(QByteArray("Origin"), QByteArray("https://evil.example")));
    require(client.raw("POST", body, origin).status == 403, "Foreign origin");
    require(client.raw("POST", body, withHeader("Mcp-Session-Id", "unknown")).status == 404,
            "Unknown session");
    require(client.raw("GET", {}, client.base()).status == 405, "No server push stream");
    require(client.raw("POST", "{not json", client.base()).json().value("error").toObject().value(
                "code") == -32700,
            "Parse error");
    require(client.raw("POST", "[]", client.base()).status == 400, "No batches");
    unchanged("Refusals");
    const auto unknownMethod = client.post(client.request("resources/list", {})).json();
    require(unknownMethod.value("error").toObject().value("code") == -32601, "Unknown method");
    const auto unknownTool =
        client.post(client.request("tools/call", {{"name", "format_disk"}})).json();
    require(unknownTool.value("error").toObject().value("code") == -32602, "Unknown tool");
    unchanged("Unknown method and tool");

    // Refused from the header alone, before any body is read.
    Connection large(server.port());
    QByteArray oversized = "POST /mcp HTTP/1.1\r\n";
    for (const auto& [name, value] : client.base())
        oversized += name + ": " + value + "\r\n";
    oversized += "Content-Length: " +
                 QByteArray::number(mcp::McpHttpServer::maximumRequestBytes + 1) + "\r\n\r\n";
    require(large.sendRaw(oversized, "POST").status == 413, "Oversized request");
    require(client.raw("DELETE", {}, client.base()).status == 200 &&
                client.post(client.request("ping", {})).status == 404,
            "A deleted session is gone");
    unchanged("Session end");
    std::cout << "PASS AI04b: MCP over local HTTP\n";
}
} // namespace

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    try {
        run();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
