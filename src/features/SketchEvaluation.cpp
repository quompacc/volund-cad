#include <voelund/features/SketchEvaluation.hpp>

#include <voelund/features/SketchProfiles.hpp>

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRep_Builder.hxx>
#include <GC_MakeCircle.hxx>
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
    return circleEdge(sketch, std::get<document::SketchCircle>(geometry));
}

} // namespace

std::vector<EvaluatedSketchElement>
evaluateSketchElements(const document::SketchParameters& sketch) {
    std::vector<EvaluatedSketchElement> result;
    result.reserve(sketch.elements.values.size());
    for (const document::SketchElement& element : sketch.elements.values) {
        result.push_back(EvaluatedSketchElement{element.id, elementShape(sketch, element.geometry),
                                                element.construction});
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
