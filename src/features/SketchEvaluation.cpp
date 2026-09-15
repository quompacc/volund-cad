#include "SketchConnection.hpp"
#include "SketchPlaneGuard.hpp"
#include <voelund/features/GeometryEvaluationError.hpp>
#include <voelund/features/SketchEvaluation.hpp>

#include <voelund/features/SketchProfiles.hpp>

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRep_Builder.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <GC_MakeCircle.hxx>
#include <Geom_Circle.hxx>
#include <Geom_Ellipse.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Circ.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>

namespace voelund::features {
namespace {

using detail::connected;

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

TopoDS_Wire rectangleWire(const document::SketchParameters& sketch,
                          const document::SketchRectangle& rectangle) {
    BRepBuilderAPI_MakePolygon polygon;
    polygon.Add(worldPoint(sketch.plane, rectangle.first));
    polygon.Add(worldPoint(sketch.plane, {rectangle.second.x, rectangle.first.y}));
    polygon.Add(worldPoint(sketch.plane, rectangle.second));
    polygon.Add(worldPoint(sketch.plane, {rectangle.first.x, rectangle.second.y}));
    polygon.Close();
    return polygon.Wire();
}

TopoDS_Edge circleEdge(const document::SketchParameters& sketch,
                       const document::SketchCircle& circle) {
    const gp_Pnt center = worldPoint(sketch.plane, circle.center);
    const gp_Dir normal(sketch.plane.normalX, sketch.plane.normalY, sketch.plane.normalZ);
    const gp_Circ curve = GC_MakeCircle(center, normal, circle.radius).Value()->Circ();
    return BRepBuilderAPI_MakeEdge(curve).Edge();
}

TopoDS_Edge arcEdge(const document::SketchParameters& sketch, const document::SketchArc& arc) {
    const gp_Pnt center = worldPoint(sketch.plane, arc.center);
    const gp_Dir normal(sketch.plane.normalX, sketch.plane.normalY, sketch.plane.normalZ);
    const gp_Dir xDirection(sketch.plane.xDirectionX, sketch.plane.xDirectionY,
                            sketch.plane.xDirectionZ);
    const Handle(Geom_Circle) curve =
        new Geom_Circle(gp_Ax2(center, normal, xDirection), arc.radius);
    return BRepBuilderAPI_MakeEdge(curve, arc.startAngle, arc.endAngle).Edge();
}

TopoDS_Edge ellipseEdge(const document::SketchParameters& sketch,
                        const document::SketchEllipse& ellipse) {
    const gp_Pnt center = worldPoint(sketch.plane, ellipse.center);
    const gp_Dir normal(sketch.plane.normalX, sketch.plane.normalY, sketch.plane.normalZ);
    const gp_Dir planeX(sketch.plane.xDirectionX, sketch.plane.xDirectionY,
                        sketch.plane.xDirectionZ);
    const gp_Dir planeY = normal.Crossed(planeX);
    const gp_Dir majorDirection(gp_Vec(planeX) * std::cos(ellipse.rotation) +
                                gp_Vec(planeY) * std::sin(ellipse.rotation));
    const Handle(Geom_Ellipse) curve = new Geom_Ellipse(gp_Ax2(center, normal, majorDirection),
                                                        ellipse.majorRadius, ellipse.minorRadius);
    return BRepBuilderAPI_MakeEdge(curve).Edge();
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
    const auto& arc = std::get<document::SketchArc>(geometry);
    return {{arc.center.x + arc.radius * std::cos(arc.startAngle),
             arc.center.y + arc.radius * std::sin(arc.startAngle)},
            {arc.center.x + arc.radius * std::cos(arc.endAngle),
             arc.center.y + arc.radius * std::sin(arc.endAngle)}};
}

TopoDS_Face curveProfileFace(const document::SketchParameters& sketch,
                             const SketchProfile& profile) {
    BRepBuilderAPI_MakeWire wire;
    std::optional<document::SketchPoint> current;
    for (document::SketchElementId id : profile.elementIds) {
        const auto& geometry = findElement(sketch, id).geometry;
        auto points = curveEndpoints(geometry);
        TopoDS_Edge edge = std::holds_alternative<document::SketchLine>(geometry)
                               ? BRepBuilderAPI_MakeEdge(worldPoint(sketch.plane, points.first),
                                                         worldPoint(sketch.plane, points.second))
                                     .Edge()
                               : arcEdge(sketch, std::get<document::SketchArc>(geometry));
        if (current.has_value() && connected(*current, points.second)) {
            edge.Reverse();
            std::swap(points.first, points.second);
        }
        // Preserve the original curves. OCCT may share vertices within this
        // explicit tolerance; no polygon reconstruction or endpoint snapping.
        BRep_Builder builder;
        const double tolerance = detail::connectionTolerance(points.first, points.second).millimetres;
        for (TopExp_Explorer vertex(edge, TopAbs_VERTEX); vertex.More(); vertex.Next())
            builder.UpdateVertex(TopoDS::Vertex(vertex.Current()), tolerance);
        wire.Add(edge);
        if (!wire.IsDone())
            throw GeometryEvaluationError("Sketch profile wire could not be connected");
        current = points.second;
    }
    BRepBuilderAPI_MakeFace face(wire.Wire());
    if (!face.IsDone() || !BRepCheck_Analyzer(face.Face()).IsValid())
        throw GeometryEvaluationError("Sketch profile did not produce a valid face");
    return face.Face();
}

TopoDS_Face profileFace(const document::SketchParameters& sketch, const SketchProfile& profile) {
    const document::SketchGeometry& first =
        findElement(sketch, profile.elementIds.front()).geometry;
    if (const auto* rectangle = std::get_if<document::SketchRectangle>(&first)) {
        return BRepBuilderAPI_MakeFace(rectangleWire(sketch, *rectangle)).Face();
    }
    if (const auto* circle = std::get_if<document::SketchCircle>(&first)) {
        return BRepBuilderAPI_MakeFace(BRepBuilderAPI_MakeWire(circleEdge(sketch, *circle)).Wire())
            .Face();
    }
    if (const auto* ellipse = std::get_if<document::SketchEllipse>(&first)) {
        return BRepBuilderAPI_MakeFace(
                   BRepBuilderAPI_MakeWire(ellipseEdge(sketch, *ellipse)).Wire())
            .Face();
    }
    return curveProfileFace(sketch, profile);
}

TopoDS_Shape elementShape(const document::SketchParameters& sketch,
                          const document::SketchGeometry& geometry) {
    if (const auto* line = std::get_if<document::SketchLine>(&geometry)) {
        return BRepBuilderAPI_MakeEdge(worldPoint(sketch.plane, line->start),
                                       worldPoint(sketch.plane, line->end))
            .Edge();
    }
    if (const auto* rectangle = std::get_if<document::SketchRectangle>(&geometry)) {
        return rectangleWire(sketch, *rectangle);
    }
    if (const auto* circle = std::get_if<document::SketchCircle>(&geometry))
        return circleEdge(sketch, *circle);
    if (const auto* arc = std::get_if<document::SketchArc>(&geometry))
        return arcEdge(sketch, *arc);
    return ellipseEdge(sketch, std::get<document::SketchEllipse>(geometry));
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
        result.push_back(EvaluatedSketchElement{
            element.id, elementShape(sketch, element.geometry), element.construction,
            elementPoints(sketch, element.geometry), radialRadius(element.geometry),
            radialAnchor(sketch, element.geometry)});
    }
    return result;
}

TopoDS_Shape evaluateSketchProfileFaces(const document::SketchParameters& sketch) {
    return evaluateSketchProfileFaces(sketch, {});
}

TopoDS_Shape
evaluateSketchProfileFaces(const document::SketchParameters& sketch,
                           std::span<const document::SketchProfileId> selectedProfiles) {
    detail::requireCanonicalPlane(sketch.plane);
    if (!sketch.elements.values.empty())
        document::validateSketchElements(sketch.elements);
    const std::vector<SketchProfile> profiles = detectClosedProfiles(sketch.elements.values);
    if (profiles.empty()) {
        throw GeometryEvaluationError("Sketch does not contain a closed profile");
    }
    TopoDS_Compound faces;
    BRep_Builder builder;
    builder.MakeCompound(faces);
    std::size_t selectedCount = 0;
    for (const SketchProfile& profile : profiles) {
        if (!selectedProfiles.empty() &&
            std::ranges::find(selectedProfiles, profile.id) == selectedProfiles.end()) {
            continue;
        }
        builder.Add(faces, profileFace(sketch, profile));
        ++selectedCount;
    }
    if (!selectedProfiles.empty() && selectedCount != selectedProfiles.size())
        throw GeometryEvaluationError("Extrusion references an unknown sketch profile");
    return faces;
}

} // namespace voelund::features
