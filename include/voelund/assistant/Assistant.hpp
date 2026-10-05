#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>

#include <functional>
#include <memory>
#include <vector>

class QProcess;

// AI03a (docs/implementation-plan/AI03-plan.md): the assistant behind the Claude panel. A
// backend sends one user message per run and reports typed events; it is exchangeable
// (Claude Code now, an API-key adapter later). No user interface here.
namespace voelund::assistant {

struct AssistantEvent final {
    enum class Kind {
        started,      // A run began; sessionId identifies the conversation.
        text,         // Text of Claude's answer.
        toolStarted,  // Claude calls a Völund tool (tool without the mcp__volund__ prefix).
        toolFinished, // Its result; toolError for rejections and failures.
        finished,     // The run ended normally; text is the final answer.
        failed,       // The run ended with an error; text explains it.
        cancelled     // The user stopped the run.
    };
    Kind kind;
    QString text{};
    QString tool{};
    QString toolId{};
    bool toolError{false};
    QJsonObject toolResult{}; // structured result of a Völund tool, if any
    QString sessionId{};
};

// Parses one line of `claude -p --output-format stream-json`. Unknown lines and event types
// give no events; a Völund server that failed to connect is an error.
[[nodiscard]] std::vector<AssistantEvent> parseStreamJsonLine(const QByteArray& line);

class AssistantBackend {
  public:
    using Handler = std::function<void(const AssistantEvent&)>;
    virtual ~AssistantBackend() = default;
    // Starts a run unless one is running; events arrive on the calling thread.
    [[nodiscard]] virtual bool send(const QString& message) = 0;
    // Stops the run; no change of the run can happen afterwards.
    virtual void cancel() = 0;
    [[nodiscard]] virtual bool isRunning() const = 0;
    // A new conversation.
    virtual void reset() = 0;
    void setHandler(Handler handler) { handler_ = std::move(handler); }

  protected:
    void emitEvent(const AssistantEvent& event) const {
        if (handler_)
            handler_(event);
    }

  private:
    Handler handler_;
};

// Address and fresh token of the Völund MCP server for one run.
struct RunAccess final {
    QString url;
    QByteArray token; // Empty: the run must not start (e.g. AI functions switched off).
};

struct ClaudeCodeOptions final {
    QString program{QStringLiteral("claude")};
    // Opens access for a run (McpHttpServer run token) and revokes it at its end.
    std::function<RunAccess()> beginRun;
    std::function<void(const QByteArray&)> endRun;
};

// Runs `claude -p` once per message, continues the conversation with --resume. Only the
// Völund tools are available: built-in tools off, only this MCP server, user settings and
// hooks not loaded, non-essential traffic of Claude Code off.
class ClaudeCodeBackend final : public AssistantBackend {
  public:
    explicit ClaudeCodeBackend(ClaudeCodeOptions options);
    ~ClaudeCodeBackend() override;
    [[nodiscard]] bool send(const QString& message) override;
    void cancel() override;
    [[nodiscard]] bool isRunning() const override;
    void reset() override { sessionId_.clear(); }
    [[nodiscard]] const QString& sessionId() const noexcept { return sessionId_; }
    // The command line of a run, for tests and diagnostics.
    [[nodiscard]] QStringList arguments(const QString& message, const QString& configPath) const;

  private:
    void readOutput();
    void finish(int exitCode, bool crashed);
    void handle(const AssistantEvent& event);

    ClaudeCodeOptions options_;
    QTemporaryDir directory_;
    std::unique_ptr<QProcess> process_;
    QByteArray buffer_;
    QByteArray token_;
    QString sessionId_;
    bool ended_{false};
};

} // namespace voelund::assistant
