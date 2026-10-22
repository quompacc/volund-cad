// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/mcp/ActionJson.hpp>
#include <voelund/mcp/McpServer.hpp>

#include <QJsonArray>

#include <stdexcept>

namespace voelund::mcp {
namespace {
constexpr int invalidRequest = -32600;
constexpr int methodNotFound = -32601;
constexpr int invalidParams = -32602;
constexpr int internalError = -32603;

QJsonObject error(const QJsonValue& id, int code, const QString& message) {
    return {{"jsonrpc", "2.0"},
            {"id", id},
            {"error", QJsonObject{{"code", code}, {"message", message}}}};
}
QJsonObject success(const QJsonValue& id, const QJsonObject& result) {
    return {{"jsonrpc", "2.0"}, {"id", id}, {"result", result}};
}

const QString instructions = QStringLiteral(
    "Völund CAD document tools. Call get_context first: it returns documentId and stamp for "
    "every request, the features with their parameters and the tools available per target. "
    "Lengths are mm, sketch angles rad, angle parameters deg. Give every intended change its "
    "own requestId; retry with the same requestId and content after a lost answer, it is "
    "never applied twice. After a staleStamp rejection call get_context again. Every change "
    "is one undo step for the user. A sketch on a base plane starts a new body; to add to a "
    "body (e.g. a rib on a plate), sketch on one of its faces with create_sketch_on_face, "
    "and the extrusion joins that body. For a pocket or a hole, extrude with operation cut "
    "and direction reversed (into the body). To make a body thicker or longer, or to push a "
    "face in, use push_pull_face on that face.");
} // namespace

const QStringList& McpProtocol::supportedVersions() {
    static const QStringList versions{"2025-11-25", "2025-06-18", "2025-03-26"};
    return versions;
}

std::optional<QJsonObject> McpProtocol::handle(const QJsonObject& message) {
    const bool request = message.contains("id");
    const auto id = message.value("id");
    if (!message.contains("method"))
        return request ? std::optional{error(id, invalidRequest, "Missing method")}
                       : std::nullopt; // A response of the client; nothing was asked.
    if (message.value("jsonrpc") != QLatin1String("2.0") || !message.value("method").isString())
        return error(request ? id : QJsonValue(), invalidRequest, "Not a JSON-RPC 2.0 request");
    if (!request)
        return std::nullopt; // notifications/initialized, notifications/cancelled, …
    const auto method = message.value("method").toString();
    const auto params = message.value("params").toObject();

    if (method == QLatin1String("initialize")) {
        const auto requested = params.value("protocolVersion").toString();
        const auto version =
            supportedVersions().contains(requested) ? requested : supportedVersions().front();
        return success(
            id,
            {{"protocolVersion", version},
             {"capabilities", QJsonObject{{"tools", QJsonObject{{"listChanged", false}}}}},
             {"serverInfo", QJsonObject{{"name", "volund-cad"},
                                        {"title", "Völund CAD"},
                                        {"version", QStringLiteral("tools-%1").arg(toolVersion)}}},
             {"instructions", instructions}});
    }
    if (method == QLatin1String("ping"))
        return success(id, {});
    if (method == QLatin1String("tools/list")) {
        QJsonArray tools;
        for (const auto& tool : toolCatalog())
            tools.append(QJsonObject{{"name", tool.name},
                                     {"description", tool.description},
                                     {"inputSchema", tool.inputSchema}});
        return success(id, {{"tools", tools}});
    }
    if (method == QLatin1String("tools/call")) {
        const auto name = params.value("name").toString();
        if (!host_.hasTool(name))
            return error(id, invalidParams, "Unknown tool: " + name);
        const auto arguments = params.value("arguments");
        if (!arguments.isUndefined() && !arguments.isNull() && !arguments.isObject())
            return error(id, invalidParams, "Tool arguments must be an object");
        try {
            return success(id, host_.callTool(name, arguments.toObject()));
        } catch (const std::exception& failure) {
            // Unexpected failures surface as protocol errors.
            return error(id, internalError, QString::fromUtf8(failure.what()));
        }
    }
    return error(id, methodNotFound, "Unknown method: " + method);
}

} // namespace voelund::mcp
