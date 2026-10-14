#include "JsonReader.hpp"
#include "McpDetail.hpp"

#include <voelund/features/SketchConstraintSolver.hpp>
#include <voelund/kernel/Angle.hpp>

#include <QJsonArray>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>

namespace voelund::mcp {
namespace {
using document::SketchConstraintKind;

constexpr const char* planeNames[] = {"xy", "xz", "yz"};
constexpr const char* pointNames[] = {"start", "end", "center"};
// Parallel to document::SketchConstraintKind.
constexpr const char* kindNames[] = {
    "horizontal",     "vertical",       "distance",      "radius",
    "coincident",     "parallel",       "perpendicular", "coordinateX",
    "coordinateY",    "pointOnObject",  "equal",         "tangent",
    "symmetric",      "pointDistance",  "angle",         "pointDistanceX",
    "pointDistanceY", "midpoint",       "perpendicularBisector",
    "concentric",     "diameter",       "collinear",     "horizontalPoints",
    "verticalPoints", "fixed",          "pointLineDistance"};
// Accepted from clients in tool version 1: dimensions and simple relations.
constexpr SketchConstraintKind acceptedKinds[] = {
    SketchConstraintKind::horizontal,     SketchConstraintKind::vertical,
    SketchConstraintKind::distance,       SketchConstraintKind::radius,
    SketchConstraintKind::coincident,     SketchConstraintKind::parallel,
    SketchConstraintKind::perpendicular,  SketchConstraintKind::coordinateX,
    SketchConstraintKind::coordinateY,    SketchConstraintKind::equal,
    SketchConstraintKind::pointDistance,  SketchConstraintKind::angle,
    SketchConstraintKind::pointDistanceX, SketchConstraintKind::pointDistanceY,
    SketchConstraintKind::concentric,     SketchConstraintKind::diameter};

// The unit of a constraint's value. Coordinates carry a value but cannot be bound
// (document::dimensionUnit lists only the bindable dimensions).
std::optional<document::ParameterUnit> valueUnit(SketchConstraintKind kind) {
    if (kind == SketchConstraintKind::coordinateX || kind == SketchConstraintKind::coordinateY)
        return document::ParameterUnit::length;
    return document::dimensionUnit(kind);
}

QJsonArray point(const document::SketchPoint& value) { return {value.x, value.y}; }
document::SketchPoint point(const detail::JsonReader& reader, const QString& key) {
    const auto [x, y] = reader.point(key);
    return {x, y};
}

QJsonObject encodeElement(const document::SketchElement& element) {
    QJsonObject json{{"id", static_cast<double>(element.id.value())}};
    if (element.construction)
        json.insert("construction", true);
    std::visit(
        detail::Overloaded{
            [&](const document::SketchLine& line) {
                json.insert("type", "line");
                json.insert("start", point(line.start));
                json.insert("end", point(line.end));
            },
            [&](const document::SketchRectangle& rectangle) {
                json.insert("type", "rectangle");
                json.insert("first", point(rectangle.first));
                json.insert("second", point(rectangle.second));
            },
            [&](const document::SketchCircle& circle) {
                json.insert("type", "circle");
                json.insert("center", point(circle.center));
                json.insert("radius", circle.radius);
            },
            [&](const document::SketchArc& arc) {
                json.insert("type", "arc");
                json.insert("center", point(arc.center));
                json.insert("radius", arc.radius);
                json.insert("startAngle", arc.startAngle);
                json.insert("endAngle", arc.endAngle);
            },
            [&](const document::SketchEllipse& ellipse) {
                json.insert("type", "ellipse");
                json.insert("center", point(ellipse.center));
                json.insert("majorRadius", ellipse.majorRadius);
                json.insert("minorRadius", ellipse.minorRadius);
                json.insert("rotation", ellipse.rotation);
            },
            [&](const document::SketchPointGeometry& value) { // S15
                json.insert("type", "point");
                json.insert("position", point(value.position));
            },
            [&](const document::SketchBSpline& spline) { // S21: output only
                json.insert("type", "bspline");
                QJsonArray poles;
                for (const auto& pole : spline.poles)
                    poles.append(point(pole));
                json.insert("poles", poles);
            }},
        element.geometry);
    return json;
}

document::SketchElement decodeElement(const detail::JsonReader& reader) {
    constexpr const char* types[] = {"line", "rectangle", "circle", "arc"};
    const document::SketchElementId id{reader.id("id")};
    const bool construction = reader.boolean("construction", false);
    switch (reader.choice("type", types)) {
    case 0:
        return {id, document::SketchLine{point(reader, "start"), point(reader, "end")},
                construction};
    case 1: {
        const auto first = point(reader, "first");
        const auto second = point(reader, "second");
        return {id, document::SketchRectangle{first.x, first.y, second.x, second.y},
                construction};
    }
    case 2:
        return {id, document::SketchCircle{point(reader, "center"), reader.number("radius")},
                construction};
    default:
        return {id,
                document::SketchArc{point(reader, "center"), reader.number("radius"),
                                    reader.number("startAngle"), reader.number("endAngle")},
                construction};
    }
}

QJsonObject encodeReference(const document::SketchGeometryReference& reference) {
    return {{"element", static_cast<double>(reference.element.value())},
            {"point", pointNames[static_cast<std::size_t>(reference.point)]}};
}
document::SketchGeometryReference decodeReference(const detail::JsonReader& reader) {
    return {document::SketchElementId{reader.id("element")},
            static_cast<document::SketchPointPosition>(
                reader.has("point") ? reader.choice("point", pointNames) : 0)};
}

QJsonObject encodeConstraint(const document::SketchConstraint& constraint) {
    QJsonObject json{{"id", static_cast<double>(constraint.id.value())},
                     {"kind", kindNames[static_cast<std::size_t>(constraint.kind)]},
                     {"first", encodeReference(constraint.first)}};
    if (constraint.second)
        json.insert("second", encodeReference(*constraint.second));
    if (constraint.third)
        json.insert("third", encodeReference(*constraint.third));
    if (const auto unit = valueUnit(constraint.kind)) {
        json.insert("value", *unit == document::ParameterUnit::angle
                                 ? detail::quantity(constraint.angleValue.value(), "rad")
                                 : detail::quantity(constraint.value, "mm"));
    }
    if (constraint.parameter)
        json.insert("parameterId", static_cast<double>(constraint.parameter->value()));
    if (constraint.reference)
        json.insert("reference", true);
    return json;
}

document::SketchConstraint decodeConstraint(const detail::JsonReader& reader,
                                            const document::ParameterTable& parameters) {
    const auto kind = static_cast<SketchConstraintKind>(reader.choice("kind", kindNames));
    if (std::ranges::find(acceptedKinds, kind) == std::end(acceptedKinds))
        throw ArgumentError(reader.field("kind"), "Not supported in tool version 1");
    const document::SketchConstraintId id{reader.id("id")};
    const auto first = decodeReference(reader.object("first"));
    const auto second = reader.has("second")
                            ? std::optional{decodeReference(reader.object("second"))}
                            : std::nullopt;
    const auto unit = valueUnit(kind);
    if (!unit) {
        if (reader.has("value") || reader.has("parameterId") || reader.has("reference"))
            throw ArgumentError(reader.field("value"), "This constraint has no value");
        return {id, kind, first, second};
    }
    // A bound dimension mirrors its parameter (PAR01); angles are stored in radians.
    std::optional<document::ParameterId> bound;
    std::optional<double> value;
    if (const auto parameterId = reader.optionalId("parameterId")) {
        if (!document::dimensionUnit(kind))
            throw ArgumentError(reader.field("parameterId"), "Coordinates cannot be bound");
        bound = document::ParameterId{*parameterId};
        const auto* parameter = document::findParameter(parameters, *bound);
        if (parameter == nullptr)
            throw ArgumentError(reader.field("parameterId"), "Unknown parameter");
        if (parameter->unit != *unit)
            throw ArgumentError(reader.field("parameterId"), "The parameter has the wrong unit");
        value = *unit == document::ParameterUnit::angle
                    ? kernel::toRadians(kernel::Degrees(parameter->value)).value()
                    : parameter->value;
    }
    if (reader.has("value")) {
        const double given = detail::readQuantity(
            reader, "value", *unit == document::ParameterUnit::angle ? "rad" : "mm");
        if (value && std::abs(*value - given) > 1e-9 * std::max(1.0, std::abs(given)))
            throw ArgumentError(reader.field("value"), "The value differs from the parameter");
        value = given;
    }
    // S18: a reference shows what the geometry measures; its value may be left out.
    const bool reference = reader.boolean("reference", false);
    if (reference && bound)
        throw ArgumentError(reader.field("reference"), "A reference cannot be bound");
    if (!value && reference)
        value = 0.0;
    if (!value)
        throw ArgumentError(reader.field("value"), "A dimension needs a value or a parameter");
    auto constraint = *unit == document::ParameterUnit::angle
                          ? document::SketchConstraint{id, kind, first, second,
                                                       kernel::Radians(*value)}
                          : document::SketchConstraint{id, kind, first, second, *value};
    constraint.parameter = bound;
    constraint.reference = reference;
    return constraint;
}

std::uint64_t nextAfter(std::uint64_t maximum) { return maximum + 1; }
} // namespace

QJsonObject encodeSketch(const document::SketchParameters& sketch) {
    QJsonArray elements;
    for (const auto& element : sketch.elements.values)
        elements.append(encodeElement(element));
    QJsonArray constraints;
    for (const auto& constraint : sketch.constraints.values)
        constraints.append(encodeConstraint(constraint));
    QJsonObject json{{"elements", elements}, {"constraints", constraints}};
    // S12: associative patterns, read-only here: their instances follow the sources and carry
    // no constraints; an edit must keep every instance element.
    if (!sketch.patterns.values.empty()) {
        QJsonArray patterns;
        const auto ids = [](const std::vector<document::SketchElementId>& values) {
            QJsonArray result;
            for (const auto id : values)
                result.append(static_cast<double>(id.value()));
            return result;
        };
        for (const auto& pattern : sketch.patterns.values) {
            QJsonObject entry{{"kind", pattern.kind == document::SketchPatternKind::linear
                                           ? "linear"
                                       : pattern.kind == document::SketchPatternKind::circular
                                           ? "circular"
                                           : "mirror"},
                              {"count", static_cast<int>(pattern.count)},
                              {"sources", ids(pattern.sources)},
                              {"instances", ids(pattern.instances)}};
            if (pattern.kind == document::SketchPatternKind::linear)
                entry.insert("step", point(pattern.step));
            if (pattern.kind == document::SketchPatternKind::circular) {
                entry.insert("center", point(pattern.center));
                entry.insert("angle_deg",
                             kernel::toDegrees(kernel::Radians(pattern.angle)).value());
            }
            if (pattern.axis)
                entry.insert("axis", static_cast<double>(pattern.axis->value()));
            patterns.append(entry);
        }
        json.insert("patterns", patterns);
    }
    // Sketches on body faces have no base plane; their plane follows the support.
    json.insert("plane", sketch.datum ? QJsonValue(planeNames[static_cast<int>(*sketch.datum) - 1])
                                      : QJsonValue(QStringLiteral("face")));
    return json;
}

namespace detail {
void decodeSketchContent(const JsonReader& reader, document::SketchParameters& sketch,
                         const document::ParameterTable& parameters) {
    sketch.elements = {};
    sketch.constraints = {};
    std::uint64_t maximum = 0;
    const auto elements = reader.array("elements");
    for (qsizetype index = 0; index < elements.size(); ++index) {
        auto element = decodeElement(reader.element("elements", index));
        maximum = std::max(maximum, element.id.value());
        sketch.elements.values.push_back(std::move(element));
    }
    sketch.elements.nextId = nextAfter(maximum);
    maximum = 0;
    if (reader.has("constraints")) {
        const auto constraints = reader.array("constraints");
        for (qsizetype index = 0; index < constraints.size(); ++index) {
            auto constraint = decodeConstraint(reader.element("constraints", index), parameters);
            maximum = std::max(maximum, constraint.id.value());
            sketch.constraints.values.push_back(std::move(constraint));
        }
    }
    sketch.constraints.nextId = nextAfter(maximum);
    if (sketch.constraints.values.empty())
        return;
    // The client may give approximate geometry; the dimensions decide.
    const auto solved = features::solveSketchConstraints(sketch);
    if (solved.status != features::SketchSolveStatus::success) {
        std::string conflicts;
        for (const auto& id : solved.conflictingConstraints)
            conflicts += (conflicts.empty() ? " " : ", ") + std::to_string(id.value());
        throw ArgumentError(reader.field("constraints"),
                            "The sketch does not solve" +
                                (conflicts.empty() ? std::string{}
                                                   : "; conflicting constraints:" + conflicts));
    }
}
} // namespace detail

document::SketchParameters decodeSketch(const QJsonObject& json, const QString& field,
                                        const document::ParameterTable& parameters) {
    const detail::JsonReader reader(json, field);
    const auto datum = static_cast<document::DatumPlane>(reader.choice("plane", planeNames) + 1);
    document::SketchParameters sketch{document::datumSketchPlane(datum), {}};
    sketch.datum = datum;
    detail::decodeSketchContent(reader, sketch, parameters);
    return sketch;
}

} // namespace voelund::mcp
