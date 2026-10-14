#include "SketchCurveEditPlan.hpp"

#include <voelund/kernel/NumericPolicy.hpp>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <numbers>
#include <stdexcept>
#include <string>

namespace voelund::application::detail {
namespace {

using document::SketchArc;
using document::SketchCircle;
using document::SketchElement;
using document::SketchElementId;
using document::SketchGeometry;
using document::SketchLine;
using document::SketchPoint;
using Position = document::SketchPointPosition;

constexpr double twoPi = 2.0 * std::numbers::pi;
// Shorter pieces would be null segments; cuts closer than this count as one.
constexpr double nullSegment = kernel::minimumModelLength;

[[noreturn]] void refuse(SketchCurveEdit edit, const std::string& reason) {
    throw std::invalid_argument(std::string(curveEditName(edit)) + ": " + reason);
}

double distance(SketchPoint a, SketchPoint b) { return std::hypot(a.x - b.x, a.y - b.y); }
SketchPoint onCircle(SketchPoint center, double radius, double angle) {
    return {center.x + radius * std::cos(angle), center.y + radius * std::sin(angle)};
}
double normalized(double angle) {
    angle = std::fmod(angle, twoPi);
    if (angle < 0.0)
        angle += twoPi;
    return angle >= twoPi ? 0.0 : angle;
}
double angleOf(SketchPoint center, SketchPoint point) {
    return normalized(std::atan2(point.y - center.y, point.x - center.x));
}

// The infinite line or full circle an element lies on.
struct Support final {
    bool circle;
    SketchPoint origin;    // line start or circle center
    SketchPoint direction; // unit direction of a line
    double radius;
};
Support lineSupport(SketchPoint start, SketchPoint end) {
    const double length = distance(start, end);
    return {false, start, {(end.x - start.x) / length, (end.y - start.y) / length}, 0.0};
}

// Where two supports meet; a tangent contact is one point, concentric circles never meet.
std::vector<SketchPoint> meet(const Support& a, const Support& b, double tolerance) {
    if (!a.circle && !b.circle) {
        const double cross = a.direction.x * b.direction.y - a.direction.y * b.direction.x;
        if (std::abs(cross) <= 1.0e-12)
            return {};
        const double dx = b.origin.x - a.origin.x;
        const double dy = b.origin.y - a.origin.y;
        const double t = (dx * b.direction.y - dy * b.direction.x) / cross;
        return {{a.origin.x + a.direction.x * t, a.origin.y + a.direction.y * t}};
    }
    if (a.circle && !b.circle)
        return meet(b, a, tolerance);
    if (!a.circle) {
        const double along = (b.origin.x - a.origin.x) * a.direction.x +
                             (b.origin.y - a.origin.y) * a.direction.y;
        const SketchPoint foot{a.origin.x + a.direction.x * along,
                               a.origin.y + a.direction.y * along};
        const double offset = distance(foot, b.origin);
        if (offset > b.radius + tolerance)
            return {};
        if (std::abs(offset - b.radius) <= tolerance)
            return {foot};
        const double half = std::sqrt(b.radius * b.radius - offset * offset);
        return {{foot.x - a.direction.x * half, foot.y - a.direction.y * half},
                {foot.x + a.direction.x * half, foot.y + a.direction.y * half}};
    }
    const double centers = distance(a.origin, b.origin);
    if (centers <= tolerance || centers > a.radius + b.radius + tolerance ||
        centers < std::abs(a.radius - b.radius) - tolerance)
        return {};
    const SketchPoint unit{(b.origin.x - a.origin.x) / centers, (b.origin.y - a.origin.y) / centers};
    const double along =
        (centers * centers + a.radius * a.radius - b.radius * b.radius) / (2.0 * centers);
    const SketchPoint foot{a.origin.x + unit.x * along, a.origin.y + unit.y * along};
    const double squared = a.radius * a.radius - along * along;
    if (std::abs(centers - a.radius - b.radius) <= tolerance ||
        std::abs(centers - std::abs(a.radius - b.radius)) <= tolerance || squared <= 0.0)
        return {foot};
    const double half = std::sqrt(squared);
    return {{foot.x - unit.y * half, foot.y + unit.x * half},
            {foot.x + unit.y * half, foot.y - unit.x * half}};
}

// A point where the edited element's support meets a cutting edge.
struct Hit final {
    SketchPoint point;
    SketchElementId cutter;
    std::optional<Position> cutterEnd;
    bool onObject; // the cutter can carry a point-on-object relation
    double at{0.0}; // position along the edited element (arc length from its start)
};

// One bounded cutting edge: a segment or an arc of its support (a circle has no bounds).
struct Edge final {
    Support support;
    double first, last; // segment length or arc angles; a full circle spans [0, 2 pi)
    std::optional<std::pair<SketchPoint, SketchPoint>> ends;
    bool onObject;
};

std::vector<Edge> edges(const SketchGeometry& geometry) {
    if (const auto* line = std::get_if<SketchLine>(&geometry))
        return {{lineSupport(line->start, line->end), 0.0, distance(line->start, line->end),
                 std::pair{line->start, line->end}, true}};
    if (const auto* circle = std::get_if<SketchCircle>(&geometry))
        return {{{true, circle->center, {}, circle->radius}, 0.0, twoPi, std::nullopt, true}};
    if (const auto* arc = std::get_if<SketchArc>(&geometry))
        return {{{true, arc->center, {}, arc->radius},
                 arc->startAngle,
                 arc->endAngle,
                 std::pair{onCircle(arc->center, arc->radius, arc->startAngle),
                           onCircle(arc->center, arc->radius, arc->endAngle)},
                 false}};
    if (const auto* rectangle = std::get_if<document::SketchRectangle>(&geometry)) {
        const SketchPoint corners[] = {rectangle->first,
                                       {rectangle->second.x, rectangle->first.y},
                                       rectangle->second,
                                       {rectangle->first.x, rectangle->second.y}};
        std::vector<Edge> result;
        for (std::size_t index = 0; index < 4; ++index) {
            const SketchPoint a = corners[index], b = corners[(index + 1) % 4];
            result.push_back({lineSupport(a, b), 0.0, distance(a, b), std::nullopt, false});
        }
        return result;
    }
    return {}; // S10: ellipses do not cut.
}

bool within(const Edge& edge, SketchPoint point, double tolerance) {
    if (!edge.support.circle) {
        const double along = (point.x - edge.support.origin.x) * edge.support.direction.x +
                             (point.y - edge.support.origin.y) * edge.support.direction.y;
        return along >= edge.first - tolerance && along <= edge.last + tolerance;
    }
    if (edge.last - edge.first >= twoPi)
        return true;
    const double angle = normalized(angleOf(edge.support.origin, point) - edge.first);
    const double slack = tolerance / edge.support.radius;
    return angle <= edge.last - edge.first + slack || angle >= twoPi - slack;
}

double scaleOf(std::span<const SketchElement> elements) {
    double scale = 1.0;
    const auto grow = [&scale](SketchPoint point) {
        scale = std::max({scale, std::abs(point.x), std::abs(point.y)});
    };
    for (const auto& element : elements)
        std::visit(
            [&](const auto& value) {
                using Geometry = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Geometry, SketchLine>) {
                    grow(value.start);
                    grow(value.end);
                } else if constexpr (std::is_same_v<Geometry, document::SketchRectangle>) {
                    grow(value.first);
                    grow(value.second);
                } else if constexpr (std::is_same_v<Geometry, document::SketchEllipse>) {
                    grow(value.center);
                    scale = std::max(scale, value.majorRadius);
                } else if constexpr (std::is_same_v<Geometry, document::SketchPointGeometry>) {
                    grow(value.position);
                } else if constexpr (std::is_same_v<Geometry, document::SketchBSpline>) {
                    for (const auto& pole : value.poles)
                        grow(pole);
                } else {
                    grow(value.center);
                    scale = std::max(scale, value.radius);
                }
            },
            element.geometry);
    return scale;
}

// The edited element as a curve parametrized by arc length from its start.
struct Curve final {
    SketchGeometry geometry;
    Support support;
    double length; // a circle: its circumference

