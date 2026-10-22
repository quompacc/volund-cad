// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchRegionGeometry.hpp"
#include "SketchBSplineCurves.hpp"

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepGProp.hxx>
#include <BRep_Builder.hxx>
#include <GProp_GProps.hxx>
#include <GeomAPI.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <algorithm>
#include <cmath>
#include <gp_Pln.hxx>
#include <gp_Vec2d.hxx>
#include <stdexcept>
#include <voelund/kernel/NumericPolicy.hpp>
namespace voelund::features::detail {
double boundaryArea(const SketchIntersectionResult& split,
                    const std::vector<RegionHalfEdge>& boundary) {
    const auto anchor = split.intervals[boundary.front().interval].start;
    long double integral = 0;
    for (auto h : boundary) {
        const auto& interval = split.intervals[h.interval];
        const auto& geometry = split.pieces[interval.piece].geometry;
        long double contribution = 0;
        if (std::holds_alternative<document::SketchLine>(geometry)) {
            const long double x = interval.start.x - anchor.x, y = interval.start.y - anchor.y;
            const long double u = interval.end.x - anchor.x, v = interval.end.y - anchor.y;
            contribution = x * v - y * u;
        } else if (const auto* spline = std::get_if<document::SketchBSpline>(&geometry)) {
            // S21: Gauss-Legendre over each knot span the interval touches.
            const auto curve = bsplineCurve2d(*spline);
            constexpr double nodes[] = {-0.9061798459386640, -0.5384693101056831, 0.0,
                                        0.5384693101056831, 0.9061798459386640};
            constexpr double weights[] = {0.2369268850561891, 0.4786286704993665,
                                          0.5688888888888889, 0.4786286704993665,
                                          0.2369268850561891};
            const double first = interval.firstParameter, last = interval.lastParameter;
            for (double low = std::floor(first); low < last; low += 1) {
                const double a = std::max(low, first), b = std::min(low + 1, last);
                if (b <= a)
                    continue;
                for (int k = 0; k < 5; ++k) {
                    const double u = (a + b) / 2 + (b - a) / 2 * nodes[k];
                    gp_Pnt2d p;
                    gp_Vec2d d;
                    curve->D1(u, p, d);
                    contribution += static_cast<long double>(weights[k] * (b - a) / 2) *
                                    ((p.X() - anchor.x) * d.Y() - (p.Y() - anchor.y) * d.X());
                }
            }
        } else {
            document::SketchPoint center;
            double product;
            if (auto* c = std::get_if<document::SketchCircle>(&geometry)) {
                center = c->center;
                product = c->radius * c->radius;
            } else if (auto* a = std::get_if<document::SketchArc>(&geometry)) {
                center = a->center;
                product = a->radius * a->radius;
            } else {
                const auto& e = std::get<document::SketchEllipse>(geometry);
                center = e.center;
                product = e.majorRadius * e.minorRadius;
            }
            contribution = static_cast<long double>(center.x - anchor.x) *
                               (interval.end.y - interval.start.y) -
                           static_cast<long double>(center.y - anchor.y) *
                               (interval.end.x - interval.start.x) +
                           static_cast<long double>(product) *
                               (interval.lastParameter - interval.firstParameter);
        }
        integral += h.reversed ? -contribution : contribution;
    }
    return static_cast<double>(integral / 2);
}
namespace {
TopoDS_Face face(const SketchRegionResult& result, const SketchRegionBoundary& boundary,
                 const std::vector<PlanarCurve>& curves) {
    const gp_Pln plane(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1));
    BRepBuilderAPI_MakeWire wire;
    for (auto h : boundary.edges) {
        const auto& interval = result.split.intervals[h.interval];
        const auto& c = curves[interval.piece];
        auto edge = BRepBuilderAPI_MakeEdge(GeomAPI::To3d(c.curve, plane), interval.firstParameter,
                                            interval.lastParameter)
                        .Edge();
        if (h.reversed)
            edge.Reverse();
        BRep_Builder builder;
        for (TopExp_Explorer v(edge, TopAbs_VERTEX); v.More(); v.Next())
            builder.UpdateVertex(TopoDS::Vertex(v.Current()),
                                 kernel::connectionTolerance(c.scale).millimetres);
        wire.Add(edge);
        if (!wire.IsDone())
            throw std::runtime_error("Region wire failed");
    }
    BRepBuilderAPI_MakeFace builder(plane, wire.Wire(), true);
    if (!builder.IsDone() || !BRepCheck_Analyzer(builder.Face()).IsValid())
        throw std::runtime_error("Region classifier face invalid");
    GProp_GProps properties;
    BRepGProp::SurfaceProperties(builder.Face(), properties);
    if (std::abs(properties.Mass() - boundary.area) > 1e-8 * std::max(1., boundary.area))
        throw std::runtime_error("Analytic region area disagrees with classifier face");
    return builder.Face();
}
bool contains(const SketchRegionResult& r, std::size_t inner, const TopoDS_Face& outer,
              const std::vector<PlanarCurve>& curves) {
    bool inside = false;
    for (auto h : r.boundaries[inner].edges) {
        const auto& i = r.split.intervals[h.interval];
        const auto& c = curves[i.piece];
        // Boundaries in this arrangement cannot cross between split vertices.
        const auto p = c.curve->Value((i.firstParameter + i.lastParameter) / 2);
        BRepClass_FaceClassifier classifier(outer, gp_Pnt(p.X(), p.Y(), 0),
                                            kernel::connectionTolerance(c.scale).millimetres);
        if (classifier.State() == TopAbs_OUT)
            return false;
        if (classifier.State() == TopAbs_IN)
            inside = true;
        else if (classifier.State() != TopAbs_ON)
            throw std::runtime_error("Unknown containment");
    }
    return inside;
}
} // namespace
void classifyRegionContainment(SketchRegionResult& r, const std::vector<PlanarCurve>& curves) {
    std::vector<TopoDS_Face> faces;
    for (const auto& boundary : r.boundaries)
        faces.push_back(face(r, boundary, curves));
    const auto n = faces.size();
    std::vector<std::size_t> parent(n, n);
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = 0; j < n; ++j) {
            if (r.boundaries[j].area <= r.boundaries[i].area)
                continue;
            if (parent[i] != n && r.boundaries[j].area >= r.boundaries[parent[i]].area)
                continue;
            if (contains(r, i, faces[j], curves))
                parent[i] = j;
        }
    for (std::size_t i = 0; i < n; ++i) {
        SketchRegion region{i, {}, r.boundaries[i].area};
        for (std::size_t j = 0; j < n; ++j)
            if (parent[j] == i) {
                region.holes.push_back(j);
                region.area -= r.boundaries[j].area;
            }
        if (region.area <= 0)
            throw std::runtime_error("Non-positive region area");
        r.regions.push_back(std::move(region));
    }
}
} // namespace voelund::features::detail
