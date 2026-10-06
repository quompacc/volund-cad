#include "McpDetail.hpp"

#include <QJsonArray>

#include <algorithm>
#include <cmath>
#include <stdexcept>

// T01: faces of a body as stable semantic references (docs/subshape-reference-contract.md):
// the producing feature plus a box/cylinder role or an extrusion's end-cap boundary. No
// shape index leaves Völund.
namespace voelund::mcp::detail {
namespace {
// Parallel to kernel::PrimitiveRole; box roles are in the box's own axes.
constexpr const char* roleNames[] = {"-x", "+x", "-y", "+y", "-z", "+z", "start", "end", "side"};

// Clean output: 1e-9 mm is far below any modelling tolerance.
double clean(double value) {
    const double rounded = std::round(value * 1e9) / 1e9;
    return rounded == 0.0 ? 0.0 : rounded;
}
QJsonArray vector(double x, double y, double z) { return {clean(x), clean(y), clean(z)}; }
} // namespace

QJsonObject encodeFaceOrigin(const features::SupportOrigin& origin) {
    QJsonObject json{{"feature", static_cast<double>(origin.feature.value())}};
    if (origin.role)
        json.insert("role", roleNames[static_cast<std::size_t>(*origin.role)]);
    QJsonArray boundary;
    for (const auto element : origin.extrusionBoundary)
        boundary.append(static_cast<double>(element.value()));
    if (!boundary.isEmpty())
        json.insert("boundary", boundary);
    return json;
}

QJsonObject encodeSketchFrame(const document::SketchPlane& plane) {
    const double yX = plane.normalY * plane.xDirectionZ - plane.normalZ * plane.xDirectionY;
    const double yY = plane.normalZ * plane.xDirectionX - plane.normalX * plane.xDirectionZ;
    const double yZ = plane.normalX * plane.xDirectionY - plane.normalY * plane.xDirectionX;
    return {{"origin", vector(plane.originX, plane.originY, plane.originZ)},
            {"normal", vector(plane.normalX, plane.normalY, plane.normalZ)},
            {"xDirection", vector(plane.xDirectionX, plane.xDirectionY, plane.xDirectionZ)},
            {"yDirection", vector(yX, yY, yZ)}};
}

QJsonObject encodeFace(const features::SupportFaceFrame& face) {
    auto json = encodeSketchFrame(face.plane);
    json.insert("face", encodeFaceOrigin(face.origin));
    json.insert("min", QJsonArray{clean(face.minX), clean(face.minY)});
    json.insert("max", QJsonArray{clean(face.maxX), clean(face.maxY)});
    return json;
}

features::SupportOrigin decodeFaceOrigin(const JsonReader& reader) {
    features::SupportOrigin origin{document::FeatureId{reader.id("feature")}, std::nullopt};
    if (reader.has("role") == reader.has("boundary"))
        throw ArgumentError(reader.field("role"),
                            "A face has either a role or a boundary, as in get_context");
    if (reader.has("role"))
        origin.role = static_cast<kernel::PrimitiveRole>(reader.choice("role", roleNames));
    else {
        const auto values = reader.array("boundary");
        for (qsizetype index = 0; index < values.size(); ++index) {
            const double value = values.at(index).toDouble(-1.0);
            if (!(value >= 1.0 && value <= 9007199254740992.0) || std::floor(value) != value)
                throw ArgumentError(reader.field("boundary") + QStringLiteral("[%1]").arg(index),
                                    "Expected a sketch element id");
            origin.extrusionBoundary.push_back(
                document::SketchElementId{static_cast<std::uint64_t>(value)});
        }
        std::ranges::sort(origin.extrusionBoundary, {}, &document::SketchElementId::value);
    }
    try {
        document::validateSubshapeOrigin(origin);
    } catch (const std::invalid_argument& error) {
        throw ArgumentError(reader.field(reader.has("role") ? "role" : "boundary"), error.what());
    }
    return origin;
}

} // namespace voelund::mcp::detail
