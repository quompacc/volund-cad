#pragma once
#include <voelund/mcp/McpServer.hpp>
#include <voelund/mcp/ToolHost.hpp>

#include <QByteArray>
#include <QString>

#include <functional>

namespace voelund::application {
class CadSession;
}

// AI04c (docs/implementation-plan/AI04-plan.md): the local MCP server of a running Völund.
// Tool calls run on the GUI thread; the shell refreshes after each published change.
namespace voelund::desktop {

struct McpSettings final {
    quint16 port; // 0: any free port (tests)
    QByteArray token;
    bool enabled{false};
};
// Port, token and the on/off state persist in the user settings, so a client configured
// once keeps working and the access returns after a restart only if it was on. A missing
// token is generated (32 random bytes, hex).
[[nodiscard]] McpSettings loadMcpSettings();

class McpAccess final {
  public:
    static constexpr quint16 defaultPort = 47913;

    McpAccess(application::CadSession& session, const McpSettings& settings);
    void setChangeHandler(mcp::ToolHost::ChangeHandler changed);
    void setActivityHandler(std::function<void()> activity);
    // Starts or stops the server and remembers the choice.
    [[nodiscard]] bool setEnabled(bool enabled);
    [[nodiscard]] bool isRunning() const;
    // A new token: connected clients have to be set up again.
    void regenerateToken();
    [[nodiscard]] QString url() const;
    // The command that connects Claude Code to this server.
    [[nodiscard]] QString claudeCommand() const;

  private:
    McpSettings settings_;
    mcp::ToolHost host_;
    mcp::McpHttpServer server_;
};

} // namespace voelund::desktop
