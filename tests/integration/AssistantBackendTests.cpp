#include <voelund/application/CadSession.hpp>
#include <voelund/assistant/Assistant.hpp>
#include <voelund/mcp/McpServer.hpp>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>

#include <iostream>
#include <stdexcept>
#include <string>

// AI03a: the Claude Code backend against a fake `claude` that makes real MCP calls. Command
// line and environment, event sequence, --resume, a stopped or revoked run that cannot change
// the document any more, errors of the process, unknown output, an unreachable server.
namespace {
using namespace voelund;
using assistant::AssistantEvent;
using Kind = AssistantEvent::Kind;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
template <class Done> void spinUntil(Done done, const char* what) {
    QElapsedTimer timer;
    timer.start();
    while (!done()) {
        require(timer.elapsed() < 30000, std::string("Timeout: ") + what);
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
}
bool ends(const AssistantEvent& event) {
    return event.kind == Kind::finished || event.kind == Kind::failed ||
           event.kind == Kind::cancelled;
}

class Fixture final {
  public:
    Fixture(const QString& program)
        : host(session), server(host, "main-token-not-used-by-the-panel") {
        session.newDocument();
        require(server.listen(), "Server listens");
        server.setMainTokenAccepted(false); // Only run tokens, as with external access off.
        backend = std::make_unique<assistant::ClaudeCodeBackend>(assistant::ClaudeCodeOptions{
            program,
            [this] {
                lastToken = allowed ? server.issueRunToken() : QByteArray();
                return assistant::RunAccess{server.url(), lastToken};
            },
            [this](const QByteArray& token) { server.revokeRunToken(token); }});
        backend->setHandler([this](const AssistantEvent& event) { events.push_back(event); });
    }
    std::vector<AssistantEvent> runToEnd(const QString& message) {
        events.clear();
        require(backend->send(message), "The run starts");
        spinUntil([&] { return !events.empty() && ends(events.back()); }, "end of run");
        spinUntil([&] { return !backend->isRunning(); }, "process exit");
        return events;
    }
    [[nodiscard]] bool has(Kind kind, const QString& text = {}) const {
        return std::ranges::any_of(events, [&](const auto& event) {
            return event.kind == kind && (text.isEmpty() || event.text.contains(text));
        });
    }
    application::CadSession session;
    mcp::ToolHost host;
    mcp::McpHttpServer server;
    std::unique_ptr<assistant::ClaudeCodeBackend> backend;
    std::vector<AssistantEvent> events;
    QByteArray lastToken;
    bool allowed{true};
};

void parser() {
    auto events = assistant::parseStreamJsonLine(
        R"({"type":"assistant","message":{"content":[{"type":"text","text":"Hallo"},)"
        R"({"type":"tool_use","id":"x","name":"mcp__volund__create_box","input":{}}]}})");
    require(events.size() == 2 && events[0].kind == Kind::text && events[0].text == "Hallo" &&
                events[1].kind == Kind::toolStarted && events[1].tool == "create_box" &&
                events[1].toolId == "x",
            "Text and tool use");
    events = assistant::parseStreamJsonLine(
        R"({"type":"user","message":{"content":[{"type":"tool_result","tool_use_id":"x",)"
        R"("is_error":true,"content":[{"type":"text","text":"{\"rejection\":\"staleStamp\"}"}]}]}})");
    require(events.size() == 1 && events[0].toolError &&
                events[0].toolResult.value("rejection") == QLatin1String("staleStamp"),
            "Tool result with its structured content");
    require(assistant::parseStreamJsonLine("garbage").empty() &&
                assistant::parseStreamJsonLine(R"({"type":"rate_limit_event"})").empty(),
            "Unknown lines give no events");
    events = assistant::parseStreamJsonLine(
        R"({"type":"result","subtype":"error_max_turns","is_error":true,"session_id":"s"})");
    require(events.size() == 1 && events[0].kind == Kind::failed &&
                events[0].text == "error_max_turns",
            "A failed result");
}

