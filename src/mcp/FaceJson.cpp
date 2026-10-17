#include "McpDetail.hpp"

#include <QJsonArray>

#include <algorithm>
#include <cmath>
#include <stdexcept>

// T01: faces of a body as stable semantic references (docs/subshape-reference-contract.md):
// the producing feature plus a box/cylinder role or an extrusion's profile boundary; T04: a
// push/pull has no boundary. Extrusion faces add "cap": "start" (the end cap is the default
// of a boundary, "cap": "end" for a push/pull) or "side": the edge the face was swept from.
// No shape index leaves Völund.
namespace voelund::mcp::detail {
namespace {
// Parallel to kernel::PrimitiveRole; box roles are in the box's own axes.
constexpr const char* roleNames[] = {"-x",    "+x",  "-y",   "+y",     "-z",   "+z",
                                     "start", "end", "side", "sphere", "torus"};
constexpr const char* capNames[] = {"end", "start"}; // parallel to ExtrusionFace
// Parallel to document::ContourRole and ContourStage.
constexpr const char* contourRoles[] = {"line",           "circle",          "arc",
                                        "ellipse",        "rectangleFirst",  "rectangleSecond",
                                        "rectangleThird", "rectangleFourth", "bspline"};
constexpr const char* contourStages[] = {"sketch", "start", "end"};

QJsonObject encodeEdge(const document::EdgeOrigin& edge) {
    if (const auto* primitive = std::get_if<document::PrimitiveEdgeOrigin>(&edge.value))
        return {{"feature", static_cast<double>(primitive->feature.value())},
                {"between", QJsonArray{roleNames[static_cast<std::size_t>(primitive->first)],
                                       roleNames[static_cast<std::size_t>(primitive->second)]}}};
    const auto& contour = std::get<document::ContourEdgeOrigin>(edge.value);
    QJsonObject json{{"sketch", static_cast<double>(contour.sketch.value())},
                     {"element", static_cast<double>(contour.element.value())},
                     {"role", contourRoles[static_cast<std::size_t>(contour.role)]},
                     {"stage", contourStages[static_cast<std::size_t>(contour.stage)]}};
    if (contour.extrusion)
        json.insert("extrusion", static_cast<double>(contour.extrusion->value()));
    return json;
}

document::EdgeOrigin decodeEdge(const JsonReader& reader) {
    if (reader.has("between")) {
        const auto roles = reader.array("between");
        if (roles.size() != 2)
            throw ArgumentError(reader.field("between"), "Expected two face roles");
        const auto role = [&](int index) {
            const auto text = roles.at(index).toString();
            const auto found = std::ranges::find(roleNames, text);
            if (found == std::end(roleNames))
                throw ArgumentError(reader.field("between"), "Unknown face role");
            return static_cast<kernel::PrimitiveRole>(found - std::begin(roleNames));
        };
        return document::PrimitiveEdgeOrigin{document::FeatureId{reader.id("feature")}, role(0),
                                             role(1)};
    }
    const auto extrusion = reader.optionalId("extrusion");
    return document::ContourEdgeOrigin{
        document::FeatureId{reader.id("sketch")}, document::SketchElementId{reader.id("element")},
        static_cast<document::ContourRole>(reader.choice("role", contourRoles)),
        extrusion ? std::optional{document::FeatureId{*extrusion}} : std::nullopt,
        static_cast<document::ContourStage>(reader.choice("stage", contourStages))};
}

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
    if (origin.importedFace) // M17
        json.insert("importedFace", static_cast<double>(*origin.importedFace));
    QJsonArray boundary;
    for (const auto element : origin.extrusionBoundary)
        boundary.append(static_cast<double>(element.value()));
    if (!boundary.isEmpty())
        json.insert("boundary", boundary);
    if (origin.sweptEdge)
        json.insert("side", encodeEdge(*origin.sweptEdge));
    else if (origin.faceExtrusion || origin.extrusionFace == document::ExtrusionFace::start)
        json.insert("cap", capNames[static_cast<std::size_t>(origin.extrusionFace)]);
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
    if (face.secondary) // T05: a second name of a merged face; picks take the first one
        json.insert("secondary", true);
    return json;
}

features::SupportOrigin decodeFaceOrigin(const JsonReader& reader) {
    features::SupportOrigin origin{document::FeatureId{reader.id("feature")}, std::nullopt};
    const bool extrusionFace = reader.has("cap") || reader.has("side");
    if ((reader.has("role") && (reader.has("boundary") || extrusionFace)) ||
        (!reader.has("role") && !reader.has("boundary") && !extrusionFace) ||
        (reader.has("cap") && reader.has("side")))
        throw ArgumentError(reader.field("role"),
                            "A face has a role, a boundary and/or a cap or side, as in "
                            "get_context");
    if (reader.has("cap"))
        origin.extrusionFace = static_cast<document::ExtrusionFace>(reader.choice("cap", capNames));
    if (reader.has("side")) {
        origin.extrusionFace = document::ExtrusionFace::side;
        origin.sweptEdge = decodeEdge(reader.object("side"));
    }
    if (!reader.has("role") && !reader.has("boundary"))
        origin.faceExtrusion = true;
    else if (reader.has("role"))
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