    [[nodiscard]] const SketchLine* line() const { return std::get_if<SketchLine>(&geometry); }
    [[nodiscard]] const SketchArc* arc() const { return std::get_if<SketchArc>(&geometry); }
    [[nodiscard]] double start() const { return arc() ? arc()->startAngle : 0.0; }
    // Unbounded along a line; on a circle in [0, circumference) from the start.
    [[nodiscard]] double at(SketchPoint point) const {
        if (line())
            return (point.x - support.origin.x) * support.direction.x +
                   (point.y - support.origin.y) * support.direction.y;
        return normalized(angleOf(support.origin, point) - start()) * support.radius;
    }
    [[nodiscard]] SketchPoint point(double at) const {
        if (const auto* value = line())
            return at == 0.0      ? value->start
                   : at == length ? value->end
                                  : SketchPoint{value->start.x + support.direction.x * at,
                                                value->start.y + support.direction.y * at};
        return onCircle(support.origin, support.radius, start() + at / support.radius);
    }
    // The piece between two positions; positions beyond the ends extend it.
    [[nodiscard]] SketchGeometry piece(double from, double to) const {
        if (const auto* value = line())
            return SketchLine{point(from), point(to), value->constraint};
        const double radius = support.radius;
        return SketchArc{support.origin, radius, start() + from / radius,
                         arc() && to == length ? arc()->endAngle : start() + to / radius};
    }
};

Curve curveOf(const SketchGeometry& geometry) {
    if (const auto* line = std::get_if<SketchLine>(&geometry))
        return {geometry, lineSupport(line->start, line->end), distance(line->start, line->end)};
    if (const auto* arc = std::get_if<SketchArc>(&geometry))
        return {geometry,
                {true, arc->center, {}, arc->radius},
                (arc->endAngle - arc->startAngle) * arc->radius};
    const auto& circle = std::get<SketchCircle>(geometry);
    return {geometry, {true, circle.center, {}, circle.radius}, twoPi * circle.radius};
}

// Every point where the edited curve's support meets another element, sorted along it.
std::vector<Hit> hits(const Curve& curve, std::span<const SketchElement> elements,
                      SketchElementId target, double tolerance) {
    std::vector<Hit> result;
    for (const SketchElement& element : elements) {
        if (element.id == target)
            continue;
        for (const Edge& edge : edges(element.geometry))
            for (SketchPoint point : meet(curve.support, edge.support, tolerance)) {
                if (!within(edge, point, tolerance))
                    continue;
                Hit hit{point, element.id, std::nullopt, edge.onObject, 0.0};
                if (edge.ends.has_value()) {
                    if (distance(point, edge.ends->first) <= nullSegment)
                        hit.cutterEnd = Position::start;
                    else if (distance(point, edge.ends->second) <= nullSegment)
                        hit.cutterEnd = Position::end;
                }
                hit.at = curve.at(point);
                result.push_back(hit);
            }
    }
    std::ranges::sort(result, {}, &Hit::at);
    return result;
}

std::optional<SketchCurveEditBinding> binding(std::size_t piece, Position point, const Hit& hit) {
    if (!hit.cutterEnd.has_value() && !hit.onObject)
        return std::nullopt;
    return SketchCurveEditBinding{piece, point, hit.cutter, hit.cutterEnd};
}

void bind(SketchCurveEditPlan& plan, std::size_t piece, Position point, const Hit& hit) {
    if (const auto value = binding(piece, point, hit))
        plan.bindings.push_back(*value);
}

// The picked position on the element; beyond an arc it falls to the nearer end.
double pickedAt(const Curve& curve, SketchPoint pick) {
    const double at = curve.at(pick);
    if (curve.line())
        return std::clamp(at, 0.0, curve.length);
    if (curve.arc() && at > curve.length)
        return at - curve.length < curve.support.radius * twoPi - at ? curve.length : 0.0;
    return at;
}

SketchCurveEditPlan trim(const Curve& curve, std::vector<Hit> cuts, SketchPoint pick) {
    constexpr auto edit = SketchCurveEdit::trim;
    const bool circle = !curve.line() && !curve.arc();
    // Cuts at the ends would leave null segments; close cuts are one cut.
    std::erase_if(cuts, [&](const Hit& hit) {
        return !circle && (hit.at < nullSegment || hit.at > curve.length - nullSegment);
    });
    std::vector<Hit> distinct;
    for (const Hit& hit : cuts)
        if (distinct.empty() || hit.at - distinct.back().at >= nullSegment)
            distinct.push_back(hit);
    if (circle && distinct.size() > 1 &&
        distinct.front().at + curve.length - distinct.back().at < nullSegment)
        distinct.pop_back();
    if (distinct.empty())
        refuse(edit, "Das Element schneidet keine andere Geometrie; es gibt kein Stück zu entfernen");
    if (circle && distinct.size() < 2)
        refuse(edit, "Ein Kreis braucht mindestens zwei Schnittpunkte zum Stutzen");
    const double picked = pickedAt(curve, pick);
    for (const Hit& hit : distinct) {
        const double gap = std::abs(hit.at - picked);
        if (gap < nullSegment || (circle && curve.length - gap < nullSegment))
            refuse(edit, "Die Pickstelle liegt auf einem Schnittpunkt; das Stück daneben anklicken");
    }
    const auto upper = std::ranges::find_if(distinct, [picked](const Hit& hit) {
        return hit.at > picked;
    });
    const Hit* after = upper == distinct.end() ? nullptr : &*upper;
    const Hit* before = upper == distinct.begin() ? nullptr : &*std::prev(upper);
    SketchCurveEditPlan plan{curve.geometry, std::nullopt, false, std::nullopt, std::nullopt, {}};
    if (circle) {
        // The arc that remains runs from the cut after the pick round to the cut before it.
        after = after ? after : &distinct.front();
        before = before ? before : &distinct.back();
        const double start = curve.start() + after->at / curve.support.radius;
        double end = curve.start() + before->at / curve.support.radius;
        if (end <= start)
            end += twoPi;
        plan.edited = SketchArc{curve.support.origin, curve.support.radius, start, end};
        bind(plan, 0, Position::start, *after);
        bind(plan, 0, Position::end, *before);
    } else if (before && after) {
        plan.edited = curve.piece(0.0, before->at);
        plan.piece = curve.piece(after->at, curve.length);
        bind(plan, 0, Position::end, *before);
        bind(plan, 1, Position::start, *after);
    } else if (after) {
        plan.edited = curve.piece(after->at, curve.length);
        plan.movedEnd = Position::start;
        bind(plan, 0, Position::start, *after);
    } else {
        plan.edited = curve.piece(0.0, before->at);
        plan.movedEnd = Position::end;
        bind(plan, 0, Position::end, *before);
    }
    return plan;
}

SketchCurveEditPlan extend(const Curve& curve, const std::vector<Hit>& targets, SketchPoint pick) {
    const double radius = curve.support.radius;
    bool atStart = false;
    if (curve.line())
        atStart = curve.at(pick) < curve.length / 2.0;
    else
        atStart = distance(pick, curve.point(0.0)) < distance(pick, curve.point(curve.length));
    // How far the end moves to reach the hit, or nothing when the hit is not ahead of it.
    const auto reach = [&](const Hit& hit) -> std::optional<double> {
        double step = 0.0;
        if (curve.line())
            step = atStart ? -hit.at : hit.at - curve.length;
        else if (hit.at > curve.length)
            step = atStart ? twoPi * radius - hit.at : hit.at - curve.length;
        else
            return std::nullopt;
        // An arc must not grow into itself.
        if (step < nullSegment ||
            (curve.arc() && curve.length + step > twoPi * radius - nullSegment))
            return std::nullopt;
        return step;
    };
    const Hit* nearest = nullptr;
    double best = 0.0;
    for (const Hit& hit : targets)
        if (const auto step = reach(hit); step && (!nearest || *step < best)) {
            nearest = &hit;
            best = *step;
        }
    if (nearest == nullptr)
        refuse(SketchCurveEdit::extend, "In Verlängerungsrichtung liegt keine Geometrie");
    SketchCurveEditPlan plan{curve.geometry, std::nullopt, false, std::nullopt, std::nullopt, {}};
    plan.edited = atStart ? curve.piece(-best, curve.length) : curve.piece(0.0, curve.length + best);
    plan.movedEnd = atStart ? Position::start : Position::end;
    bind(plan, 0, *plan.movedEnd, *nearest);
    return plan;
}

SketchCurveEditPlan split(const Curve& curve, SketchPoint pick) {
    const double at = curve.at(pick);
    if (at < nullSegment || at > curve.length - nullSegment)
        refuse(SketchCurveEdit::split,
               "Die Stelle liegt an einem Endpunkt; es entstünde ein Nullsegment");
    SketchCurveEditPlan plan{curve.piece(0.0, at), curve.piece(at, curve.length), true,
                             std::nullopt, std::nullopt, {}};
    if (curve.line())
        plan.keptShare = at / curve.length;
    return plan;
}

const char* unsupported(SketchCurveEdit edit, const SketchGeometry& geometry) {
    if (std::holds_alternative<document::SketchEllipse>(geometry))
        return "Ellipsen werden nicht unterstützt (UnsupportedCurve)";
    if (std::holds_alternative<document::SketchRectangle>(geometry))
        return "Alt-Rechtecke werden nicht unterstützt (UnsupportedCurve)";
    if (std::holds_alternative<document::SketchPointGeometry>(geometry))
        return "Ein Punkt ist keine Kurve (UnsupportedCurve)";
    if (std::holds_alternative<document::SketchBSpline>(geometry)) // S21
        return "B-Splines werden nicht gestutzt, verlängert oder geteilt (UnsupportedCurve)";
    return edit == SketchCurveEdit::extend
               ? "Kreise sind geschlossen und lassen sich nicht verlängern (UnsupportedCurve)"
               : "Kreise haben keinen Endpunkt zum Teilen; zuerst stutzen (UnsupportedCurve)";
}

} // namespace