void runs(const QString& program, const QString& work) {
    Fixture fixture(program);
    const auto argumentsFile = work + "/args.txt";
    qputenv("FAKE_CLAUDE_ARGS", argumentsFile.toLocal8Bit());
    qputenv("FAKE_CLAUDE_SCENARIO", "plate");

    auto events = fixture.runToEnd("Lege A an");
    require(events.front().kind == Kind::started && fixture.has(Kind::finished) &&
                fixture.backend->sessionId() == "fake-session-1",
            "A run starts and finishes with a session");
    const auto created = std::ranges::find_if(events, [](const auto& event) {
        return event.kind == Kind::toolFinished && event.toolId == "t2";
    });
    require(created != events.end() && !created->toolError &&
                created->toolResult.value("status") == QLatin1String("committed") &&
                fixture.session.parameterTable().values.size() == 1,
            "The fake's tool call changed the document");
    require(!fixture.server.hasRunTokens(), "The run token ends with the run");
    QFile file(argumentsFile);
    require(file.open(QIODevice::ReadOnly), "Arguments recorded");
    const auto recorded = QString::fromUtf8(file.readAll()).split('\n');
    const auto at = [&](const char* flag) { return recorded.indexOf(QLatin1String(flag)); };
    require(recorded.value(at("--tools") + 1).isEmpty() && at("--strict-mcp-config") >= 0 &&
                recorded.value(at("--allowedTools") + 1) == "mcp__volund__*" &&
                recorded.value(at("--setting-sources") + 1).isEmpty() &&
                recorded.value(at("--output-format") + 1) == "stream-json" && at("--resume") < 0 &&
                recorded.last() == "1",
            "Only Völund tools, no user settings, no non-essential traffic");
    file.close();

    qputenv("FAKE_CLAUDE_SCENARIO", "noise");
    fixture.runToEnd("Weiter");
    QFile again(argumentsFile);
    require(again.open(QIODevice::ReadOnly) &&
                again.readAll().contains("--resume\nfake-session-1") &&
                fixture.has(Kind::finished, "Nichts zu tun"),
            "The conversation continues; unknown output is ignored");

    // The next message right after the result, while the process is still exiting.
    qputenv("FAKE_CLAUDE_SCENARIO", "linger");
    fixture.events.clear();
    require(fixture.backend->send("Kurz"), "Lingering run starts");
    spinUntil([&] { return fixture.has(Kind::finished); }, "lingering result");
    require(fixture.backend->isRunning(), "The process still exits after its result");
    qputenv("FAKE_CLAUDE_SCENARIO", "noise");
    fixture.events.clear();
    require(fixture.backend->send("Sofort weiter"), "A message right after the result is sent");
    spinUntil([&] { return !fixture.events.empty() && ends(fixture.events.back()); }, "next run");
    require(fixture.has(Kind::finished, "Nichts zu tun"), "The next message is answered");
    spinUntil([&] { return !fixture.backend->isRunning(); }, "next exit");

    // A revoked run token: the run goes on, but its change is refused (no late change).
    const auto trigger = work + "/trigger";
    qputenv("FAKE_CLAUDE_TRIGGER", trigger.toLocal8Bit());
    qputenv("FAKE_CLAUDE_SCENARIO", "late");
    fixture.session.newDocument();
    fixture.events.clear();
    require(fixture.backend->send("Später"), "Late run starts");
    spinUntil([&] { return fixture.has(Kind::toolFinished); }, "context read");
    fixture.server.revokeRunToken(fixture.lastToken);
    static_cast<void>(QFile(trigger).open(QIODevice::WriteOnly));
    spinUntil([&] { return !fixture.events.empty() && ends(fixture.events.back()); }, "late end");
    require(fixture.session.parameterTable().values.empty() &&
                std::ranges::any_of(fixture.events,
                                    [](const auto& event) {
                                        return event.kind == Kind::toolFinished &&
                                               event.toolId == "t2" && event.toolError;
                                    }),
            "After revocation the run cannot change the document");
    spinUntil([&] { return !fixture.backend->isRunning(); }, "late exit");
    QFile::remove(trigger);

    // Stop: cancelled at once, the process ends, nothing changes.
    fixture.events.clear();
    require(fixture.backend->send("Stopp mich"), "Run to stop starts");
    spinUntil([&] { return fixture.has(Kind::toolFinished); }, "context before stop");
    fixture.backend->cancel();
    require(fixture.events.back().kind == Kind::cancelled && !fixture.server.hasRunTokens(),
            "Stop reports cancelled and revokes the token");
    static_cast<void>(QFile(trigger).open(QIODevice::WriteOnly));
    spinUntil([&] { return !fixture.backend->isRunning(); }, "stopped exit");
    require(fixture.session.parameterTable().values.empty() &&
                fixture.events.back().kind == Kind::cancelled,
            "Nothing changes and nothing follows after the stop");
    QFile::remove(trigger);

    qputenv("FAKE_CLAUDE_SCENARIO", "unreachable");
    QElapsedTimer timer;
    timer.start();
    fixture.runToEnd("Ohne Server");
    require(fixture.has(Kind::failed, "nicht erreichbar") && timer.elapsed() < 15000,
            "An unreachable Völund ends the run at once");

    qputenv("FAKE_CLAUDE_SCENARIO", "fail");
    fixture.runToEnd("Fehler");
    require(fixture.has(Kind::failed, "Code 3") && fixture.has(Kind::failed, "not logged in"),
            "Exit code and message of a failing Claude Code");

    fixture.allowed = false;
    fixture.events.clear();
    require(!fixture.backend->send("Aus") && fixture.has(Kind::failed, "ausgeschaltet") &&
                !fixture.backend->isRunning(),
            "No run while AI functions are off");
}

void missingProgram() {
    Fixture fixture(QStringLiteral("voelund-no-such-claude"));
    fixture.runToEnd("Hallo");
    require(fixture.has(Kind::failed, "nicht gefunden") && !fixture.server.hasRunTokens(),
            "A missing Claude Code is reported");
}
} // namespace

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    try {
        require(argc > 1, "Path of the fake claude expected");
        QTemporaryDir work;
        parser();
        runs(QString::fromLocal8Bit(argv[1]), work.path());
        missingProgram();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "PASS AI03a: Claude Code backend\n";
    return 0;
}
