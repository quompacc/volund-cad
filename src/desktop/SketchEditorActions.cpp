// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchEditorToolController.hpp"
#include "ToolPanelHost.hpp"

#include <QString>

#include <map>
#include <stdexcept>
#include <string>

// The sketch editor's commands by their panel and shortcut IDs.
namespace voelund::desktop {
namespace {
using document::SketchConstraintKind;

// Commands that add a relation or dimension to the selection.
const std::map<std::string, SketchConstraintKind>& relationCommands() {
    static const std::map<std::string, SketchConstraintKind> commands{
        {"coincident", SketchConstraintKind::coincident},
        {"parallel", SketchConstraintKind::parallel},
        {"perpendicular", SketchConstraintKind::perpendicular},
        {"collinear", SketchConstraintKind::collinear},
        {"horizontal_points", SketchConstraintKind::horizontalPoints},
        {"vertical_points", SketchConstraintKind::verticalPoints},
        {"fixed", SketchConstraintKind::fixed},
        {"equal", SketchConstraintKind::equal},
        {"tangent", SketchConstraintKind::tangent},
        {"symmetric", SketchConstraintKind::symmetric},
        {"midpoint", SketchConstraintKind::midpoint},
        {"perpendicular_bisector", SketchConstraintKind::perpendicularBisector},
        {"concentric", SketchConstraintKind::concentric},
        {"diameter", SketchConstraintKind::diameter},
        {"radius", SketchConstraintKind::radius},
        {"point_distance", SketchConstraintKind::pointDistance},
        {"point_line_distance", SketchConstraintKind::pointLineDistance},
        {"point_distance_x", SketchConstraintKind::pointDistanceX},
        {"point_distance_y", SketchConstraintKind::pointDistanceY},
        {"angle", SketchConstraintKind::angle},
        {"point_on_object", SketchConstraintKind::pointOnObject}};
    return commands;
}

bool isEditOperation(const std::string& id) {
    for (const char* operation :
         {"trim", "extend", "split", "fillet", "chamfer", "rotate", "scale", "offset", "mirror",
          "linear_pattern", "circular_pattern", "edit_pattern", "dissolve_pattern", "project",
          "section", "import_dxf"})
        if (id == operation)
            return true;
    return false;
}
} // namespace

void SketchEditorToolController::runAction(const std::string& id) {
    if (const auto relation = relationCommands().find(id); relation != relationCommands().end())
        return applyRelation(relation->second);
    if (isEditOperation(id))
        return applyEditOperation(id);
    if (id == "horizontal")
        applyLineConstraint(document::SketchLineConstraint::horizontal);
    else if (id == "vertical")
        applyLineConstraint(document::SketchLineConstraint::vertical);
    else if (id == "free")
        applyLineConstraint(document::SketchLineConstraint::none);
    else if (id == "construction")
        toggleConstruction();
    else if (id == "lock_point")
        lockSelectedReference();
    else if (id == "unlock_point")
        unlockSelectedReference();
    else if (id == "remove_constraint")
        removeLastConstraint();
    else if (id == "undo_sketch" || id == "redo_sketch")
        stepHistory(id == "undo_sketch");
    else if (id == "toggle_reference")
        toggleReference();
    else if (id == "delete")
        removeSelected();
}

// S02: selection and solve status come back with the step.
void SketchEditorToolController::stepHistory(bool back) {
    if (!(back ? editor_->undo() : editor_->redo()))
        return;
    syncSelectedDimensionInputs();
    syncConstraintState();
    refreshPresentation();
}

// W13: a rectangle just drawn gets its rounded corners and frame; a refusal (too small) keeps
// the plain rectangle and says why.
void SketchEditorToolController::applyRectangleOptions(
    const application::SketchCreationResult& created, const viewport::SketchElementIntent& intent) {
    if (toolIndex_ != rectangleTool && toolIndex_ != centerRectangleTool)
        return;
    auto options = rectangleOptions_;
    options.radius = intent.cornerRadius; // W13b: from their steps
    options.frameWidth = intent.frameWidth;
    try {
        editor_->finishRectangle(created, options);
    } catch (const std::invalid_argument& error) {
        statusChanged_(QString::fromUtf8(error.what()));
    }
}

// S13: "Bemaßungen: Aus" hides the dimensions, relations stay; W13: coincident points stay
// quiet (SketchEditorPresentation.cpp).
bool SketchEditorToolController::hiddenAnnotation(
    const features::SketchConstraintAnnotation& annotation) const {
    if (!showDimensions_ && annotation.category == features::SketchAnnotationCategory::dimension)
        return true;
    return annotation.kind == document::SketchConstraintKind::coincident &&
           !editor_->isConstraintConflicting(annotation.id) &&
           !editor_->isConstraintRedundant(annotation.id) &&
           editor_->selectedConstraint() != annotation.id;
}

// W13: the rectangle options show while a rectangle tool is on.
void SketchEditorToolController::syncRectangleOptions() {
    const bool rectangle = toolIndex_ == rectangleTool || toolIndex_ == centerRectangleTool;
    panel_.setChoiceVisible("rounded_corners", rectangle);
    panel_.setChoiceVisible("frame", rectangle);
}

void SketchEditorToolController::chooseOption(const std::string& id, int index) {
    if (id == "tool")
        setTool(index);
    else if (id == "pattern_reference") // N14a
        patternFromLast_ = index == 1;
    else if (id == "rounded_corners" || id == "frame") {
        (id == "frame" ? rectangleOptions_.frame : rectangleOptions_.rounded) = index == 1;
        viewport_.setSketchRectangleOptions(rectangleOptions_.rounded, rectangleOptions_.frame);
    } else
        applyViewSwitch(id, index == 0);
}

} // namespace voelund::desktop
