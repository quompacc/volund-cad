// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchCurveEditCuts.hpp"

#include <voelund/kernel/KernelError.hpp>

#include <cmath>
#include <utility>

namespace voelund::application::detail {
namespace {

using document::SketchPoint;
using Position = document::SketchPointPosition;
using Ends = std::optional<std::pair<SketchPoint, SketchPoint>>;

gp_Pnt2d point(SketchPoint p) { return {p.x, p.y}; }
double distance(SketchPoint a, SketchPoint b) { return std::hypot(a.x - b.x, a.y - b.y); }
SketchPoint onCircle(SketchPoint center, double radius, double angle) {
    return {center.x + radius * std::cos(angle), center.y + radius * std::sin(angle)};
}
kernel::Ellipse2d ellipse2d(const document::SketchEllipse& e) {
    return {point(e.center), e.majorRadius, e.minorRadius, e.rotation};
}

// One bounded curve of a cutting element.
struct Piece final {
    kernel::CurveDefinition2d curve;
    Ends ends;
    bool onObject;
};

std::vector<Piece> pieces(const document::SketchLine& line) {
    return {{kernel::Line2d{point(line.start), point(line.end)}, std::pair{line.start, line.end},
             true}};
}
std::vector<Piece> pieces(const document::SketchCircle& circle) {
    return {{kernel::Circle2d{point(circle.center), circle.radius}, std::nullopt, true}};
}
std::vector<Piece> pieces(const document::SketchArc& arc) {
    return {{kernel::Arc2d{point(arc.center), arc.radius, arc.startAngle, arc.endAngle},
             std::pair{onCircle(arc.center, arc.radius, arc.startAngle),
                       onCircle(arc.center, arc.radius, arc.endAngle)},
             false}};
}
std::vector<Piece> pieces(const document::SketchEllipse& ellipse) {
    return {{ellipse2d(ellipse), std::nullopt, false}};
}
std::vector<Piece> pieces(const document::SketchEllipticArc& arc) {
    return {{kernel::EllipticArc2d{ellipse2d(arc.ellipse()), arc.startParameter, arc.endParameter},
             std::pair{document::ellipsePoint(arc.ellipse(), arc.startParameter),
                       document::ellipsePoint(arc.ellipse(), arc.endParameter)},
             false}};
}
std::vector<Piece> pieces(const document::SketchBSpline& spline) {
    kernel::BSpline2d curve;
    for (const auto& pole : spline.poles)
        curve.poles.push_back(point(pole));
    return {{curve, std::pair{spline.poles.front(), spline.poles.back()}, false}};
}
std::vector<Piece> pieces(const document::SketchRectangle& rectangle) {
    const SketchPoint corners[] = {rectangle.first,
                                   {rectangle.second.x, rectangle.first.y},
                                   rectangle.second,
                                   {rectangle.first.x, rectangle.second.y}};
    std::vector<Piece> result;
    for (std::size_t index = 0; index < 4; ++index)
        result.push_back({kernel::Line2d{point(corners[index]), point(corners[(index + 1) % 4])},
                          std::nullopt, false});
    return result;
}
std::vector<Piece> pieces(const document::SketchPointGeometry&) { return {}; }

std::optional<Position> endAt(const Ends& ends, SketchPoint at, double nullSegment) {
    if (!ends)
        return std::nullopt;
    if (distance(at, ends->first) <= nullSegment)
        return Position::start;
    if (distance(at, ends->second) <= nullSegment)
        return Position::end;
    return std::nullopt;
}

} // namespace

bool needsCurveCuts(const document::SketchGeometry& geometry) noexcept {
    return std::holds_alternative<document::SketchEllipse>(geometry) ||
           std::holds_alternative<document::SketchEllipticArc>(geometry) ||
           std::holds_alternative<document::SketchBSpline>(geometry);
}

std::vector<CurveCut> curveCuts(const kernel::CurveDefinition2d& support,
                                const document::SketchGeometry& cutter, double tolerance,
                                double nullSegment) {
    std::vector<CurveCut> result;
    const kernel::PlanarCurve edited(support);
    for (const Piece& piece : std::visit([](const auto& value) { return pieces(value); }, cutter)) {
        const kernel::PlanarCurve curve(piece.curve);
        const auto meeting = kernel::intersectCurves(edited, curve, tolerance);
        for (const auto& hit : meeting.points) {
            try {
                // The intersection runs on the unbounded curves: keep the cutter's own part.
                const auto at = curve.value(curve.domainParameter(hit.second, tolerance));
                const SketchPoint cut{at.X(), at.Y()};
                result.push_back({cut, endAt(piece.ends, cut, nullSegment), piece.onObject});
            } catch (const kernel::KernelError&) {
            }
        }
    }
    return result;
}

} // namespace voelund::application::detail
