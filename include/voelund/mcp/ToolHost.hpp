// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once
#include <voelund/application/Actions.hpp>

#include <QJsonObject>
#include <QString>

#include <functional>
#include <utility>

// AI04a: answers MCP tool calls for one session, independent of the transport. Mutations
// run through one ActionDispatcher, so a retried requestId never mutates twice, also
// across reconnects of the client.
namespace voelund::mcp {

class ToolHost final {
  public:
    using ChangeHandler = std::function<void(const application::ActionResult&)>;
    // `changed` runs after every published mutation (not after previews or repetitions).
    explicit ToolHost(application::CadSession& session, ChangeHandler changed = {});
    void setChangeHandler(ChangeHandler changed) { changed_ = std::move(changed); }
    [[nodiscard]] bool hasTool(const QString& name) const;
    // An MCP CallToolResult: content (text), structuredContent and isError. Argument and
    // action rejections are tool errors (isError) without any mutation.
    [[nodiscard]] QJsonObject callTool(const QString& name, const QJsonObject& arguments);

  private:
    application::CadSession& session_;
    application::ActionDispatcher dispatcher_;
    ChangeHandler changed_;
};

} // namespace voelund::mcp
