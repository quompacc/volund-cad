#include <voelund/application/CadSession.hpp>
#include <voelund/mcp/McpServer.hpp>

#include "../McpTestClient.hpp"

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
using namespace voelund::test::mcpclient;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}

void run() {
    application::CadSession session;
    session.newDocument();
    int changes = 0;
    mcp::ToolHost host(session, [&](const auto&) { ++changes; });
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
    require(tools.value("result").toObject().value("tools").toArray().size() == 18,
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
    require(client.raw("POST", "{not json", client.base())
                    .json()
                    .value("error")
                    .toObject()
                    .value("code") == -32700,
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
    oversized +=
        "Content-Length: " + QByteArray::number(mcp::McpHttpServer::maximumRequestBytes + 1) +
        "\r\n\r\n";
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
