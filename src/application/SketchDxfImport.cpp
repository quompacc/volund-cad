// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchDxfImport.hpp"
#include "SketchCurveEditCuts.hpp"

#include <voelund/kernel/PlanarCurve.hpp>

#include <charconv>
#include <cmath>
#include <functional>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <utility>

namespace voelund::application::detail {
namespace {

using document::SketchPoint;
constexpr double turn = 2.0 * std::numbers::pi;

struct Group final {
    int code;
    std::string value;
};
struct Entity final {
    std::string type;
    std::vector<Group> groups;
    std::vector<Entity> vertices; // a POLYLINE's VERTEX entities
};

std::string_view trimmed(std::string_view text) {
    while (!text.empty() && (text.back() == '\r' || text.back() == ' ' || text.back() == '\t'))
        text.remove_suffix(1);
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t'))
        text.remove_prefix(1);
    return text;
}

std::vector<Group> groupsOf(std::string_view text) {
    std::vector<std::string_view> lines;
    for (std::size_t start = 0; start <= text.size();) {
        const auto end = std::min(text.find('\n', start), text.size());
        lines.push_back(trimmed(text.substr(start, end - start)));
        start = end + 1;
    }
    std::vector<Group> groups;
    for (std::size_t index = 0; index + 1 < lines.size(); index += 2) {
        int code = 0;
        const auto line = lines[index];
        if (std::from_chars(line.data(), line.data() + line.size(), code).ec != std::errc{})
            throw std::invalid_argument("DXF: Die Datei ist keine lesbare ASCII-DXF-Datei");
        groups.push_back({code, std::string(lines[index + 1])});
    }
    return groups;
}

double number(std::string_view text) {
    double value = 0.0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || !std::isfinite(value))
        throw std::invalid_argument("DXF: Eine Zahl ist nicht lesbar: " + std::string(text));
    return value;
}

// Millimetres per drawing unit ($INSUNITS); none or unitless counts as millimetres.
double unitScale(const std::vector<Group>& groups) {
    for (std::size_t index = 0; index + 1 < groups.size(); ++index)
        if (groups[index].code == 9 && groups[index].value == "$INSUNITS") {
            const int units = static_cast<int>(number(groups[index + 1].value));
            static const std::map<int, double> scales{{1, 25.4}, {2, 304.8},  {4, 1.0},
                                                      {5, 10.0}, {6, 1000.0}, {14, 100.0}};
            const auto scale = scales.find(units);
            return scale == scales.end() ? 1.0 : scale->second;
        }
    return 1.0;
}

// The groups between "SECTION / ENTITIES" and its "ENDSEC".
std::pair<std::size_t, std::size_t> entitiesSection(const std::vector<Group>& groups) {
    for (std::size_t index = 1; index < groups.size(); ++index)
        if (groups[index].code == 2 && groups[index].value == "ENTITIES" &&
            groups[index - 1].value == "SECTION") {
            std::size_t end = index + 1;
            while (end < groups.size() && !(groups[end].code == 0 && groups[end].value == "ENDSEC"))
                ++end;
            return {index + 1, end};
        }
    return {0, 0};
}

// A group of the entity being read: "0" starts the next (a VERTEX belongs to its POLYLINE,
// SEQEND ends it), any other goes to the current entity or vertex.
void take(std::vector<Entity>& entities, const Group& group) {
    if (group.code == 0) {
        if (group.value == "VERTEX" && !entities.empty())
            entities.back().vertices.push_back({group.value, {}, {}});
        else if (group.value != "SEQEND")
            entities.push_back({group.value, {}, {}});
        return;
    }
    if (entities.empty())
        return;
    auto& entity = entities.back();
    (entity.vertices.empty() ? entity.groups : entity.vertices.back().groups).push_back(group);
}

std::vector<Entity> entitiesOf(const std::vector<Group>& groups) {
    std::vector<Entity> entities;
    const auto [begin, end] = entitiesSection(groups);
    for (std::size_t index = begin; index < end; ++index)
        take(entities, groups[index]);
    return entities;
}

double value(const Entity& entity, int code, double fallback) {
    for (const auto& group : entity.groups)
        if (group.code == code)
            return number(group.value);
    return fallback;
}
std::vector<double> values(const Entity& entity, int code) {
    std::vector<double> result;
    for (const auto& group : entity.groups)
        if (group.code == code)
            result.push_back(number(group.value));
    return result;
}

// How one entity is read: its scale, whether it is seen from below, its layer.
struct Reading final {
    const Entity& entity;
    double scale;
    bool mirrored; // extrusion 0 0 -1: x turns over
    bool construction;
    std::vector<DxfCurve>& out;

