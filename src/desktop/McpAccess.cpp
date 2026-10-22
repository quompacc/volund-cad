// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/desktop/McpAccess.hpp>

#include <QRandomGenerator>
#include <QSettings>

#include <array>
#include <utility>

namespace voelund::desktop {
namespace {
QByteArray newToken() {
    std::array<quint32, 8> random{};
    QRandomGenerator::system()->fillRange(random.data(), random.size());
    return QByteArray(reinterpret_cast<const char*>(random.data()),
                      static_cast<qsizetype>(sizeof(random)))
        .toHex();
}
} // namespace

bool aiLockedByEnvironment() {
    const auto value = qEnvironmentVariable("VOELUND_DISABLE_AI").trimmed();
    return !value.isEmpty() && value != QLatin1String("0");
}

bool aiAllowed() {
    return !aiLockedByEnvironment() &&
           !QSettings().value(QStringLiteral("assistant/disabled"), false).toBool();
}

bool setAiAllowed(bool allowed) {
    if (allowed && aiLockedByEnvironment())
        return false;
    QSettings().setValue(QStringLiteral("assistant/disabled"), !allowed);
    return true;
}

McpSettings loadMcpSettings() {
    QSettings settings;
    auto token = settings.value(QStringLiteral("mcp/token")).toByteArray();
    if (token.size() < 32) {
        token = newToken();
        settings.setValue(QStringLiteral("mcp/token"), token);
    }
    const auto port = settings.value(QStringLiteral("mcp/port"), McpAccess::defaultPort).toUInt();
    return {static_cast<quint16>(port <= 65535 ? port : McpAccess::defaultPort), token,
            settings.value(QStringLiteral("mcp/enabled"), false).toBool()};
}

McpAccess::McpAccess(application::CadSession& session, const McpSettings& settings)
    : settings_(settings), host_(session), server_(host_, settings.token) {}

void McpAccess::setChangeHandler(mcp::ToolHost::ChangeHandler changed) {
    host_.setChangeHandler(std::move(changed));
}
void McpAccess::setActivityHandler(std::function<void()> activity) {
    server_.setActivityHandler(std::move(activity));
}

bool McpAccess::setEnabled(bool enabled) {
    if (enabled && !aiAllowed()) {
        server_.close();
        return false;
    }
    QSettings().setValue(QStringLiteral("mcp/enabled"), enabled);
    settings_.enabled = enabled;
    server_.setMainTokenAccepted(enabled);
    if (!enabled) {
        // A running panel run keeps its port; it closes with the run.
        if (!server_.hasRunTokens())
            server_.close();
        return true;
    }
    return server_.isListening() || server_.listen(settings_.port);
}
bool McpAccess::isRunning() const { return server_.isListening() && settings_.enabled; }

McpAccess::PanelRun McpAccess::beginPanelRun() {
    if (!aiAllowed())
        return {};
    if (!server_.isListening()) {
        if (!server_.listen(settings_.port))
            return {};
        server_.setMainTokenAccepted(false);
    }
    return {server_.url(), server_.issueRunToken()};
}

void McpAccess::endPanelRun(const QByteArray& token) {
    server_.revokeRunToken(token);
    if (!settings_.enabled && !server_.hasRunTokens())
        server_.close();
}

void McpAccess::regenerateToken() {
    settings_.token = newToken();
    QSettings().setValue(QStringLiteral("mcp/token"), settings_.token);
    server_.setToken(settings_.token);
}

QString McpAccess::url() const {
    return server_.isListening() ? server_.url()
                                 : QStringLiteral("http://127.0.0.1:%1/mcp").arg(settings_.port);
}

QString McpAccess::claudeCommand() const {
    return QStringLiteral("claude mcp add --transport http volund %1 --header \"Authorization: "
                          "Bearer %2\"")
        .arg(url(), QString::fromLatin1(settings_.token));
}

} // namespace voelund::desktop
