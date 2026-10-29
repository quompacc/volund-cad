// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchBSplineCurves.hpp"
#include "SketchConnection.hpp"
#include "SketchPlaneGuard.hpp"
#include "SketchProfileFace.hpp"
#include <voelund/features/GeometryEvaluationError.hpp>
#include <voelund/features/SketchEvaluation.hpp>

#include <voelund/features/SketchContourGraph.hpp>
#include <voelund/features/SketchProfiles.hpp>
#include <voelund/kernel/EdgeQueries.hpp>
#include <voelund/kernel/KernelError.hpp>
#include <voelund/kernel/ShapeQueries.hpp>
#include <voelund/kernel/SketchShapes.hpp>

#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Circ.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <stdexcept>

namespace voelund::features {
namespace {

gp_Pnt worldPoint(const document::SketchPlane& plane, const document::SketchPoint& point) {
    const gp_Dir normal(plane.normalX, plane.normalY, plane.normalZ);
    const gp_Dir xDirection(plane.xDirectionX, plane.xDirectionY, plane.xDirectionZ);
    const gp_Dir yDirection = normal.Crossed(xDirection);
    return gp_Pnt(plane.originX, plane.originY, plane.originZ)
        .Translated(gp_Vec(xDirection) * point.x + gp_Vec(yDirection) * point.y);
}

std::vector<EvaluatedSketchPoint> elementPoints(const document::SketchParameters& sketch,
                                                const document::SketchGeometry& geometry) {
    std::vector<std::pair<document::SketchPointPosition, document::SketchPoint>> points;
    if (const auto* line = std::get_if<document::SketchLine>(&geometry)) {
        points = {{document::SketchPointPosition::start, line->start},
                  {document::SketchPointPosition::end, line->end}};
    } else if (const auto* rectangle = std::get_if<document::SketchRectangle>(&geometry)) {
        points = {
            {document::SketchPointPosition::start, rectangle->first},
            {document::SketchPointPosition::end, {rectangle->second.x, rectangle->first.y}},
            {document::SketchPointPosition::end, rectangle->second},
            {document::SketchPointPosition::start, {rectangle->first.x, rectangle->second.y}}};
    } else if (const auto* circle = std::get_if<document::SketchCircle>(&geometry)) {
        points = {{document::SketchPointPosition::center, circle->center}};
    } else if (const auto* ellipse = std::get_if<document::SketchEllipse>(&geometry)) {
        points = {{document::SketchPointPosition::center, ellipse->center}};
    } else if (const auto* point = std::get_if<document::SketchPointGeometry>(&geometry)) {
        points = {{document::SketchPointPosition::center, point->position}}; // S15
    } else if (const auto* spline = std::get_if<document::SketchBSpline>(&geometry)) {
        points = {{document::SketchPointPosition::start, spline->poles.front()}, // S21
                  {document::SketchPointPosition::end, spline->poles.back()}};
    } else if (const auto* elliptic = std::get_if<document::SketchEllipticArc>(&geometry)) {
        points = {{document::SketchPointPosition::center, elliptic->center}, // N14c
                  {document::SketchPointPosition::start,
                   document::ellipsePoint(elliptic->ellipse(), elliptic->startParameter)},
                  {document::SketchPointPosition::end,
                   document::ellipsePoint(elliptic->ellipse(), elliptic->endParameter)}};
    } else {
        const auto& arc = std::get<document::SketchArc>(geometry);
        points = {{document::SketchPointPosition::center, arc.center},
                  {document::SketchPointPosition::start,
                   {arc.center.x + arc.radius * std::cos(arc.startAngle),
                    arc.center.y + arc.radius * std::sin(arc.startAngle)}},
                  {document::SketchPointPosition::end,
                   {arc.center.x + arc.radius * std::cos(arc.endAngle),
                    arc.center.y + arc.radius * std::sin(arc.endAngle)}}};
    }
    std::vector<EvaluatedSketchPoint> result;
    result.reserve(points.size());
    for (const auto& [kind, local] : points)
        result.push_back({kind, local, worldPoint(sketch.plane, local)});
    return result;
}

double radialRadius(const document::SketchGeometry& geometry) {
    if (const auto* circle = std::get_if<document::SketchCircle>(&geometry))
        return circle->radius;
    if (const auto* arc = std::get_if<document::SketchArc>(&geometry))
        return arc->radius;
    return 0.0;
}

std::optional<gp_Pnt> radialAnchor(const document::SketchParameters& sketch,
                                   const document::SketchGeometry& geometry) {
    if (const auto* circle = std::get_if<document::SketchCircle>(&geometry))
        return worldPoint(sketch.plane, {circle->center.x + circle->radius, circle->center.y});
    return std::nullopt;
}

void recordEdge(std::vector<EvaluatedContourEdge>& edges, document::SketchElementId id,
                ContourRole role, const TopoDS_Edge& source, const TopoDS_Edge& actual) {
    // MakeWire may copy the edge to share vertices. Its Edge() result is the
    // authoritative topology; a shared analytic curve proves parameter continuity.
    if (!kernel::sameEdgeCurve(source, actual))
        throw GeometryEvaluationError(
            "Contour builder did not preserve its analytic parameter domain");
    const auto range = kernel::storedRange(source);
    edges.push_back(
        {id, role, TopoDS::Edge(actual.Oriented(TopAbs_FORWARD)), range.first, range.last});
}

// The face inside a closed wire; a face OCCT cannot make is a kernel failure.
TopoDS_Face requiredFace(const TopoDS_Wire& wire) {
    const auto face = kernel::faceFromWire(wire);
    if (!face)
        throw kernel::KernelError(kernel::KernelOperation::sketchGeometry,
                                  "The face of a closed wire could not be made");
    return *face;
}

TopoDS_Wire rectangleWire(const document::SketchParameters& sketch,
                          const document::SketchRectangle& rectangle, document::SketchElementId id,
                          std::vector<EvaluatedContourEdge>& edges) {
    const document::SketchPoint points[] = {rectangle.first,
                                            {rectangle.second.x, rectangle.first.y},
                                            rectangle.second,
                                            {rectangle.first.x, rectangle.second.y}};
    const ContourRole roles[] = {ContourRole::rectangleFirst, ContourRole::rectangleSecond,
                                 ContourRole::rectangleThird, ContourRole::rectangleFourth};
    kernel::WireBuilder wire;
    for (int i = 0; i < 4; ++i) {
        const auto edge = kernel::segmentEdge(worldPoint(sketch.plane, points[i]),
                                              worldPoint(sketch.plane, points[(i + 1) % 4]));
        if (!wire.add(edge))
            throw GeometryEvaluationError("Rectangle wire could not be connected");
        recordEdge(edges, id, roles[i], edge, wire.lastEdge());
    }
    return wire.wire();
}

TopoDS_Edge circleEdge(const document::SketchParameters& sketch,
                       const document::SketchCircle& circle) {
    const gp_Pnt center = worldPoint(sketch.plane, circle.center);
    const gp_Dir normal(sketch.plane.normalX, sketch.plane.normalY, sketch.plane.normalZ);
    const gp_Dir xDirection(sketch.plane.xDirectionX, sketch.plane.xDirectionY,
                            sketch.plane.xDirectionZ);
    return kernel::circleEdge(gp_Ax2(center, normal, xDirection), circle.radius);
}

TopoDS_Edge arcEdge(const document::SketchParameters& sketch, const document::SketchArc& arc) {
    const gp_Pnt center = worldPoint(sketch.plane, arc.center);
    const gp_Dir normal(sketch.plane.normalX, sketch.plane.normalY, sketch.plane.normalZ);
    const gp_Dir xDirection(sketch.plane.xDirectionX, sketch.plane.xDirectionY,
                            sketch.plane.xDirectionZ);
    return kernel::SpaceCurve::circle(gp_Ax2(center, normal, xDirection), arc.radius)
        .edge(arc.startAngle, arc.endAngle);
}

kernel::SpaceCurve ellipseCurve(const document::SketchParameters& sketch,
                                const document::SketchEllipse& ellipse) {
    const gp_Pnt center = worldPoint(sketch.plane, ellipse.center);
    const gp_Dir normal(sketch.plane.normalX, sketch.plane.normalY, sketch.plane.normalZ);
    const gp_Dir planeX(sketch.plane.xDirectionX, sketch.plane.xDirectionY,
                        sketch.plane.xDirectionZ);
    const gp_Dir planeY = normal.Crossed(planeX);
    const gp_Dir majorDirection(gp_Vec(planeX) * std::cos(ellipse.rotation) +
                                gp_Vec(planeY) * std::sin(ellipse.rotation));
    return kernel::SpaceCurve::ellipse(gp_Ax2(center, normal, majorDirection), ellipse.majorRadius,
                                       ellipse.minorRadius);
}
TopoDS_Edge ellipseEdge(const document::SketchParameters& sketch,
                        const document::SketchEllipse& ellipse) {
    return ellipseCurve(sketch, ellipse).edge();
}
// N14c: the piece of the ellipse curve between the arc's parameters.
TopoDS_Edge ellipticArcEdge(const document::SketchParameters& sketch,
                            const document::SketchEllipticArc& arc) {
    return ellipseCurve(sketch, arc.ellipse()).edge(arc.startParameter, arc.endParameter);
}

const document::SketchElement& findElement(const document::SketchParameters& sketch,
                                           document::SketchElementId id) {
    const auto match = std::ranges::find(sketch.elements.values, id, &document::SketchElement::id);
    if (match == sketch.elements.values.end()) {
        throw GeometryEvaluationError("Sketch profile references an unknown element");
    }
    return *match;
}

std::pair<document::SketchPoint, document::SketchPoint>
curveEndpoints(const document::SketchGeometry& geometry) {
    if (const auto* line = std::get_if<document::SketchLine>(&geometry))
        return {line->start, line->end};
    if (const auto* spline = std::get_if<document::SketchBSpline>(&geometry))
        return {spline->poles.front(), spline->poles.back()}; // S21
    if (const auto* elliptic = std::get_if<document::SketchEllipticArc>(&geometry))
        return {document::ellipsePoint(elliptic->ellipse(), elliptic->startParameter), // N14c
                document::ellipsePoint(elliptic->ellipse(), elliptic->endParameter)};
    const auto& arc = std::get<document::SketchArc>(geometry);
    return {{arc.center.x + arc.radius * std::cos(arc.startAngle),
             arc.center.y + arc.radius * std::sin(arc.startAngle)},
            {arc.center.x + arc.radius * std::cos(arc.endAngle),
             arc.center.y + arc.radius * std::sin(arc.endAngle)}};
}

// The role a curve's edge plays; an elliptic arc (N14c) is a piece of an ellipse.
ContourRole contourRole(const document::SketchGeometry& geometry) {
    static constexpr std::array<ContourRole, std::variant_size_v<document::SketchGeometry>> roles{
        ContourRole::line,    ContourRole::rectangleFirst, ContourRole::circle,
        ContourRole::arc,     ContourRole::ellipse,        ContourRole::line,
        ContourRole::bspline, ContourRole::ellipse};
    return roles[geometry.index()];
}

// The edge of an open curve: line, arc, B-spline or elliptic arc.
TopoDS_Edge openCurveEdge(const document::SketchParameters& sketch,
                          const document::SketchGeometry& geometry) {
    if (const auto* line = std::get_if<document::SketchLine>(&geometry))
        return kernel::segmentEdge(worldPoint(sketch.plane, line->start),
                                   worldPoint(sketch.plane, line->end));
    if (const auto* spline = std::get_if<document::SketchBSpline>(&geometry))
        return detail::bsplineCurve3d(*spline, sketch.plane).edge();
    if (const auto* elliptic = std::get_if<document::SketchEllipticArc>(&geometry))
        return ellipticArcEdge(sketch, *elliptic);
    return arcEdge(sketch, std::get<document::SketchArc>(geometry));
}

TopoDS_Face curveProfileFace(const document::SketchParameters& sketch, const SketchProfile& profile,
                             std::vector<EvaluatedContourEdge>& edges) {
    kernel::WireBuilder wire;
    std::size_t contourIndex = 0;
    for (document::SketchElementId id : profile.elementIds) {
        const auto& geometry = findElement(sketch, id).geometry;
        auto points = curveEndpoints(geometry);
        TopoDS_Edge edge = openCurveEdge(sketch, geometry);
        if (profile.reversed.at(contourIndex++)) {
            edge.Reverse();
            std::swap(points.first, points.second);
        }
        // Preserve the original curves. OCCT may share vertices within this
        // explicit tolerance; no polygon reconstruction or endpoint snapping.
        kernel::widenVertexTolerance(
            edge, detail::connectionTolerance(points.first, points.second).millimetres);
        if (!wire.add(edge))
            throw GeometryEvaluationError("Sketch profile wire could not be connected",
                                          "profile-invalid");
        recordEdge(edges, id, contourRole(geometry), edge, wire.lastEdge());
    }
    const auto face = kernel::faceFromWire(wire.wire());
    if (!face || !kernel::isValidShape(*face))
        throw GeometryEvaluationError("Sketch profile did not produce a valid face",
                                      "profile-invalid");
    return *face;
}

TopoDS_Face profileFace(const document::SketchParameters& sketch, const SketchProfile& profile,
                        std::vector<EvaluatedContourEdge>& edges) {
    const document::SketchGeometry& first =
        findElement(sketch, profile.elementIds.front()).geometry;
    if (const auto* rectangle = std::get_if<document::SketchRectangle>(&first)) {
        return requiredFace(rectangleWire(sketch, *rectangle, profile.elementIds.front(), edges));
    }
    if (const auto* circle = std::get_if<document::SketchCircle>(&first)) {
        const auto edge = circleEdge(sketch, *circle);
        const kernel::WireBuilder wire(edge);
        recordEdge(edges, profile.elementIds.front(), ContourRole::circle, edge, wire.lastEdge());
        return requiredFace(wire.wire());
    }
    if (const auto* ellipse = std::get_if<document::SketchEllipse>(&first)) {
        const auto edge = ellipseEdge(sketch, *ellipse);
        const kernel::WireBuilder wire(edge);
        recordEdge(edges, profile.elementIds.front(), ContourRole::ellipse, edge, wire.lastEdge());
        return requiredFace(wire.wire());
    }
    return curveProfileFace(sketch, profile, edges);
}

TopoDS_Shape elementShape(const document::SketchParameters& sketch,
                          const document::SketchElement& element,
                          std::vector<EvaluatedContourEdge>& edges) {
    const auto& geometry = element.geometry;
    if (const auto* rectangle = std::get_if<document::SketchRectangle>(&geometry))
        return rectangleWire(sketch, *rectangle, element.id, edges);
    // S15: a point is a vertex; it has no contour edge.
    if (const auto* point = std::get_if<document::SketchPointGeometry>(&geometry))
        return kernel::pointVertex(worldPoint(sketch.plane, point->position));
    TopoDS_Edge edge;
    if (const auto* circle = std::get_if<document::SketchCircle>(&geometry))
        edge = circleEdge(sketch, *circle);
    else if (const auto* ellipse = std::get_if<document::SketchEllipse>(&geometry))
        edge = ellipseEdge(sketch, *ellipse);
    else
        edge = openCurveEdge(sketch, geometry); // S21/N14c
    recordEdge(edges, element.id, contourRole(geometry), edge, edge);
    return edge;
}

} // namespace

std::vector<EvaluatedSketchElement>
evaluateSketchElements(const document::SketchParameters& sketch) {
    detail::requireCanonicalPlane(sketch.plane);
    if (!sketch.elements.values.empty())
        document::validateSketchElements(sketch.elements);
    std::vector<EvaluatedSketchElement> result;
    result.reserve(sketch.elements.values.size());
    for (const document::SketchElement& element : sketch.elements.values) {
        std::vector<EvaluatedContourEdge> edges;
        const auto shape = elementShape(sketch, element, edges);
        result.push_back(EvaluatedSketchElement{
            element.id, shape, element.construction, elementPoints(sketch, element.geometry),
            radialRadius(element.geometry), radialAnchor(sketch, element.geometry),
            std::move(edges)});
    }
    return result;
}

TopoDS_Shape evaluateSketchProfileFaces(const document::SketchParameters& sketch) {
    return evaluateSketchProfileFaces(sketch, {});
}

TopoDS_Shape
evaluateSketchProfileFaces(const document::SketchParameters& sketch,
                           std::span<const document::SketchProfileId> selectedProfiles) {
    std::vector<TopoDS_Shape> faces;
    for (const auto& profile : evaluateNamedProfileFaces(sketch, selectedProfiles))
        faces.push_back(profile.face);
    return kernel::collect(faces);
}

EvaluatedProfileFace detail::profileFaceOf(const document::SketchParameters& sketch,
                                           const SketchProfile& profile) {
    auto boundary = profile.elementIds;
    std::ranges::sort(boundary, {}, &document::SketchElementId::value);
    std::vector<EvaluatedContourEdge> edges;
    const auto face = profileFace(sketch, profile, edges);
    for (const auto& edge : edges) {
        bool present = false;
        for (TopExp_Explorer it(face, TopAbs_EDGE); it.More(); it.Next())
            present |= it.Current().IsSame(edge.edge);
        if (!present)
            throw GeometryEvaluationError("Named contour edge is absent from the built profile");
    }
    return {std::move(boundary), face, std::move(edges)};
}

std::vector<EvaluatedProfileFace>
evaluateNamedProfileFaces(const document::SketchParameters& sketch,
                          std::span<const document::SketchProfileId> selectedProfiles) {
    detail::requireCanonicalPlane(sketch.plane);
    if (!sketch.elements.values.empty())
        document::validateSketchElements(sketch.elements);
    const std::vector<SketchProfile> profiles = detectClosedProfiles(sketch.elements.values);
    if (profiles.empty()) {
        std::string message = "Sketch does not contain a closed profile";
        const auto graph = buildSketchContourGraph(sketch.elements.values);
        for (const auto& d : graph.diagnostics) {
            switch (d.issue) {
            case ContourIssue::openEnd:
                message += "; open end";
                break;
            case ContourIssue::branch:
                message += "; branch";
                break;
            case ContourIssue::overlap:
                message += "; overlapping boundaries";
                break;
            case ContourIssue::ambiguousConnection:
                message += "; ambiguous tolerance connection";
                break;
            case ContourIssue::degenerate:
                message += "; degenerate boundary";
                break;
            }
            message += " [elements";
            for (auto id : d.elements)
                message += " " + std::to_string(id.value());
            message += "]";
        }
        throw GeometryEvaluationError(message);
    }
    std::vector<EvaluatedProfileFace> faces;
    std::size_t selectedCount = 0;
    for (const SketchProfile& profile : profiles) {
        if (!selectedProfiles.empty() &&
            std::ranges::find(selectedProfiles, profile.id) == selectedProfiles.end()) {
            continue;
        }
        faces.push_back(detail::profileFaceOf(sketch, profile));
        ++selectedCount;
    }
    if (!selectedProfiles.empty() && selectedCount != selectedProfiles.size())
        throw GeometryEvaluationError("Extrusion references an unknown sketch profile");
    return faces;
}

} // namespace voelund::features
