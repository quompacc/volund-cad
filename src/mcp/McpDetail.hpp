#pragma once
#include "JsonReader.hpp"

#include <QJsonObject>

namespace voelund::mcp::detail {

template <class... Visitors> struct Overloaded : Visitors... {
    using Visitors::operator()...;
};

inline constexpr const char* unitNames[] = {"mm", "deg", "rad"}; // Parallel to QuantityUnit.

inline QJsonObject quantity(double value, const char* unit) {
    return {{"value", value}, {"unit", unit}};
}
inline QJsonObject quantity(const application::Quantity& value) {
    return quantity(value.value, unitNames[static_cast<std::size_t>(value.unit)]);
}
// Any known unit; whether it fits the action is the action layer's check (invalidUnit).
inline application::Quantity readQuantity(const JsonReader& reader, const QString& key) {
    const auto object = reader.object(key);
    return {object.number("value"),
            static_cast<application::QuantityUnit>(object.choice("unit", unitNames))};
}
// Sketch values have exactly one unit.
inline double readQuantity(const JsonReader& reader, const QString& key, const char* unit) {
    const auto value = readQuantity(reader, key);
    if (QLatin1String(unitNames[static_cast<std::size_t>(value.unit)]) != QLatin1String(unit))
        throw ArgumentError(reader.field(key) + QStringLiteral(".unit"),
                            std::string("Expected ") + unit);
    return value.value;
}

[[nodiscard]] bool isActionTool(const QString& name);
// T01 (FaceJson.cpp): a face as feature plus role or end-cap boundary, and its frame.
[[nodiscard]] QJsonObject encodeFaceOrigin(const features::SupportOrigin& origin);
[[nodiscard]] QJsonObject encodeSketchFrame(const document::SketchPlane& plane);
[[nodiscard]] QJsonObject encodeFace(const features::SupportFaceFrame& face);
[[nodiscard]] features::SupportOrigin decodeFaceOrigin(const JsonReader& reader);
void decodeSketchContent(const JsonReader& reader, document::SketchParameters& sketch,
                         const document::ParameterTable& parameters);

} // namespace voelund::mcp::detail
