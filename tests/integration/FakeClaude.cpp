#include "../McpTestClient.hpp"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QThread>
#include <QUrl>

#include <cstdio>
#include <iostream>

// AI03a test double for `claude -p --output-format stream-json`: makes real MCP calls against
// the server named in --mcp-config and prints the events Claude Code prints. The scenario
// comes from FAKE_CLAUDE_SCENARIO; FAKE_CLAUDE_ARGS names a file for the received arguments.
namespace {
using voelund::test::mcpclient::Client;
using voelund::test::mcpclient::structured;

void print(const QJsonObject& event) {
    std::cout << QJsonDocument(event).toJson(QJsonDocument::Compact).toStdString() << std::endl;
}
void init(const QString& session, const char* status) {
    print({{"type", "system"},
           {"subtype", "init"},
           {"session_id", session},
           {"tools", QJsonArray{}},
           {"mcp_servers", QJsonArray{QJsonObject{{"name", "volund"}, {"status", status}}}}});
}
QJsonObject message(const QString& type, const QJsonObject& block) {
    return {{"type", type},
            {"message", QJsonObject{{"content", QJsonArray{block}}}}};
}
void toolUse(const QString& id, const QString& tool) {
    print(message("assistant", {{"type", "tool_use"},
                                {"id", id},
                                {"name", "mcp__volund__" + tool},
                                {"input", QJsonObject{}}}));
}
void toolResult(const QString& id, const QJsonObject& result, bool error) {
    const QJsonObject textBlock{
        {"type", "text"},
        {"text", QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact))}};
    print(message("user", {{"type", "tool_result"},
                           {"tool_use_id", id},
                           {"is_error", error},
                           {"content", QJsonArray{textBlock}}}));
}
void text(const QString& value) { print(message("assistant", {{"type", "text"}, {"text", value}})); }
void finalResult(const QString& session, const QString& value, bool success) {
    print({{"type", "result"},
           {"subtype", success ? "success" : "error_during_execution"},
           {"is_error", !success},
           {"session_id", session},
           {"result", value}});
}

struct Server final {
    quint16 port;
    QByteArray token;
};
Server server(const QStringList& arguments) {
    const auto index = arguments.indexOf("--mcp-config");
    QFile file(arguments.value(index + 1));
    if (!file.open(QIODevice::ReadOnly))
        throw std::runtime_error("No MCP config");
    const auto volund = QJsonDocument::fromJson(file.readAll())
                            .object()
                            .value("mcpServers")
                            .toObject()
                            .value("volund")
                            .toObject();
    const auto header = volund.value("headers").toObject().value("Authorization").toString();
    return {static_cast<quint16>(QUrl(volund.value("url").toString()).port()),
            header.mid(QStringLiteral("Bearer ").size()).toLatin1()};
}

// One tool call; a refused request (e.g. revoked token) becomes an error result.
QJsonObject call(Client& client, const QString& tool, const QJsonObject& arguments, bool& error) {
    const auto response = client.post(
        client.request("tools/call", {{"name", tool}, {"arguments", arguments}}));
    if (response.status != 200) {
        error = true;
        return {{"error", QStringLiteral("HTTP %1").arg(response.status)}};
    }
    const auto structuredContent = structured(response.json());
    error = response.json().value("result").toObject().value("isError").toBool();
    return structuredContent;
}

int run(const QStringList& arguments) {
    const auto scenario = qEnvironmentVariable("FAKE_CLAUDE_SCENARIO", "plate");
    if (const auto path = qEnvironmentVariable("FAKE_CLAUDE_ARGS"); !path.isEmpty()) {
        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            file.write(arguments.join('\n').toUtf8() + "\n--env--\n" +
                       qgetenv("CLAUDE_CODE_DISABLE_NONESSENTIAL_TRAFFIC"));
        }
    }
    const auto resume = arguments.indexOf("--resume");
    const QString session = resume >= 0 ? arguments.value(resume + 1) : "fake-session-1";
    if (scenario == "fail") {
        std::cerr << "Error: not logged in" << std::endl;
        return 3;
    }
    if (scenario == "unreachable") {
        init(session, "failed");
        QThread::sleep(20); // The backend has to stop this run.
        return 0;
    }
    init(session, "connected");
    if (scenario == "linger") {
        // Like Claude Code: the result comes a moment before the process exits.
        text("Fertig.");
        finalResult(session, "Fertig.", true);
        QThread::msleep(1500);
        return 0;
    }
    if (scenario == "noise") {
        std::cout << "not json" << std::endl;
        print({{"type", "rate_limit_event"}, {"session_id", session}});
        print({{"type", "system"}, {"subtype", "thinking_tokens"}});
        text("Nichts zu tun.");
        finalResult(session, "Nichts zu tun.", true);
        return 0;
    }
    const auto target = server(arguments);
    Client client(target.port, target.token);
    static_cast<void>(client.open());
    bool error = false;
    toolUse("t1", "get_context");
    const auto context = call(client, "get_context", {}, error);
    toolResult("t1", context, error);
    if (scenario == "late") {
        // Waits for the test (cancel or revocation), then tries to change the document.
        const auto trigger = qEnvironmentVariable("FAKE_CLAUDE_TRIGGER");
        for (int i = 0; i < 400 && !QFile::exists(trigger); ++i)
            QThread::msleep(50);
    }
    toolUse("t2", "create_parameter");
    const auto created = call(client, "create_parameter",
                              {{"requestId", "fake-" + session},
                               {"documentId", context.value("documentId")},
                               {"stamp", context.value("stamp")},
                               {"name", "A"},
                               {"unit", "length"},
                               {"value", QJsonObject{{"value", 8}, {"unit", "mm"}}}},
                              error);
    toolResult("t2", created, error);
    text(error ? "Das hat nicht geklappt." : "Parameter A angelegt.");
    finalResult(session, error ? "Das hat nicht geklappt." : "Parameter A angelegt.", true);
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    try {
        return run(QCoreApplication::arguments().mid(1));
    } catch (const std::exception& failure) {
        std::cerr << failure.what() << std::endl;
        return 2;
    }
}
