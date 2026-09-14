#include "SketchEditorToolController.hpp"

#include "ToolPanelHost.hpp"

#include <QObject>

namespace voelund::desktop {

void SketchEditorToolController::syncConstraintState() {
    panel_.setErrorCommitHandler({});
    if (!editor_)
        return;
    const auto enable = [this](const char* id, document::SketchConstraintKind kind) {
        panel_.setActionEnabled(id, editor_->canAddSelectedRelation(kind));
    };
    enable("coincident", document::SketchConstraintKind::coincident);
    enable("parallel", document::SketchConstraintKind::parallel);
    enable("perpendicular", document::SketchConstraintKind::perpendicular);
    enable("collinear", document::SketchConstraintKind::collinear);
    enable("horizontal_points", document::SketchConstraintKind::horizontalPoints);
    enable("vertical_points", document::SketchConstraintKind::verticalPoints);
    enable("fixed", document::SketchConstraintKind::fixed);
    enable("point_line_distance", document::SketchConstraintKind::pointLineDistance);
    enable("equal", document::SketchConstraintKind::equal);
    enable("tangent", document::SketchConstraintKind::tangent);
    enable("symmetric", document::SketchConstraintKind::symmetric);
    enable("midpoint", document::SketchConstraintKind::midpoint);
    enable("perpendicular_bisector", document::SketchConstraintKind::perpendicularBisector);
    enable("concentric", document::SketchConstraintKind::concentric);
    enable("diameter", document::SketchConstraintKind::diameter);
    enable("point_distance", document::SketchConstraintKind::pointDistance);
    enable("point_distance_x", document::SketchConstraintKind::pointDistanceX);
    enable("point_distance_y", document::SketchConstraintKind::pointDistanceY);
    enable("angle", document::SketchConstraintKind::angle);
    enable("point_on_object", document::SketchConstraintKind::pointOnObject);
    panel_.setActionEnabled("lock_point", editor_->canLockSelectedReference());
    panel_.setActionEnabled("unlock_point", editor_->isSelectedReferenceLocked());
    panel_.setActionEnabled("construction", editor_->selectedElement().has_value());
    const bool hasSelection = editor_->selectedElement().has_value();
    panel_.setActionEnabled("trim", hasSelection);
    panel_.setActionEnabled("extend", hasSelection);
    panel_.setActionEnabled("split", hasSelection);
    panel_.setActionEnabled("offset", hasSelection);
    panel_.setActionEnabled("linear_pattern", hasSelection);
    panel_.setActionEnabled("circular_pattern", hasSelection);
    panel_.setActionEnabled("fillet", editor_->selectedReferences().size() >= 2);
    panel_.setActionEnabled("mirror", editor_->selectedReferences().size() >= 2);
    panel_.setActionEnabled("remove_constraint", !editor_->constraints().empty());
    panel_.setActionEnabled("undo_sketch", editor_->canUndo());
    panel_.setActionEnabled("redo_sketch", editor_->canRedo());
    panel_.setAcceptEnabled(editor_->canAccept());
    const auto& result = editor_->lastSolveResult();
    if (result.status == features::SketchSolveStatus::conflicting) {
        panel_.setGuidance(QObject::tr("Conflicting sketch: %1 conflicting constraint(s).")
                               .arg(result.conflictingConstraints.size()));
    } else if (result.status == features::SketchSolveStatus::failed) {
        panel_.setGuidance(
            QObject::tr("Sketch solver failed. Correct the sketch or undo; OK is disabled."));
    } else if (result.degreesOfFreedom == 0) {
        panel_.setGuidance(QObject::tr("Sketch fully constrained."));
    } else if (result.degreesOfFreedom > 0) {
        panel_.setGuidance(QObject::tr("Sketch underconstrained: %1 degree(s) of freedom. "
                                       "Ctrl-click adds relation references.")
                               .arg(result.degreesOfFreedom));
    } else {
        panel_.setGuidance(QObject::tr("Draw sketch geometry to begin."));
    }
}

} // namespace voelund::desktop
