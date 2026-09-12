#include <voelund/features/SketchEvaluation.hpp>

#include <voelund/features/SketchProfiles.hpp>

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRep_Builder.hxx>
#include <GC_MakeCircle.hxx>
#include <Geom_Circle.hxx>
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

constexpr double connectionTolerance = 1.0e-8;

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

const document::SketchElement& findElement(const document::SketchParameters& sketch,
                                           document::SketchElementId id) {
    const auto match = std::ranges::find(sketch.elements.values, id, &document::SketchElement::id);
    if (match == sketch.elements.values.end()) {
        throw std::runtime_error("Sketch profile references an unknown element");
    }
    return *match;
}

bool connected(const document::SketchPoint& first, const document::SketchPoint& second) {
    return std::hypot(second.x - first.x, second.y - first.y) <= connectionTolerance;
}

TopoDS_Face lineProfileFace(const document::SketchParameters& sketch,
                            const SketchProfile& profile) {
    BRepBuilderAPI_MakePolygon polygon;
    std::optional<document::SketchPoint> current;
    for (document::SketchElementId id : profile.elementIds) {
        const auto& line = std::get<document::SketchLine>(findElement(sketch, id).geometry);
        if (!current.has_value()) {
            polygon.Add(worldPoint(sketch.plane, line.start));
            current = line.end;
        } else if (connected(*current, line.start)) {
            current = line.end;
        } else {
            current = line.start;
        }
        polygon.Add(worldPoint(sketch.plane, *current));
    }
    polygon.Close();
    return BRepBuilderAPI_MakeFace(polygon.Wire()).Face();
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
        wire.Add(edge);
        current = points.second;
    }
    return BRepBuilderAPI_MakeFace(wire.Wire()).Face();
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
    if (std::ranges::any_of(profile.elementIds, [&sketch](document::SketchElementId id) {
            return std::holds_alternative<document::SketchArc>(findElement(sketch, id).geometry);
        }))
        return curveProfileFace(sketch, profile);
    return lineProfileFace(sketch, profile);
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
    return arcEdge(sketch, std::get<document::SketchArc>(geometry));
}

} // namespace

std::vector<EvaluatedSketchElement>
evaluateSketchElements(const document::SketchParameters& sketch) {
    std::vector<EvaluatedSketchElement> result;
    result.reserve(sketch.elements.values.size());
    for (const document::SketchElement& element : sketch.elements.values) {
        result.push_back(EvaluatedSketchElement{element.id, elementShape(sketch, element.geometry),
                                                element.construction,
                                                elementPoints(sketch, element.geometry)});
    }
    return result;
}

TopoDS_Shape evaluateSketchProfileFaces(const document::SketchParameters& sketch) {
    return evaluateSketchProfileFaces(sketch, {});
}

TopoDS_Shape
evaluateSketchProfileFaces(const document::SketchParameters& sketch,
                           std::span<const document::SketchProfileId> selectedProfiles) {
    const std::vector<SketchProfile> profiles = detectClosedProfiles(sketch.elements.values);
    if (profiles.empty()) {
        throw std::runtime_error("Sketch does not contain a closed profile");
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
        throw std::runtime_error("Extrusion references an unknown sketch profile");
    return faces;
}

} // namespace voelund::features
