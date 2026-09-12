#include "SketchPresentation.hpp"

#include <Aspect_TypeOfMarker.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <Geom_CartesianPoint.hxx>
#include <Graphic3d_ZLayerId.hxx>
#include <Prs3d_PointAspect.hxx>
#include <Quantity_Color.hxx>
#include <Quantity_TypeOfColor.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS_Edge.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <array>

namespace voelund::viewport {
namespace {

const Quantity_Color regularColor(0.15, 0.9, 0.4, Quantity_TOC_RGB);
const Quantity_Color selectedColor(1.0, 0.65, 0.1, Quantity_TOC_RGB);
const Quantity_Color conflictingColor(1.0, 0.15, 0.12, Quantity_TOC_RGB);
const Quantity_Color constructionColor(0.2, 0.65, 1.0, Quantity_TOC_RGB);

bool selected(std::span<const std::uint64_t> selectedIds, std::uint64_t id) {
    return std::ranges::find(selectedIds, id) != selectedIds.end();
}

} // namespace

void SketchPresentation::setElements(const occ::handle<AIS_InteractiveContext>& context,
                                     std::span<const SceneSketchElement> elements,
                                     std::span<const std::uint64_t> selectedIds, bool showPoints) {
    clear(context);
    if (context.IsNull()) {
        return;
    }
    elements_.reserve(elements.size());
    for (const SceneSketchElement& element : elements) {
        occ::handle<AIS_Shape> presentation = new AIS_Shape(element.shape);
        const bool isSelected = selected(selectedIds, element.id);
        presentation->SetColor(element.conflicting    ? conflictingColor
                               : isSelected           ? selectedColor
                               : element.construction ? constructionColor
                                                      : regularColor);
        presentation->SetWidth(isSelected || element.conflicting ? 5.0
                               : element.construction            ? 2.0
                                                                 : 3.0);
        if (showPoints)
            presentation->SetZLayer(Graphic3d_ZLayerId_Topmost);
        context->Display(presentation, false);
        context->Activate(presentation, AIS_Shape::SelectionMode(TopAbs_EDGE));
        std::vector<occ::handle<AIS_Point>> points;
        if (showPoints) {
            points.reserve(element.points.size());
            for (const SceneSketchPoint& point : element.points) {
                occ::handle<Geom_CartesianPoint> geometry =
                    new Geom_CartesianPoint(point.worldX, point.worldY, point.worldZ);
                occ::handle<AIS_Point> marker = new AIS_Point(geometry);
                marker->Attributes()->SetPointAspect(
                    new Prs3d_PointAspect(Aspect_TOM_O_POINT, selectedColor, 5.0));
                marker->SetZLayer(Graphic3d_ZLayerId_Topmost);
                context->Display(marker, false);
                points.push_back(marker);
            }
        }
        elements_.push_back(ElementPresentation{element.id, presentation, std::move(points),
                                                element.conflicting, element.construction});
    }
}

void SketchPresentation::clear(const occ::handle<AIS_InteractiveContext>& context) {
    if (!context.IsNull()) {
        for (const ElementPresentation& element : elements_) {
            context->Remove(element.shape, false);
            for (const auto& point : element.points)
                context->Remove(point, false);
        }
    }
    elements_.clear();
}

void SketchPresentation::setReferences(const occ::handle<AIS_InteractiveContext>& context,
                                       const PlanarFaceSelection& support, bool visible) {
    if (!context.IsNull()) {
        for (const auto& axis : referenceAxes_)
            context->Remove(axis, false);
        for (const auto& edge : projectedEdges_)
            context->Remove(edge, false);
        if (!origin_.IsNull())
            context->Remove(origin_, false);
    }
    referenceAxes_.clear();
    projectedEdges_.clear();
    origin_.Nullify();
    if (!visible || context.IsNull())
        return;
    const gp_Pnt origin(support.originX, support.originY, support.originZ);
    const gp_Dir normal(support.normalX, support.normalY, support.normalZ);
    const gp_Dir xDirection(support.xDirectionX, support.xDirectionY, support.xDirectionZ);
    const gp_Dir yDirection = normal.Crossed(xDirection);
    constexpr double extent = 1000.0;
    const std::array directions{xDirection, yDirection};
    const std::array colors{Quantity_Color(0.95, 0.25, 0.2, Quantity_TOC_RGB),
                            Quantity_Color(0.2, 0.65, 1.0, Quantity_TOC_RGB)};
    for (std::size_t index = 0; index < directions.size(); ++index) {
        const gp_Vec offset(directions[index]);
        occ::handle<AIS_Shape> axis = new AIS_Shape(
            BRepBuilderAPI_MakeEdge(origin.Translated(offset * -extent),
                                    origin.Translated(offset * extent))
                .Edge());
        axis->SetColor(colors[index]);
        axis->SetWidth(1.5);
        axis->SetZLayer(Graphic3d_ZLayerId_Topmost);
        context->Display(axis, false);
        referenceAxes_.push_back(axis);
    }
    for (const PlanarReferenceSegment& segment : support.referenceSegments) {
        occ::handle<AIS_Shape> edge = new AIS_Shape(
            BRepBuilderAPI_MakeEdge(gp_Pnt(segment.firstWorldX, segment.firstWorldY,
                                          segment.firstWorldZ),
                                    gp_Pnt(segment.secondWorldX, segment.secondWorldY,
                                          segment.secondWorldZ))
                .Edge());
        edge->SetColor(Quantity_Color(0.75, 0.45, 1.0, Quantity_TOC_RGB));
        edge->SetWidth(2.0);
        edge->SetZLayer(Graphic3d_ZLayerId_Topmost);
        context->Display(edge, false);
        projectedEdges_.push_back(edge);
    }
    origin_ = new AIS_Point(new Geom_CartesianPoint(origin));
    origin_->Attributes()->SetPointAspect(
        new Prs3d_PointAspect(Aspect_TOM_PLUS, selectedColor, 7.0));
    origin_->SetZLayer(Graphic3d_ZLayerId_Topmost);
    context->Display(origin_, false);
}

void SketchPresentation::select(const occ::handle<AIS_InteractiveContext>& context,
                                std::span<const std::uint64_t> selectedIds) {
    if (context.IsNull()) {
        return;
    }
    for (const ElementPresentation& element : elements_) {
        const bool isSelected = selected(selectedIds, element.id);
        element.shape->SetColor(element.conflicting    ? conflictingColor
                                : isSelected           ? selectedColor
                                : element.construction ? constructionColor
                                                       : regularColor);
        element.shape->SetWidth(isSelected || element.conflicting ? 5.0
                                : element.construction            ? 2.0
                                                                  : 3.0);
        context->Redisplay(element.shape, false);
    }
}

std::optional<std::uint64_t> SketchPresentation::detectedElement(
    const occ::handle<AIS_InteractiveObject>& detected) const noexcept {
    for (const ElementPresentation& element : elements_) {
        if (element.shape == detected) {
            return element.id;
        }
        if (std::ranges::find(element.points, detected) != element.points.end())
            return element.id;
    }
    return std::nullopt;
}

std::size_t SketchPresentation::size() const noexcept { return elements_.size(); }

} // namespace voelund::viewport
