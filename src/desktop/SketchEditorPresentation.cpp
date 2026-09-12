#include "SketchEditorToolController.hpp"

#include "SketchConstraintLabel.hpp"
#include "SketchViewAdapter.hpp"
#include "ToolPanelHost.hpp"

#include <QObject>

#include <algorithm>
#include <cmath>
#include <vector>

namespace voelund::desktop {

void SketchEditorToolController::applyEditOperation(const std::string& id) {
    if (!editor_)
        return;
    const bool changed = id == "trim"       ? editor_->trimSelected()
                         : id == "extend"    ? editor_->extendSelected()
                         : id == "split"     ? editor_->splitSelected()
                         : id == "fillet"    ? editor_->filletSelected(filletRadius_)
                         : id == "offset"    ? editor_->offsetSelected(offsetDistance_)
                         : id == "mirror"    ? editor_->mirrorSelected()
                         : id == "linear_pattern"
                             ? editor_->linearPatternSelected(
                                   static_cast<int>(std::lround(patternCount_)), patternSpacing_)
                         : id == "circular_pattern"
                             ? editor_->circularPatternSelected(
                                   static_cast<int>(std::lround(patternCount_)), patternAngle_)
                             : false;
    if (!changed)
        return;
    resetTranslationInputs();
    syncSelectedDimensionInputs();
    syncConstraintState();
    refreshPresentation();
    statusChanged_(QObject::tr("Sketch operation completed"));
}

void SketchEditorToolController::syncSelectedDimensionInputs() {
    if (!editor_)
        return;
    const auto length = editor_->selectedLineLength();
    const auto width = editor_->selectedRectangleWidth();
    const auto height = editor_->selectedRectangleHeight();
    const auto radius = editor_->selectedCircleRadius();
    const bool selecting = toolIndex_ == 0;
    const bool hasSelection = editor_->selectedElement().has_value();
    panel_.setLengthVisible("translation_x", selecting && hasSelection);
    panel_.setLengthVisible("translation_y", selecting && hasSelection);
    panel_.setLengthVisible("line_length", length.has_value());
    panel_.setLengthVisible("rectangle_width", width.has_value());
    panel_.setLengthVisible("rectangle_height", height.has_value());
    panel_.setLengthVisible("circle_radius", radius.has_value());
    panel_.setLengthVisible("polygon_sides", toolIndex_ == 9);
    panel_.setLengthVisible("fillet_radius", selecting && editor_->selectedReferences().size() >= 2);
    panel_.setLengthVisible("offset_distance", selecting && hasSelection);
    panel_.setLengthVisible("pattern_count", selecting && hasSelection);
    panel_.setLengthVisible("pattern_spacing", selecting && hasSelection);
    panel_.setLengthVisible("pattern_angle", selecting && hasSelection);
    panel_.setLengthEnabled("line_length", length.has_value());
    panel_.setLengthEnabled("rectangle_width", width.has_value());
    panel_.setLengthEnabled("rectangle_height", height.has_value());
    panel_.setLengthEnabled("circle_radius", radius.has_value());
    panel_.setActionEnabled("horizontal", length.has_value());
    panel_.setActionEnabled("vertical", length.has_value());
    panel_.setActionEnabled("free", length.has_value());
    if (length.has_value())
        panel_.setLengthValue("line_length", *length);
    if (width.has_value())
        panel_.setLengthValue("rectangle_width", *width);
    if (height.has_value())
        panel_.setLengthValue("rectangle_height", *height);
    if (radius.has_value())
        panel_.setLengthValue("circle_radius", *radius);
}

void SketchEditorToolController::resetTranslationInputs() {
    translationX_ = 0.0;
    translationY_ = 0.0;
    panel_.setLengthValue("translation_x", 0.0);
    panel_.setLengthValue("translation_y", 0.0);
}

void SketchEditorToolController::refreshPresentation() {
    if (!editor_)
        return;
    editor_->commitHistory();
    const auto evaluated = editor_->evaluatedElements();
    std::vector<viewport::SceneSketchElement> elements;
    elements.reserve(evaluated.size());
    for (const features::EvaluatedSketchElement& element : evaluated) {
        auto presentation =
            makeSceneSketchElement(element, editor_->isElementConflicting(element.id));
        if (std::ranges::find(editor_->elements(), element.id, &document::SketchElement::id) ==
            editor_->elements().end())
            presentation.points.clear();
        elements.push_back(std::move(presentation));
    }
    viewport_.setSketchElements(elements);
    std::vector<std::uint64_t> selectedIds;
    for (const auto& reference : editor_->selectedReferences())
        if (std::ranges::find(selectedIds, reference.element.value()) == selectedIds.end())
            selectedIds.push_back(reference.element.value());
    if (selectedIds.empty() && editor_->selectedElement().has_value())
        selectedIds.push_back(editor_->selectedElement()->value());
    viewport_.setSelectedSketchElements(selectedIds);
    const auto annotations = editor_->evaluatedConstraints();
    std::vector<viewport::SceneSketchConstraint> constraints;
    constraints.reserve(annotations.size());
    for (const features::SketchConstraintAnnotation& annotation : annotations) {
        const bool editable = annotation.kind == document::SketchConstraintKind::distance ||
                              annotation.kind == document::SketchConstraintKind::radius ||
                              annotation.kind == document::SketchConstraintKind::diameter ||
                              annotation.kind == document::SketchConstraintKind::pointDistance ||
                              annotation.kind ==
                                  document::SketchConstraintKind::pointLineDistance ||
                              annotation.kind == document::SketchConstraintKind::pointDistanceX ||
                              annotation.kind == document::SketchConstraintKind::pointDistanceY ||
                              annotation.kind == document::SketchConstraintKind::angle;
        constraints.push_back({annotation.id.value(), sketchConstraintLabel(annotation),
                               annotation.position.X(), annotation.position.Y(),
                               annotation.position.Z(),
                               editor_->isConstraintConflicting(annotation.id), editable});
    }
    if (previewAutomatic_.has_value() && evaluated.size() > editor_->elements().size() &&
        !evaluated.back().points.empty()) {
        const auto& first = evaluated.back().points[0].world;
        const gp_Pnt second = evaluated.back().points.size() >= 2
                                  ? evaluated.back().points[1].world
                                  : evaluated.back().radialAnchor.value_or(first);
        const auto previewLabel = *previewAutomatic_ == viewport::SketchAutomaticConstraint::horizontal
                                      ? "[H?]"
                                  : *previewAutomatic_ == viewport::SketchAutomaticConstraint::vertical
                                      ? "[V?]"
                                  : *previewAutomatic_ == viewport::SketchAutomaticConstraint::parallel
                                      ? "[P?]"
                                  : *previewAutomatic_ == viewport::SketchAutomaticConstraint::perpendicular
                                      ? "[⊥?]"
                                  : *previewAutomatic_ == viewport::SketchAutomaticConstraint::tangent
                                      ? "[T?]"
                                  : *previewAutomatic_ == viewport::SketchAutomaticConstraint::equal
                                      ? "[=?]"
                                  : *previewAutomatic_ == viewport::SketchAutomaticConstraint::concentric
                                      ? "[CC?]"
                                      : "[O?]";
        constraints.push_back(
            {0, previewLabel,
             (first.X() + second.X()) * 0.5, (first.Y() + second.Y()) * 0.5,
             (first.Z() + second.Z()) * 0.5, false, false});
    }
    viewport_.setSketchConstraints(constraints);
}

} // namespace voelund::desktop
