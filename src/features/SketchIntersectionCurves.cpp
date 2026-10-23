// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchIntersectionCurves.hpp"
#include <stdexcept>
#include <variant>
namespace voelund::features::detail {
namespace {
gp_Pnt2d point(document::SketchPoint p) { return {p.x, p.y}; }

struct Definition final {
    kernel::CurveDefinition2d operator()(const document::SketchLine& l) const {
        return kernel::Line2d{point(l.start), point(l.end)};
    }
    kernel::CurveDefinition2d operator()(const document::SketchCircle& c) const {
        return kernel::Circle2d{point(c.center), c.radius};
    }
    kernel::CurveDefinition2d operator()(const document::SketchArc& a) const {
        return kernel::Arc2d{point(a.center), a.radius, a.startAngle, a.endAngle};
    }
    kernel::CurveDefinition2d operator()(const document::SketchEllipse& e) const {
        return kernel::Ellipse2d{point(e.center), e.majorRadius, e.minorRadius, e.rotation};
    }
    kernel::CurveDefinition2d operator()(const document::SketchBSpline& s) const { // S21
        kernel::BSpline2d spline;
        for (const auto& pole : s.poles)
            spline.poles.push_back(point(pole));
        return spline;
    }
    kernel::CurveDefinition2d operator()(const document::SketchRectangle&) const {
        throw std::logic_error("Rectangle must be expanded into contour pieces");
    }
    kernel::CurveDefinition2d operator()(const document::SketchPointGeometry&) const {
        throw std::logic_error("A point is no contour piece");
    }
};
} // namespace
kernel::CurveDefinition2d curveDefinition(const document::SketchGeometry& geometry) {
    return std::visit(Definition{}, geometry);
}
PlanarCurve planarCurve(const ContourPiece& piece) {
    return PlanarCurve(curveDefinition(piece.geometry));
}
} // namespace voelund::features::detail
