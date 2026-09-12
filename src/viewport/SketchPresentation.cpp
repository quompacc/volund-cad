#include "SketchPresentation.hpp"

#include <Aspect_TypeOfMarker.hxx>
#include <Geom_CartesianPoint.hxx>
#include <Graphic3d_ZLayerId.hxx>
#include <Prs3d_PointAspect.hxx>
#include <Quantity_Color.hxx>
#include <Quantity_TypeOfColor.hxx>
#include <TopAbs_ShapeEnum.hxx>

#include <algorithm>

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
