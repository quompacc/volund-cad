#include "SketchEditorToolController.hpp"

#include "SketchViewAdapter.hpp"
#include "ToolPanelHost.hpp"

#include <QObject>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace voelund::desktop {
namespace {

QString relationName(document::SketchConstraintKind kind) {
    using Kind = document::SketchConstraintKind;
    switch (kind) {
    case Kind::horizontal: return QObject::tr("Waagrecht");
    case Kind::vertical: return QObject::tr("Senkrecht");
    case Kind::distance: return QObject::tr("Länge");
    case Kind::radius: return QObject::tr("Radius");
    case Kind::diameter: return QObject::tr("Durchmesser");
    case Kind::coincident: return QObject::tr("Deckungsgleich");
    case Kind::parallel: return QObject::tr("Parallel");
    case Kind::perpendicular: return QObject::tr("Rechtwinklig");
    case Kind::coordinateX: return QObject::tr("Koordinate X");
    case Kind::coordinateY: return QObject::tr("Koordinate Y");
    case Kind::pointOnObject: return QObject::tr("Punkt auf Objekt");
    case Kind::equal: return QObject::tr("Gleich");
    case Kind::tangent: return QObject::tr("Tangential");
    case Kind::symmetric: return QObject::tr("Symmetrisch");
    case Kind::pointDistance: return QObject::tr("Distanz");
    case Kind::angle: return QObject::tr("Winkel");
    case Kind::pointDistanceX: return QObject::tr("Abstand X");
    case Kind::pointDistanceY: return QObject::tr("Abstand Y");
    case Kind::midpoint: return QObject::tr("Mittelpunkt");
    case Kind::perpendicularBisector: return QObject::tr("Halbierend");
    case Kind::concentric: return QObject::tr("Konzentrisch");
    case Kind::collinear: return QObject::tr("Kollinear");
    case Kind::horizontalPoints: return QObject::tr("Waagrecht ausgerichtet");
    case Kind::verticalPoints: return QObject::tr("Senkrecht ausgerichtet");
    case Kind::fixed: return QObject::tr("Fixiert");
    case Kind::pointLineDistance: return QObject::tr("Punkt–Linie");
    }
    return {};
}

} // namespace

// S10/S11: the reason when the edit cannot run, with the elements causing it shown in red;
// after it, the relations it had to remove.
void SketchEditorToolController::applyCurveEdit(
    const std::function<application::SketchCurveEditResult()>& edit) {
    application::SketchCurveEditResult result;
    problemElements_.clear();
    try {
        result = edit();
    } catch (const application::SketchEditRefusal& refusal) {
        for (const auto id : refusal.elements())
            problemElements_.push_back(id.value());
        refreshPresentation();
        statusChanged_(QString::fromStdString(refusal.what()));
        return;
    } catch (const std::invalid_argument& error) {
        refreshPresentation();
        statusChanged_(QString::fromStdString(error.what()));
        return;
    }
    resetTranslationInputs();
    syncSelectedDimensionInputs();
    syncConstraintState();
    refreshPresentation();
    if (result.removed.empty()) {
        statusChanged_(QObject::tr("Skizzenoperation abgeschlossen"));
        return;
    }
    QStringList names;
    for (const auto& constraint : result.removed)
        names << relationName(constraint.kind);
    statusChanged_(QObject::tr("Skizzenoperation abgeschlossen; entfernte Bedingungen, die nicht "
                               "mehr gelten: %1")
                       .arg(names.join(QStringLiteral(", "))));
}

// S12: associative patterns; a refusal names its reason, a legacy rectangle is copied once.
void SketchEditorToolController::applyPatternOperation(const std::string& id) {
    const int count = static_cast<int>(std::lround(patternCount_));
    QString done;
    problemElements_.clear();
    try {
        if (id == "dissolve_pattern" || id == "edit_pattern") {
            const auto pattern = editor_->selectedPattern();
            if (!pattern)
                return;
            if (id == "dissolve_pattern") {
                if (!editor_->dissolvePattern(pattern->id))
                    return;
                done = QObject::tr("Muster aufgelöst; die Kopien sind unabhängige Elemente");
            } else {
                if (!editor_->editPattern(pattern->id, count, patternSpacing_, patternAngle_))
                    return;
                done = QObject::tr("Muster geändert");
            }
        } else {
            const auto kind = id == "mirror"           ? document::SketchPatternKind::mirror
                              : id == "linear_pattern" ? document::SketchPatternKind::linear
                                                       : document::SketchPatternKind::circular;
            done = editor_->createPattern(kind, count, patternSpacing_, patternAngle_)
                       ? QObject::tr("Muster angelegt; die Kopien folgen dem Original")
                       : QObject::tr("Alt-Rechteck einmalig kopiert (kein Muster)");
        }
    } catch (const application::SketchEditRefusal& refusal) {
        for (const auto element : refusal.elements())
            problemElements_.push_back(element.value());
        refreshPresentation();
        statusChanged_(QString::fromStdString(refusal.what()));
        return;
    }
    resetTranslationInputs();
    syncSelectedDimensionInputs();
    syncConstraintState();
    refreshPresentation();
    statusChanged_(done);
}

