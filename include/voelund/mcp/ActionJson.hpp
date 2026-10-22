// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once
#include <voelund/application/Actions.hpp>

#include <QJsonObject>
#include <QString>

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// AI04a (docs/implementation-plan/AI04-plan.md): JSON form of the action layer for MCP.
// Tool version 1. Quantities are {"value": number, "unit": "mm" | "deg" | "rad"}; sketch
// coordinates are plain millimetres and arc angles plain radians, as in the document.
namespace voelund::mcp {

inline constexpr int toolVersion = 1;

struct ToolDefinition final {
    QString name;
    QString description;
    QJsonObject inputSchema;
};
// get_context and one tool per action; the order is stable.
[[nodiscard]] const std::vector<ToolDefinition>& toolCatalog();

// Malformed arguments: names the offending field ("sketch.elements[2].radius").
class ArgumentError final : public std::runtime_error {
  public:
    ArgumentError(QString field, const std::string& message)
        : std::runtime_error(message), field_(std::move(field)) {}
    [[nodiscard]] const QString& field() const noexcept { return field_; }

  private:
    QString field_;
};

struct DecodedCall final {
    application::ActionRequest request;
    bool preview;
};
// Throws ArgumentError, or std::invalid_argument for an unknown or read-only tool. Bound
// sketch dimensions without a value take the parameter's value. A sketch with constraints
// must solve; its solved geometry is what the request carries. edit_sketch keeps plane,
// base plane and support binding of the existing sketch.
[[nodiscard]] DecodedCall decodeActionCall(const QString& tool, const QJsonObject& arguments,
                                           const application::CadSession& session);

[[nodiscard]] QJsonObject encodeResult(const application::ActionResult& result, bool preview);
// describeActionContext plus the parameters of every feature, sketches in the same form
// the tools accept.
[[nodiscard]] QJsonObject encodeContext(const application::CadSession& session);

[[nodiscard]] QJsonObject encodeSketch(const document::SketchParameters& sketch);
[[nodiscard]] document::SketchParameters decodeSketch(const QJsonObject& sketch,
                                                      const QString& field,
                                                      const document::ParameterTable& parameters);

} // namespace voelund::mcp
