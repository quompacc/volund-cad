#pragma once
#include <voelund/application/Actions.hpp>

#include <QJsonObject>
#include <QString>

#include <functional>

// AI04a: answers MCP tool calls for one session, independent of the transport. Mutations
// run through one ActionDispatcher, so a retried requestId never mutates twice, also
// across reconnects of the client.
namespace voelund::mcp {

class ToolHost final {
  public:
    // `changed` runs after every published mutation (not after previews or repetitions).
    explicit ToolHost(application::CadSession& session, std::function<void()> changed = {});
    [[nodiscard]] bool hasTool(const QString& name) const;
    // An MCP CallToolResult: content (text), structuredContent and isError. Argument and
    // action rejections are tool errors (isError) without any mutation.
    [[nodiscard]] QJsonObject callTool(const QString& name, const QJsonObject& arguments);

  private:
    application::CadSession& session_;
    application::ActionDispatcher dispatcher_;
    std::function<void()> changed_;
};

} // namespace voelund::mcp
