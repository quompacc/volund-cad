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
    enable("radius", document::SketchConstraintKind::radius);
    enable("point_distance", document::SketchConstraintKind::pointDistance);
    enable("point_distance_x", document::SketchConstraintKind::pointDistanceX);
    enable("point_distance_y", document::SketchConstraintKind::pointDistanceY);
    enable("angle", document::SketchConstraintKind::angle);
    enable("point_on_object", document::SketchConstraintKind::pointOnObject);
    panel_.setActionEnabled("lock_point", editor_->canLockSelectedReference());
    panel_.setActionEnabled("unlock_point", editor_->isSelectedReferenceLocked());
    panel_.setActionEnabled("construction", editor_->selectedElement().has_value());
    const bool hasSelection = editor_->selectedElement().has_value();
    // S10: only for the curve types each edit supports (SketchCurveEdit.hpp).
    panel_.setActionEnabled("trim", editor_->canEditCurve(application::SketchCurveEdit::trim));
    panel_.setActionEnabled("extend", editor_->canEditCurve(application::SketchCurveEdit::extend));
    panel_.setActionEnabled("split", editor_->canEditCurve(application::SketchCurveEdit::split));
    panel_.setActionEnabled("offset", hasSelection);
    panel_.setActionEnabled("rotate", hasSelection); // S20
    panel_.setActionEnabled("scale", hasSelection);
    panel_.setActionEnabled("linear_pattern", hasSelection);
    panel_.setActionEnabled("circular_pattern", hasSelection);
    // S12: a selected source or copy names its pattern.
    const auto pattern = editor_->selectedPattern();
    panel_.setActionEnabled("edit_pattern",
                            pattern && pattern->kind != document::SketchPatternKind::mirror);
    panel_.setActionEnabled("dissolve_pattern", pattern.has_value());
    panel_.setActionEnabled("fillet", editor_->selectedReferences().size() >= 2);
    panel_.setActionEnabled("chamfer", editor_->selectedReferences().size() >= 2);
    panel_.setActionEnabled("mirror", editor_->selectedReferences().size() >= 2);
    panel_.setActionEnabled("remove_constraint", !editor_->constraints().empty());
    const auto constraint = editor_->selectedConstraint();
    panel_.setActionEnabled("toggle_reference",
                            constraint && (editor_->isReferenceDimension(*constraint) ||
                                           !editor_->dimensionProblem(*constraint)));
    panel_.setActionEnabled("delete", hasSelection || editor_->selectedConstraint().has_value());
    panel_.setActionEnabled("undo_sketch", editor_->canUndo());
    panel_.setActionEnabled("redo_sketch", editor_->canRedo());
    panel_.setAcceptEnabled(editor_->canAccept());
    // The state stays visible in its own line; tool hints use the guidance below it (user
    // request 2026-10-05). Geometry is colored by the same state.
    const auto& result = editor_->lastSolveResult();
    if (result.status == features::SketchSolveStatus::conflicting) {
        panel_.setStatus(
            QObject::tr("Widerspruch: %1 Bedingung(en)").arg(result.conflictingConstraints.size()));
        panel_.setGuidance(QObject::tr("Rote Geometrie widerspricht sich. Entferne eine "
                                       "Bedingung oder mache rückgängig."));
    } else if (result.status == features::SketchSolveStatus::failed) {
        panel_.setStatus(QObject::tr("Löser fehlgeschlagen"));
        panel_.setGuidance(QObject::tr("Korrigiere die Skizze oder mache rückgängig; OK ist "
                                       "gesperrt."));
    } else if (!result.redundantConstraints.empty()) {
        // S07: redundant relations take no freedom; the count would not reach zero.
        panel_.setStatus(
            QObject::tr("Überflüssig: %1 Bedingung(en)").arg(result.redundantConstraints.size()));
        panel_.setGuidance(QObject::tr("Gelbe Bedingungen folgen schon aus anderen. Entferne "
                                       "sie, dann stimmen die Freiheitsgrade."));
    } else if (editor_->elements().empty()) {
        panel_.setStatus({});
        panel_.setGuidance(QObject::tr("Zeichne Skizzengeometrie, um zu beginnen."));
    } else if (result.degreesOfFreedom == 0) {
        panel_.setStatus(QObject::tr("✓ Vollständig bestimmt"), true);
        panel_.setGuidance(QObject::tr("Alle Elemente sind festgelegt (dunkel)."));
    } else {
        panel_.setStatus(QObject::tr("Freiheitsgrade: %1").arg(result.degreesOfFreedom));
        panel_.setGuidance(QObject::tr("Blaue Elemente sind noch frei. Bemaßungen und "
                                       "Beziehungen legen sie fest; Strg-Klick wählt mehrere "
                                       "Bezüge."));
    }
}

} // namespace voelund::desktop
