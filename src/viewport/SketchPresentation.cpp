#include "SketchPresentation.hpp"

#include <Aspect_TypeOfMarker.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <Geom_CartesianPoint.hxx>
#include <Graphic3d_ZLayerId.hxx>
#include <Prs3d_Drawer.hxx>
#include <Prs3d_LineAspect.hxx>
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

const Quantity_Color selectedColor(1.0, 0.65, 0.1, Quantity_TOC_RGB);
const Quantity_Color conflictingColor(1.0, 0.15, 0.12, Quantity_TOC_RGB);

bool selected(std::span<const std::uint64_t> selectedIds, std::uint64_t id) {
    return std::ranges::find(selectedIds, id) != selectedIds.end();
}

// Color by state: conflict, selection, then free or fully determined (user decision
// 2026-10-05). Construction geometry is dashed and thinner in the same colors.
void applyStyle(const occ::handle<AIS_Shape>& shape, bool isSelected, bool conflicting,
                bool construction, const Quantity_Color& stateColor) {
    const Quantity_Color& color = conflicting  ? conflictingColor
                                  : isSelected ? selectedColor
                                               : stateColor;
    const double width = isSelected || conflicting ? 5.0 : construction ? 2.0 : 3.0;
    shape->SetColor(color);
    shape->SetWidth(width);
    // Own aspects: the drawer may still share the context's defaults.
    const auto type = construction ? Aspect_TOL_DASH : Aspect_TOL_SOLID;
    const occ::handle<Prs3d_Drawer>& drawer = shape->Attributes();
    drawer->SetWireAspect(new Prs3d_LineAspect(color, type, width));
    drawer->SetLineAspect(new Prs3d_LineAspect(color, type, width));
    drawer->SetFreeBoundaryAspect(new Prs3d_LineAspect(color, type, width));
    drawer->SetUnFreeBoundaryAspect(new Prs3d_LineAspect(color, type, width));
    // S15: a sketch point is drawn as a filled marker in the same color.
    drawer->SetPointAspect(new Prs3d_PointAspect(Aspect_TOM_BALL, color, 3.0 + width));
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
        applyStyle(presentation, selected(selectedIds, element.id), element.conflicting,
                   element.construction, element.determined ? determined_ : free_);
        if (showPoints)
            presentation->SetZLayer(Graphic3d_ZLayerId_Topmost);
        context->Display(presentation, false);
        // S15: a point is a vertex and is picked as one.
        context->Activate(presentation,
                          AIS_Shape::SelectionMode(element.shape.ShapeType() == TopAbs_VERTEX
                                                       ? TopAbs_VERTEX
                                                       : TopAbs_EDGE));
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
                                                element.conflicting, element.construction,
                                                element.determined});
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
        applyStyle(element.shape, selected(selectedIds, element.id), element.conflicting,
                   element.construction, element.determined ? determined_ : free_);
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

void SketchPresentation::setColors(const Quantity_Color& free, const Quantity_Color& determined) {
    free_ = free;
    determined_ = determined;
}

std::optional<Quantity_Color> SketchPresentation::color(std::uint64_t id) const {
    for (const ElementPresentation& element : elements_)
        if (element.id == id)
            return element.shape->Attributes()->WireAspect()->Aspect()->Color();
    return std::nullopt;
}

std::optional<bool> SketchPresentation::dashed(std::uint64_t id) const {
    for (const ElementPresentation& element : elements_)
        if (element.id == id)
            return element.shape->Attributes()->WireAspect()->Aspect()->LineType() ==
                   Aspect_TOL_DASH;
    return std::nullopt;
}

} // namespace voelund::viewport
