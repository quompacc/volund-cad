#include <voelund/assistant/Assistant.hpp>

#include <QFile>
#include <QJsonDocument>
#include <QProcess>
#include <QProcessEnvironment>

#include <utility>

namespace voelund::assistant {
namespace {
using Kind = AssistantEvent::Kind;
} // namespace

ClaudeCodeBackend::ClaudeCodeBackend(ClaudeCodeOptions options) : options_(std::move(options)) {}

ClaudeCodeBackend::~ClaudeCodeBackend() {
    if (!isRunning())
        return;
    if (options_.endRun && !token_.isEmpty())
        options_.endRun(token_);
    process_->disconnect();
    process_->kill();
    process_->waitForFinished(2000);
}

QStringList ClaudeCodeBackend::arguments(const QString& message, const QString& configPath) const {
    QStringList arguments{QStringLiteral("-p"),
                          message,
                          QStringLiteral("--output-format"),
                          QStringLiteral("stream-json"),
                          QStringLiteral("--verbose"),
                          QStringLiteral("--tools"),
                          QString(),
                          QStringLiteral("--strict-mcp-config"),
                          QStringLiteral("--mcp-config"),
                          configPath,
                          QStringLiteral("--allowedTools"),
                          QStringLiteral("mcp__volund__*"),
                          QStringLiteral("--setting-sources"),
                          QString()};
    if (!sessionId_.isEmpty())
        arguments << QStringLiteral("--resume") << sessionId_;
    return arguments;
}

bool ClaudeCodeBackend::send(const QString& message) {
    // Claude Code reports its result shortly before the process exits; a message sent in
    // between must not be lost.
    if (isRunning()) {
        if (!ended_)
            return false; // The previous message is still being worked on.
        if (!process_->waitForFinished(5000)) {
            process_->kill();
            process_->waitForFinished(2000);
        }
    }
    const auto access = options_.beginRun ? options_.beginRun() : RunAccess{};
    token_ = access.token;
    if (token_.isEmpty()) {
        handle({Kind::failed, QStringLiteral("Die KI-Funktionen sind ausgeschaltet.")});
        return false;
    }
    const auto config = directory_.path() + QStringLiteral("/mcp.json");
    QFile file(config);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        options_.endRun(std::exchange(token_, {}));
        handle({Kind::failed, QStringLiteral("Die MCP-Konfiguration ließ sich nicht schreiben.")});
        return false;
    }
    const QJsonObject server{{"type", "http"},
                             {"url", access.url},
                             {"headers", QJsonObject{{"Authorization",
                                                      QString::fromLatin1("Bearer " + token_)}}}};
    file.write(QJsonDocument(QJsonObject{{"mcpServers", QJsonObject{{"volund", server}}}}).toJson());
    file.close();

    process_ = std::make_unique<QProcess>();
    auto environment = QProcessEnvironment::systemEnvironment();
    // No telemetry, error reports or updates of Claude Code; only the model requests leave.
    environment.insert(QStringLiteral("CLAUDE_CODE_DISABLE_NONESSENTIAL_TRAFFIC"),
                       QStringLiteral("1"));
    process_->setProcessEnvironment(environment);
    process_->setProgram(options_.program);
    process_->setArguments(arguments(message, config));
    process_->setStandardInputFile(QProcess::nullDevice());
    QObject::connect(process_.get(), &QProcess::readyReadStandardOutput,
                     [this] { readOutput(); });
    QObject::connect(process_.get(), &QProcess::finished,
                     [this](int code, QProcess::ExitStatus status) {
                         finish(code, status == QProcess::CrashExit);
                     });
    QObject::connect(process_.get(), &QProcess::errorOccurred, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            finish(-1, false);
    });
    buffer_.clear();
    ended_ = false;
    process_->start();
    return true;
}

void ClaudeCodeBackend::cancel() {
    if (!isRunning())
        return;
    // Revoke first: from now on no call of this run reaches the document.
    if (options_.endRun && !token_.isEmpty())
        options_.endRun(std::exchange(token_, {}));
    ended_ = true;
    process_->kill();
    handle({Kind::cancelled, QStringLiteral("Abgebrochen.")});
}

bool ClaudeCodeBackend::isRunning() const {
    return process_ && process_->state() != QProcess::NotRunning;
}

void ClaudeCodeBackend::readOutput() {
    buffer_ += process_->readAllStandardOutput();
    qsizetype end = -1;
    while ((end = buffer_.indexOf('\n')) >= 0) {
        const auto line = buffer_.left(end);
        buffer_.remove(0, end + 1);
        if (ended_)
            continue; // Output after a cancel is ignored.
        for (const auto& event : parseStreamJsonLine(line))
            handle(event);
    }
}

void ClaudeCodeBackend::finish(int exitCode, bool crashed) {
    if (process_->error() != QProcess::FailedToStart)
        readOutput();
    if (options_.endRun && !token_.isEmpty())
        options_.endRun(std::exchange(token_, {}));
    if (ended_)
        return;
    QString reason;
    if (process_->error() == QProcess::FailedToStart)
        reason = QStringLiteral("Claude Code wurde nicht gefunden. Installation und Anmeldung: "
                                "https://claude.com/claude-code");
    else if (crashed)
        reason = QStringLiteral("Claude Code wurde unerwartet beendet.");
    else {
        const auto details =
            QString::fromLocal8Bit(process_->readAllStandardError()).trimmed().split('\n').last();
        reason = QStringLiteral("Claude Code endete mit Code %1. %2").arg(exitCode).arg(details);
    }
    handle({Kind::failed, reason.trimmed()});
}

void ClaudeCodeBackend::handle(const AssistantEvent& event) {
    if (!event.sessionId.isEmpty() &&
        (event.kind == Kind::started || event.kind == Kind::finished))
        sessionId_ = event.sessionId;
    if (event.kind == Kind::finished || event.kind == Kind::failed ||
        event.kind == Kind::cancelled)
        ended_ = true;
    // A run that failed early (e.g. Völund not reachable) does not continue without tools.
    if (event.kind == Kind::failed && isRunning()) {
        if (options_.endRun && !token_.isEmpty())
            options_.endRun(std::exchange(token_, {}));
        process_->kill();
    }
    emitEvent(event);
}

} // namespace voelund::assistant
