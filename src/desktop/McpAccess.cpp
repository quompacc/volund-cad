#include <voelund/desktop/MainWindow.hpp>
#include <voelund/desktop/McpAccess.hpp>

#include <QRandomGenerator>
#include <QSettings>

#include <array>

namespace voelund::desktop {

McpSettings loadMcpSettings() {
    QSettings settings;
    auto token = settings.value(QStringLiteral("mcp/token")).toByteArray();
    if (token.size() < 32) {
        std::array<quint32, 8> random{};
        QRandomGenerator::system()->fillRange(random.data(), random.size());
        token = QByteArray(reinterpret_cast<const char*>(random.data()),
                           static_cast<qsizetype>(sizeof(random)))
                    .toHex();
        settings.setValue(QStringLiteral("mcp/token"), token);
    }
    const auto port = settings.value(QStringLiteral("mcp/port"), McpAccess::defaultPort).toUInt();
    return {static_cast<quint16>(port > 0 && port <= 65535 ? port : McpAccess::defaultPort),
            token};
}

McpAccess::McpAccess(application::CadSession& session, MainWindow& window,
                     const McpSettings& settings)
    : settings_(settings), host_(session, [&window] { window.refreshAfterExternalChange(); }),
      server_(host_, settings.token) {}

bool McpAccess::start() { return server_.isListening() || server_.listen(settings_.port); }
void McpAccess::stop() { server_.close(); }
bool McpAccess::isRunning() const { return server_.isListening(); }
QString McpAccess::url() const { return server_.url(); }

QString McpAccess::claudeCommand() const {
    return QStringLiteral("claude mcp add --transport http volund %1 --header \"Authorization: "
                          "Bearer %2\"")
        .arg(url(), QString::fromLatin1(settings_.token));
}

} // namespace voelund::desktop
