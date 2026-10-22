// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "McpDetail.hpp"

#include <voelund/mcp/ToolHost.hpp>

#include <QJsonArray>
#include <QJsonDocument>

#include <optional>
#include <stdexcept>
#include <utility>

namespace voelund::mcp {
namespace {
// Structured result plus the same JSON as text for clients without structured content.
QJsonObject toolResult(const QJsonObject& structured, bool error) {
    const auto text = QString::fromUtf8(QJsonDocument(structured).toJson(QJsonDocument::Compact));
    return {{"content", QJsonArray{QJsonObject{{"type", "text"}, {"text", text}}}},
            {"structuredContent", structured},
            {"isError", error}};
}
QJsonObject argumentError(const QString& field, const char* message) {
    return toolResult({{"toolVersion", toolVersion},
                       {"error", "invalidArguments"},
                       {"field", field},
                       {"message", QString::fromUtf8(message)}},
                      true);
}
} // namespace

ToolHost::ToolHost(application::CadSession& session, ChangeHandler changed)
    : session_(session), dispatcher_(session), changed_(std::move(changed)) {}

bool ToolHost::hasTool(const QString& name) const {
    return name == QLatin1String("get_context") || detail::isActionTool(name);
}

QJsonObject ToolHost::callTool(const QString& name, const QJsonObject& arguments) {
    if (name == QLatin1String("get_context"))
        return toolResult(encodeContext(session_), false);
    if (!detail::isActionTool(name))
        throw std::invalid_argument("Unknown tool: " + name.toStdString());
    std::optional<DecodedCall> call;
    try {
        call = decodeActionCall(name, arguments, session_);
    } catch (const ArgumentError& error) {
        return argumentError(error.field(), error.what());
    } catch (const std::invalid_argument& error) { // Document validation of a sketch draft.
        return argumentError({}, error.what());
    } catch (const std::out_of_range& error) {
        return argumentError({}, error.what());
    }
    const auto result = call->preview ? dispatcher_.preview(call->request).result
                                      : dispatcher_.execute(call->request);
    const bool published =
        result.command.status() != application::CommandStatus::rejected && !result.repeated;
    if (!call->preview && published && changed_)
        changed_(result);
    return toolResult(encodeResult(result, call->preview),
                      result.command.status() == application::CommandStatus::rejected);
}

} // namespace voelund::mcp
