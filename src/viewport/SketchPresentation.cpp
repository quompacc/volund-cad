#include "SketchPresentation.hpp"

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
                                     std::span<const std::uint64_t> selectedIds) {
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
        context->Display(presentation, false);
        context->Activate(presentation, AIS_Shape::SelectionMode(TopAbs_EDGE));
        elements_.push_back(ElementPresentation{element.id, presentation, element.conflicting,
                                                element.construction});
    }
}

void SketchPresentation::clear(const occ::handle<AIS_InteractiveContext>& context) {
    if (!context.IsNull()) {
        for (const ElementPresentation& element : elements_) {
            context->Remove(element.shape, false);
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
    }
    return std::nullopt;
}

std::size_t SketchPresentation::size() const noexcept { return elements_.size(); }

} // namespace voelund::viewport
