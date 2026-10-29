// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Elips.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>

#include <optional>
#include <vector>

// N15a: where a body meets a plane, for section curves in a sketch. A failing OCCT computation
// is reported as KernelError.
namespace voelund::kernel {

// The edges of the body's section with the (unbounded) plane; none where they do not meet.
[[nodiscard]] std::vector<TopoDS_Edge> planeSectionEdges(const TopoDS_Shape& body,
                                                         const gp_Pln& plane);

// An edge on an ellipse with its parameter range (eccentric anomalies, as the edge runs).
struct EdgeEllipse final {
    gp_Elips ellipse;
    double first;
    double last;
};
[[nodiscard]] std::optional<EdgeEllipse> edgeEllipse(const TopoDS_Edge& edge);

// A cubic, clamped B-spline within `tolerance` of the edge: its poles and flat knots (each
// repeated by its multiplicity, starting at 0); none when it needs more than `maximumPoles`.
struct SpaceSpline final {
    std::vector<gp_Pnt> poles;
    std::vector<double> knots;
};
[[nodiscard]] std::optional<SpaceSpline> cubicSplineOfEdge(const TopoDS_Edge& edge,
                                                           double tolerance, int maximumPoles);

} // namespace voelund::kernel
