#pragma once
#include <voelund/mcp/McpServer.hpp>
#include <voelund/mcp/ToolHost.hpp>

#include <QByteArray>
#include <QString>

namespace voelund::application {
class CadSession;
}

// AI04c (docs/implementation-plan/AI04-plan.md): the local MCP server of a running Völund.
// Tool calls run on the GUI thread; after each published change the window refreshes.
namespace voelund::desktop {

class MainWindow;

struct McpSettings final {
    quint16 port;
    QByteArray token;
};
// Port and token persist in the user settings, so a client configured once keeps working.
// A missing token is generated (32 random bytes, hex).
[[nodiscard]] McpSettings loadMcpSettings();

class McpAccess final {
  public:
    static constexpr quint16 defaultPort = 47913;

    McpAccess(application::CadSession& session, MainWindow& window, const McpSettings& settings);
    [[nodiscard]] bool start();
    void stop();
    [[nodiscard]] bool isRunning() const;
    [[nodiscard]] QString url() const;
    // The command that connects Claude Code to this server.
    [[nodiscard]] QString claudeCommand() const;

  private:
    McpSettings settings_;
    mcp::ToolHost host_;
    mcp::McpHttpServer server_;
};

} // namespace voelund::desktop