    [[nodiscard]] SketchPoint point(double x, double y) const {
        return {(mirrored ? -x : x) * scale, y * scale};
    }
    [[nodiscard]] SketchPoint at(int xCode) const {
        return point(value(entity, xCode, 0.0), value(entity, xCode + 10, 0.0));
    }
    void add(document::SketchGeometry geometry) const {
        out.push_back({std::move(geometry), construction});
    }
};

double distance(SketchPoint a, SketchPoint b) { return std::hypot(a.x - b.x, a.y - b.y); }

// The arc of a polyline segment with `bulge` (tan of a quarter of its angle; positive turns
// counter-clockwise from `a` to `b`).
document::SketchArc bulgeArc(SketchPoint a, SketchPoint b, double bulge) {
    if (bulge < 0.0) {
        std::swap(a, b);
        bulge = -bulge;
    }
    const double chord = distance(a, b);
    const double sagitta = bulge * chord / 2.0;
    const double radius = (chord * chord / 4.0 + sagitta * sagitta) / (2.0 * sagitta);
    const SketchPoint middle{(a.x + b.x) / 2.0, (a.y + b.y) / 2.0};
    const SketchPoint left{-(b.y - a.y) / chord, (b.x - a.x) / chord};
    const SketchPoint center{middle.x + left.x * (radius - sagitta),
                             middle.y + left.y * (radius - sagitta)};
    const double start = std::atan2(a.y - center.y, a.x - center.x);
    double end = std::atan2(b.y - center.y, b.x - center.x);
    while (end <= start)
        end += turn;
    return {center, radius, start, end};
}

struct Vertex final {
    SketchPoint point;
    double bulge;
};
void addPolyline(const Reading& reading, const std::vector<Vertex>& vertices, bool closed) {
    const std::size_t count = vertices.size();
    for (std::size_t index = 0; index + (closed ? 0 : 1) < count; ++index) {
        const auto& from = vertices[index];
        const auto& to = vertices[(index + 1) % count];
        if (distance(from.point, to.point) < 1e-9)
            continue;
        if (std::abs(from.bulge) < 1e-12)
            reading.add(document::SketchLine{from.point, to.point});
        else
            reading.add(
                bulgeArc(from.point, to.point, reading.mirrored ? -from.bulge : from.bulge));
    }
}

bool readLine(const Reading& r) {
    r.add(document::SketchLine{r.at(10), r.at(11)});
    return true;
}
bool readCircle(const Reading& r) {
    r.add(document::SketchCircle{r.at(10), value(r.entity, 40, 0.0) * r.scale});
    return true;
}
bool readArc(const Reading& r) {
    const double pi = std::numbers::pi;
    double start = value(r.entity, 50, 0.0) * pi / 180.0;
    double end = value(r.entity, 51, 360.0) * pi / 180.0;
    if (r.mirrored)
        std::tie(start, end) = std::pair{pi - end, pi - start};
    while (end <= start)
        end += turn;
    r.add(document::SketchArc{r.at(10), value(r.entity, 40, 0.0) * r.scale, start, end});
    return true;
}
bool readPoint(const Reading& r) {
    r.out.push_back({document::SketchPointGeometry{r.at(10)}, true});
    return true;
}
bool readLightPolyline(const Reading& r) {
    std::vector<Vertex> vertices;
    double x = 0.0;
    for (const auto& group : r.entity.groups) {
        if (group.code == 10)
            x = number(group.value);
        else if (group.code == 20)
            vertices.push_back({r.point(x, number(group.value)), 0.0});
        else if (group.code == 42 && !vertices.empty())
            vertices.back().bulge = number(group.value);
    }
    addPolyline(r, vertices, (static_cast<int>(value(r.entity, 70, 0.0)) & 1) != 0);
    return true;
}
bool readPolyline(const Reading& r) {
    std::vector<Vertex> vertices;
    for (const auto& vertex : r.entity.vertices)
        vertices.push_back(
            {r.point(value(vertex, 10, 0.0), value(vertex, 20, 0.0)), value(vertex, 42, 0.0)});
    addPolyline(r, vertices, (static_cast<int>(value(r.entity, 70, 0.0)) & 1) != 0);
    return true;
}
bool readEllipse(const Reading& r) {
    if (r.mirrored)
        return false;
    const SketchPoint major{value(r.entity, 11, 0.0) * r.scale, value(r.entity, 21, 0.0) * r.scale};
    const double a = std::hypot(major.x, major.y);
    const document::SketchEllipse ellipse{r.at(10), a, a * value(r.entity, 40, 1.0),
                                          std::atan2(major.y, major.x)};
    const double start = value(r.entity, 41, 0.0);
    double end = value(r.entity, 42, turn);
    if (end - start >= turn - 1e-9) {
        r.add(ellipse);
        return true;
    }
    while (end <= start)
        end += turn;
    r.add(document::SketchEllipticArc{ellipse.center, ellipse.majorRadius, ellipse.minorRadius,
                                      ellipse.rotation, start, end});
    return true;
}
bool readSpline(const Reading& r) {
    const auto weights = values(r.entity, 41);
    const auto xs = values(r.entity, 10), ys = values(r.entity, 20);
    if (r.mirrored || xs.empty() || xs.size() != ys.size() ||
        std::ranges::any_of(weights, [](double w) { return std::abs(w - 1.0) > 1e-12; }))
        return false;
    std::vector<gp_Pnt2d> poles;
    for (std::size_t index = 0; index < xs.size(); ++index) {
        const auto p = r.point(xs[index], ys[index]);
        poles.emplace_back(p.x, p.y);
    }
    const auto spline = kernel::clampedCubicBSpline(poles, values(r.entity, 40),
                                                    static_cast<int>(value(r.entity, 71, 3.0)));
    if (!spline || spline->poles.size() > document::maximumBSplinePoles)
        return false;
    const auto curve = sketchSpline(*spline);
    if (distance(curve.poles.front(), curve.poles.back()) > 1e-9) {
        r.add(curve);
        return true;
    }
    // A closed spline in two halves: a sketch spline has two distinct ends.
    const double middle = spline->knots.back() / 2.0;
    r.add(sketchSpline(kernel::bsplinePiece(*spline, 0.0, middle)));
    r.add(sketchSpline(kernel::bsplinePiece(*spline, middle, spline->knots.back())));
    return true;
}

using Reader = std::function<bool(const Reading&)>;
const std::map<std::string, Reader>& readers() {
    static const std::map<std::string, Reader> table{{"LINE", readLine},
                                                     {"CIRCLE", readCircle},
                                                     {"ARC", readArc},
                                                     {"POINT", readPoint},
                                                     {"LWPOLYLINE", readLightPolyline},
                                                     {"POLYLINE", readPolyline},
                                                     {"ELLIPSE", readEllipse},
                                                     {"SPLINE", readSpline}};
    return table;
}

std::string layerOf(const Entity& entity) {
    for (const auto& group : entity.groups)
        if (group.code == 8)
            return group.value;
    return {};
}

} // namespace

DxfDrawing readDxfDrawing(std::string_view text) {
    const auto groups = groupsOf(text);
    const double scale = unitScale(groups);
    DxfDrawing drawing;
    for (const auto& entity : entitiesOf(groups)) {
        const auto reader = readers().find(entity.type);
        const double normal = value(entity, 230, 1.0);
        const bool flat = std::abs(std::abs(normal) - 1.0) < 1e-9;
        const Reading reading{entity, scale, normal < 0.0, layerOf(entity) == "Konstruktion",
                              drawing.curves};
        if (reader == readers().end() || !flat || !reader->second(reading))
            ++drawing.skipped[entity.type];
    }
    return drawing;
}

} // namespace voelund::application::detail