void SketchEditorToolController::applyEditOperation(const std::string& id) {
    if (!editor_)
        return;
    if (id == "trim" || id == "extend" || id == "split") {
        const auto edit = id == "trim"     ? application::SketchCurveEdit::trim
                          : id == "extend" ? application::SketchCurveEdit::extend
                                           : application::SketchCurveEdit::split;
        applyCurveEdit([this, edit] { return editor_->editCurve(edit); });
        return;
    }
    if (id == "fillet") {
        applyCurveEdit([this] { return editor_->filletCorner(filletRadius_); });
        return;
    }
    if (id == "mirror" || id == "linear_pattern" || id == "circular_pattern" ||
        id == "edit_pattern" || id == "dissolve_pattern") {
        applyPatternOperation(id);
        return;
    }
    if (id == "offset") {
        if (const auto problem = editor_->offsetProblem(offsetDistance_)) {
            statusChanged_(QString::fromStdString(*problem));
            return;
        }
    }
    const bool changed = id == "offset" && editor_->offsetSelected(offsetDistance_);
    if (!changed)
        return;
    resetTranslationInputs();
    syncSelectedDimensionInputs();
    syncConstraintState();
    refreshPresentation();
    statusChanged_(QObject::tr("Skizzenoperation abgeschlossen"));
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
    panel_.setLengthVisible("fillet_radius",
                            selecting && editor_->selectedReferences().size() >= 2);
    panel_.setLengthVisible("offset_distance", selecting && hasSelection);
    panel_.setLengthVisible("pattern_count", selecting && hasSelection);
    panel_.setLengthVisible("pattern_spacing", selecting && hasSelection);
    panel_.setLengthVisible("pattern_angle", selecting && hasSelection);
    // S12: a selected source or copy shows its pattern's values for "Muster ändern".
    if (const auto pattern = editor_->selectedPattern();
        pattern && pattern->kind != document::SketchPatternKind::mirror) {
        patternCount_ = pattern->count;
        panel_.setLengthValue("pattern_count", patternCount_);
        if (pattern->kind == document::SketchPatternKind::linear) {
            patternSpacing_ = std::hypot(pattern->step.x, pattern->step.y);
            panel_.setLengthValue("pattern_spacing", patternSpacing_);
        } else {
            patternAngle_ =
                kernel::toDegrees(kernel::Radians(pattern->angle)).value() * pattern->count;
            panel_.setLengthValue("pattern_angle", patternAngle_);
        }
    }
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
    // A new selection or operation ends a running translation as its own step (U09).
    if (editor_ && editor_->isDragging())
        editor_->endDrag();
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
            makeSceneSketchElement(element,
                                   editor_->isElementConflicting(element.id) ||
                                       std::ranges::find(problemElements_, element.id.value()) !=
                                           problemElements_.end(),
                                   editor_->isElementDetermined(element.id));
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
    // S07: label, value and editability come from one projection (SketchConstraintAnnotations).
    for (const features::SketchConstraintAnnotation& annotation : annotations)
        constraints.push_back({annotation.id.value(), annotation.label, annotation.position.X(),
                               annotation.position.Y(), annotation.position.Z(),
                               editor_->isConstraintConflicting(annotation.id),
                               annotation.editable, editor_->isConstraintRedundant(annotation.id)});
    if (previewAutomatic_.has_value() && evaluated.size() > editor_->elements().size() &&
        !evaluated.back().points.empty()) {
        const auto& first = evaluated.back().points[0].world;
        const gp_Pnt second = evaluated.back().points.size() >= 2
                                  ? evaluated.back().points[1].world
                                  : evaluated.back().radialAnchor.value_or(first);
        const auto previewLabel =
            *previewAutomatic_ == viewport::SketchAutomaticConstraint::horizontal      ? "[H?]"
            : *previewAutomatic_ == viewport::SketchAutomaticConstraint::vertical      ? "[V?]"
            : *previewAutomatic_ == viewport::SketchAutomaticConstraint::parallel      ? "[P?]"
            : *previewAutomatic_ == viewport::SketchAutomaticConstraint::perpendicular ? "[⊥?]"
            : *previewAutomatic_ == viewport::SketchAutomaticConstraint::tangent       ? "[T?]"
            : *previewAutomatic_ == viewport::SketchAutomaticConstraint::equal         ? "[=?]"
            : *previewAutomatic_ == viewport::SketchAutomaticConstraint::concentric    ? "[CC?]"
                                                                                       : "[O?]";
        constraints.push_back({0, previewLabel, (first.X() + second.X()) * 0.5,
                               (first.Y() + second.Y()) * 0.5, (first.Z() + second.Z()) * 0.5,
                               false, false});
    }
    viewport_.setSketchConstraints(constraints);
}

} // namespace voelund::desktop
