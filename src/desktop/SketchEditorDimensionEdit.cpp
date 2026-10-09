#include "SketchEditorToolController.hpp"

#include "ToolPanelHost.hpp"

#include <QInputDialog>
#include <QObject>

#include <algorithm>
#include <stdexcept>

// S07: editing a dimension from its label. Relations are removed by a click; a dimension opens
// a value dialog that keeps a refused value with its reason for correction.
namespace voelund::desktop {

void SketchEditorToolController::editConstraint(std::uint64_t constraintId) {
    if (!editor_)
        return;
    const document::SketchConstraintId id{constraintId};
    const auto currentAngle = editor_->constraintAngle(id);
    const auto current = currentAngle
                             ? std::optional<double>{kernel::toDegrees(*currentAngle).value()}
                             : editor_->constraintValue(id);
    if (!current.has_value()) {
        // S13: a relation is selected, not removed at once (Delete removes it).
        selectConstraint(constraintId);
        return;
    }
    // S07: a bound or reference dimension says why it cannot be edited here.
    if (const auto problem = editor_->dimensionProblem(id)) {
        statusChanged_(QString::fromStdString(*problem));
        return;
    }
    const auto annotations = editor_->evaluatedConstraints();
    const auto annotation =
        std::ranges::find(annotations, id, &features::SketchConstraintAnnotation::id);
    const bool angle = currentAngle.has_value();
    const bool signedValue =
        annotation != annotations.end() &&
        (annotation->kind == document::SketchConstraintKind::pointDistanceX ||
         annotation->kind == document::SketchConstraintKind::pointDistanceY ||
         annotation->kind == document::SketchConstraintKind::coordinateX ||
         annotation->kind == document::SketchConstraintKind::coordinateY);
    // S07: a refused value comes back with its reason, ready to be corrected.
    double value = *current;
    QString reason;
    for (;;) {
        bool accepted = false;
        const QString prompt = angle ? QObject::tr("Winkel (Grad)") : QObject::tr("Wert (mm)");
        value = QInputDialog::getDouble(
            &panel_, QObject::tr("Skizzenmaß bearbeiten"),
            reason.isEmpty() ? prompt : reason + QStringLiteral("\n\n") + prompt, value,
            angle        ? -179.999
            : signedValue ? -1'000'000.0
                          : 0.001,
            angle ? 179.999 : 1'000'000.0, 3, &accepted);
        if (!accepted)
            return;
        try {
            if (!editor_->editDimension(
                    id, angle ? kernel::toRadians(kernel::Degrees(value)).value() : value))
                return;
            break;
        } catch (const std::invalid_argument& error) {
            reason = QString::fromStdString(error.what());
            statusChanged_(reason);
        }
    }
    syncSelectedDimensionInputs();
    syncConstraintState();
    refreshPresentation();
    statusChanged_(angle ? QObject::tr("Skizzenwinkel auf %1° gesetzt").arg(value, 0, 'f', 3)
                         : QObject::tr("Skizzenmaß auf %1 mm gesetzt").arg(value, 0, 'f', 3));
}

void SketchEditorToolController::selectElements(const std::vector<std::uint64_t>& ids,
                                                bool additive) {
    if (!editor_)
        return;
    std::vector<document::SketchElementId> elements;
    for (const auto id : ids)
        elements.emplace_back(id);
    if (!editor_->selectElements(elements, additive))
        return;
    editor_->clearPreview();
    problemElements_.clear();
    resetTranslationInputs();
    syncSelectedDimensionInputs();
    syncConstraintState();
    refreshPresentation();
    statusChanged_(QObject::tr("%n Skizzenelement(e) gewählt", nullptr,
                               static_cast<int>(editor_->selectedReferences().empty()
                                                    ? 1
                                                    : editor_->selectedReferences().size())));
}

void SketchEditorToolController::selectConstraint(std::uint64_t id) {
    if (!editor_ || !editor_->selectConstraint(document::SketchConstraintId{id}))
        return;
    problemElements_.clear();
    resetTranslationInputs();
    syncSelectedDimensionInputs();
    syncConstraintState();
    refreshPresentation();
    statusChanged_(QObject::tr("Bedingung gewählt; Entf entfernt sie"));
}

bool SketchEditorToolController::removeSelectedConstraint() {
    const auto selected = editor_->selectedConstraint();
    if (!selected || !editor_->removeConstraint(*selected))
        return false;
    editor_->clearSelection();
    syncSelectedDimensionInputs();
    syncConstraintState();
    refreshPresentation();
    statusChanged_(QObject::tr("Skizzenbedingung entfernt"));
    return true;
}

} // namespace voelund::desktop