const char* curveEditName(SketchCurveEdit edit) noexcept {
    return edit == SketchCurveEdit::trim     ? "Stutzen"
           : edit == SketchCurveEdit::extend ? "Verlängern"
                                             : "Teilen";
}

bool supportsCurveEdit(SketchCurveEdit edit, const SketchGeometry& geometry) noexcept {
    if (std::holds_alternative<SketchLine>(geometry) || std::holds_alternative<SketchArc>(geometry))
        return true;
    return edit == SketchCurveEdit::trim && std::holds_alternative<SketchCircle>(geometry);
}

SketchCurveEditPlan planCurveEdit(SketchCurveEdit edit, std::span<const SketchElement> elements,
                                  SketchElementId target, SketchPoint pick) {
    const auto match = std::ranges::find(elements, target, &SketchElement::id);
    if (match == elements.end())
        refuse(edit, "Das gewählte Element gibt es nicht mehr");
    if (!supportsCurveEdit(edit, match->geometry))
        refuse(edit, unsupported(edit, match->geometry));
    const Curve curve = curveOf(match->geometry);
    if (edit == SketchCurveEdit::split)
        return split(curve, pick);
    const double tolerance = kernel::connectionTolerance(scaleOf(elements)).millimetres;
    auto found = hits(curve, elements, target, tolerance);
    return edit == SketchCurveEdit::trim ? trim(curve, std::move(found), pick)
                                         : extend(curve, found, pick);
}

} // namespace voelund::application::detail
