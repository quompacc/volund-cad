#include "SketchOffset.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>
#include <stdexcept>

namespace voelund::application::detail {
namespace {

using document::SketchArc;
using document::SketchLine;
using document::SketchPoint;
using Position = document::SketchPointPosition;

constexpr double tolerance = 1e-9;

[[noreturn]] void refuse(const char* reason) {
    throw std::invalid_argument(std::string("Versatz: ") + reason);
}

double distance(SketchPoint a, SketchPoint b) { return std::hypot(a.x - b.x, a.y - b.y); }
SketchPoint onArc(const SketchArc& arc, double angle) {
    return {arc.center.x + arc.radius * std::cos(angle),
            arc.center.y + arc.radius * std::sin(angle)};
}

// One element of the contour as it is traversed; `reversed` runs against its stored order.
struct Segment final {
    document::SketchGeometry geometry;
    bool reversed{false};

    [[nodiscard]] SketchPoint stored(bool end) const {
        if (const auto* line = std::get_if<SketchLine>(&geometry))
            return end ? line->end : line->start;
        const auto& arc = std::get<SketchArc>(geometry);
        return onArc(arc, end ? arc.endAngle : arc.startAngle);
    }
    [[nodiscard]] SketchPoint start() const { return stored(reversed); }
    [[nodiscard]] SketchPoint end() const { return stored(!reversed); }
    // Unit direction of travel at the start or the end.
    [[nodiscard]] SketchPoint direction(bool atEnd) const {
        if (std::holds_alternative<SketchLine>(geometry)) {
            const SketchPoint a = start(), b = end();
            const double length = distance(a, b);
            return {(b.x - a.x) / length, (b.y - a.y) / length};
        }
        const auto& arc = std::get<SketchArc>(geometry);
        const double angle = (atEnd != reversed) ? arc.endAngle : arc.startAngle;
        // Stored arcs run counterclockwise; travelled against that, the tangent turns round.
        const double sign = reversed ? -1.0 : 1.0;
        return {-std::sin(angle) * sign, std::cos(angle) * sign};
    }
};

double cross(SketchPoint a, SketchPoint b) { return a.x * b.y - a.y * b.x; }
double dot(SketchPoint a, SketchPoint b) { return a.x * b.x + a.y * b.y; }

// Orders the selection into one chain, following the first element's stored direction.
std::vector<Segment> chain(std::span<const document::SketchGeometry> selected, bool& closed) {
    std::vector<Segment> pending;
    for (const auto& geometry : selected) {
        if (!std::holds_alternative<SketchLine>(geometry) &&
            !std::holds_alternative<SketchArc>(geometry))
            refuse("Kreise und Rechtecke nur einzeln, Ellipsen gar nicht");
        pending.push_back({geometry, false});
    }
    std::vector<Segment> ordered{pending.front()};
    pending.erase(pending.begin());
    const auto attach = [&](bool forward) {
        for (bool found = true; found && !pending.empty();) {
            found = false;
            const SketchPoint joint = forward ? ordered.back().end() : ordered.front().start();
            for (auto it = pending.begin(); it != pending.end(); ++it) {
                for (const bool reversed : {false, true}) {
                    Segment candidate{it->geometry, reversed};
                    const SketchPoint touching = forward ? candidate.start() : candidate.end();
                    if (distance(touching, joint) > 1e-7)
                        continue;
                    if (forward)
                        ordered.push_back(candidate);
                    else
                        ordered.insert(ordered.begin(), candidate);
                    pending.erase(it);
                    found = true;
                    break;
                }
                if (found)
                    break;
            }
        }
    };
    attach(true);
    attach(false);
    if (!pending.empty())
        refuse("Die Auswahl bildet keine zusammenhängende Kontur");
    closed = ordered.size() > 1 && distance(ordered.back().end(), ordered.front().start()) <= 1e-7;
    return ordered;
}

// Twice the signed area of the closed contour, arcs sampled; positive counterclockwise.
double signedArea(const std::vector<Segment>& contour) {
    double area = 0.0;
    for (const auto& segment : contour) {
        std::vector<SketchPoint> points{segment.start()};
        if (const auto* arc = std::get_if<SketchArc>(&segment.geometry))
            for (int step = 1; step < 16; ++step) {
                const double t = static_cast<double>(step) / 16;
                const double from = segment.reversed ? arc->endAngle : arc->startAngle;
                const double to = segment.reversed ? arc->startAngle : arc->endAngle;
                points.push_back(onArc(*arc, from + (to - from) * t));
            }
        points.push_back(segment.end());
        for (std::size_t index = 0; index + 1 < points.size(); ++index)
            area += cross(points[index], points[index + 1]);
    }
    return area;
}

std::optional<SketchPoint> intersection(SketchPoint a, SketchPoint aDirection, SketchPoint b,
                                        SketchPoint bDirection) {
    const double denominator = cross(aDirection, bDirection);
    if (std::abs(denominator) <= tolerance)
        return std::nullopt;
    const SketchPoint delta{b.x - a.x, b.y - a.y};
    const double t = cross(delta, bDirection) / denominator;
    return SketchPoint{a.x + t * aDirection.x, a.y + t * aDirection.y};
}

bool segmentsCross(SketchPoint a, SketchPoint b, SketchPoint c, SketchPoint d) {
    const SketchPoint ab{b.x - a.x, b.y - a.y}, cd{d.x - c.x, d.y - c.y};
    const double d1 = cross(ab, {c.x - a.x, c.y - a.y}), d2 = cross(ab, {d.x - a.x, d.y - a.y});
    const double d3 = cross(cd, {a.x - c.x, a.y - c.y}), d4 = cross(cd, {b.x - c.x, b.y - c.y});
    return d1 * d2 < -tolerance && d3 * d4 < -tolerance;
}

SketchOffsetResult single(const document::SketchGeometry& geometry, double offset) {
    if (const auto* circle = std::get_if<document::SketchCircle>(&geometry)) {
        if (circle->radius + offset <= tolerance)
            refuse("Der Kreis fällt bei diesem Abstand zusammen");
        auto result = *circle;
        result.radius += offset;
        return {{result}, {}};
    }
    const auto& rectangle = std::get<document::SketchRectangle>(geometry);
    const double left = std::min(rectangle.first.x, rectangle.second.x) - offset;
    const double right = std::max(rectangle.first.x, rectangle.second.x) + offset;
    const double bottom = std::min(rectangle.first.y, rectangle.second.y) - offset;
    const double top = std::max(rectangle.first.y, rectangle.second.y) + offset;
    if (right - left <= tolerance || top - bottom <= tolerance)
        refuse("Das Rechteck fällt bei diesem Abstand zusammen");
    return {{document::SketchRectangle{left, bottom, right, top}}, {}};
}

} // namespace

SketchOffsetResult offsetGeometry(std::span<const document::SketchGeometry> selected,
                                  double offset) {
    if (selected.empty())
        refuse("Nichts gewählt");
    if (!std::isfinite(offset) || std::abs(offset) <= tolerance)
        refuse("Der Abstand muss endlich und ungleich null sein");
    if (std::ranges::any_of(selected, [](const auto& geometry) {
            return std::holds_alternative<document::SketchPointGeometry>(geometry);
        }))
        refuse("Ein Punkt lässt sich nicht versetzen");
    if (std::ranges::any_of(selected, [](const auto& geometry) {
            return std::holds_alternative<document::SketchBSpline>(geometry);
        }))
        refuse("B-Splines lassen sich nicht versetzen (UnsupportedCurve)"); // S21
    if (std::ranges::any_of(selected, [](const auto& geometry) {
            return std::holds_alternative<document::SketchEllipse>(geometry);
        }))
        refuse("Ellipsen werden nicht unterstützt (UnsupportedCurve); ihr Versatz ist keine "
               "Ellipse");
    if (selected.size() == 1 && !std::holds_alternative<SketchLine>(selected.front()) &&
        !std::holds_alternative<SketchArc>(selected.front()))
        return single(selected.front(), offset);

    bool closed = false;
    auto contour = chain(selected, closed);
    // Left offset in the direction of travel; closed contours grow outwards for positive d.
    const double left = closed && signedArea(contour) > 0.0 ? -offset : offset;

    std::vector<Segment> moved;
    for (const auto& segment : contour) {
        Segment result = segment;
        if (auto* line = std::get_if<SketchLine>(&result.geometry)) {
            const SketchPoint along = segment.direction(false);
            const SketchPoint normal{-along.y * left, along.x * left};
            line->start = {line->start.x + normal.x, line->start.y + normal.y};
            line->end = {line->end.x + normal.x, line->end.y + normal.y};
        } else {
            auto& arc = std::get<SketchArc>(result.geometry);
            // Travelling counterclockwise the left side faces the centre; a lone arc grows
            // with positive d, like a circle.
            arc.radius += contour.size() == 1 ? offset : segment.reversed ? left : -left;
            if (arc.radius <= tolerance)
                refuse("Ein Bogen fällt bei diesem Abstand zusammen");
        }
        moved.push_back(result);
    }

    // Joints: tangent ones stay joined; line corners are re-intersected.
    const std::size_t joints = closed ? moved.size() : moved.size() - 1;
    for (std::size_t index = 0; index < joints; ++index) {
        Segment& a = moved[index];
        Segment& b = moved[(index + 1) % moved.size()];
        const SketchPoint outgoing = contour[index].direction(true);
        const SketchPoint incoming = contour[(index + 1) % contour.size()].direction(false);
        if (std::abs(cross(outgoing, incoming)) <= 1e-9) {
            if (dot(outgoing, incoming) < 0.0)
                refuse("Die Kontur kehrt an einem Anschluss um");
            continue;
        }
        auto* first = std::get_if<SketchLine>(&a.geometry);
        auto* second = std::get_if<SketchLine>(&b.geometry);
        if (first == nullptr || second == nullptr)
            refuse("Ein Knick zwischen Linie und Bogen wird nicht unterstützt");
        const auto corner = intersection(a.end(), outgoing, b.start(), incoming);
        if (!corner)
            refuse("Ein Anschluss lässt sich nicht schneiden");
        (a.reversed ? first->start : first->end) = *corner;
        (b.reversed ? second->end : second->start) = *corner;
    }

    // A segment that vanished or turned round means the inner offset collapsed; non-adjacent
    // lines that cross mean the offset contour intersects itself.
    for (std::size_t index = 0; index < moved.size(); ++index)
        if (std::holds_alternative<SketchLine>(moved[index].geometry)) {
            const SketchPoint was = contour[index].direction(false);
            const double length = distance(moved[index].start(), moved[index].end());
            if (length <= 1e-7 || dot(moved[index].direction(false), was) <= 0.0)
                refuse("Bei diesem Abstand verschwindet ein Segment der Kontur");
        }
    for (std::size_t i = 0; i < moved.size(); ++i)
        for (std::size_t j = i + 2; j < moved.size(); ++j) {
            if (closed && i == 0 && j + 1 == moved.size())
                continue;
            if (std::holds_alternative<SketchLine>(moved[i].geometry) &&
                std::holds_alternative<SketchLine>(moved[j].geometry) &&
                segmentsCross(moved[i].start(), moved[i].end(), moved[j].start(), moved[j].end()))
                refuse("Die versetzte Kontur schneidet sich selbst");
        }

    SketchOffsetResult result;
    for (const auto& segment : moved)
        result.elements.push_back(segment.geometry);
    for (std::size_t index = 0; index < joints; ++index) {
        const std::size_t next = (index + 1) % moved.size();
        result.joints.push_back({index, moved[index].reversed ? Position::start : Position::end,
                                 next, moved[next].reversed ? Position::end : Position::start});
    }
    return result;
}

} // namespace voelund::application::detail
