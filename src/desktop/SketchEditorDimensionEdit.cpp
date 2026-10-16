#include "SketchEditorToolController.hpp"

#include "ToolPanelHost.hpp"

#include <QCompleter>
#include <QInputDialog>
#include <QLineEdit>
#include <QLocale>
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
    // S07: a reference or edge-bound dimension says why it cannot be edited here.
    if (const auto problem = editor_->dimensionProblem(id)) {
        if (editor_->isReferenceDimension(id)) // S18: selected, to switch it back
            selectConstraint(constraintId);
        statusChanged_(QString::fromStdString(*problem));
        return;
    }
    const bool angle = currentAngle.has_value();
    // S07: a refused value comes back with its reason, ready to be corrected.
    // PAR02: the field also takes "=Name" of a document parameter of the dimension's unit; a
    // bound dimension opens with its name, a number unbinds it.
    const auto bound = editor_->dimensionParameter(id);
    const auto* boundParameter =
        bound ? document::findParameter(session_.parameterTable(), *bound) : nullptr;
    QString text = boundParameter
                       ? QStringLiteral("=") + QString::fromStdString(boundParameter->name)
                       : QString::number(*current, 'g', 12);
    QStringList names;
    for (const auto& parameter : session_.parameterTable().values)
        if (parameter.unit == (angle ? document::ParameterUnit::angle
                                     : document::ParameterUnit::length))
            names.append(QStringLiteral("=") + QString::fromStdString(parameter.name));
    QString reason;
    QString applied;
    for (;;) {
        const QString prompt = (angle ? QObject::tr("Winkel (Grad)") : QObject::tr("Wert (mm)")) +
                               QObject::tr(" oder =Name eines Parameters");
        QInputDialog dialog(&panel_);
        dialog.setObjectName(QStringLiteral("dimensionValueDialog"));
        dialog.setWindowTitle(QObject::tr("Skizzenmaß bearbeiten"));
        dialog.setInputMode(QInputDialog::TextInput);
        dialog.setLabelText(reason.isEmpty() ? prompt : reason + QStringLiteral("\n\n") + prompt);
        dialog.setTextValue(text);
        if (auto* field = dialog.findChild<QLineEdit*>()) {
            auto* completer = new QCompleter(names, &dialog);
            completer->setCaseSensitivity(Qt::CaseInsensitive);
            field->setCompleter(completer);
        }
        if (dialog.exec() != QDialog::Accepted)
            return;
        text = dialog.textValue().trimmed();
        try {
            if (text.startsWith(QLatin1Char('='))) {
                const QString name = text.mid(1).trimmed();
                const auto resolved = panel_.resolveParameter(
                    name, angle ? ToolValueKind::angle : ToolValueKind::length, *current);
                if (!resolved)
                    return; // declined, or the reason is in the status line
                if (resolved->created)
                    editor_->acceptParameterTableChange(); // only the table changed
                if (!editor_->bindDimension(id, resolved->id))
                    return;
                applied = QObject::tr("Skizzenmaß an den Parameter „%1“ gebunden").arg(name);
                break;
            }
            QString number = text;
            for (const QString& unit : {QStringLiteral("mm"), QStringLiteral("°")})
                if (number.endsWith(unit))
                    number = number.chopped(unit.size()).trimmed();
            bool ok = false;
            double value = QLocale::c().toDouble(number, &ok);
            if (!ok)
                value = QLocale().toDouble(number, &ok);
            if (!ok) {
                reason = QObject::tr("„%1“ ist weder eine Zahl noch =Name").arg(text);
                statusChanged_(reason);
                continue;
            }
            if (!editor_->editDimension(
                    id, angle ? kernel::toRadians(kernel::Degrees(value)).value() : value))
                return;
            applied = angle ? QObject::tr("Skizzenwinkel auf %1° gesetzt").arg(value, 0, 'f', 3)
                            : QObject::tr("Skizzenmaß auf %1 mm gesetzt").arg(value, 0, 'f', 3);
            break;
        } catch (const std::invalid_argument& error) {
            reason = QString::fromStdString(error.what());
            statusChanged_(reason);
        }
    }
    syncSelectedDimensionInputs();
    syncConstraintState();
    refreshPresentation();
    statusChanged_(applied);
}

// PAR02: "=Name" in the length or radius field of the selected element binds its dimension
// (created with the parameter's value when it has none); a number there unbinds it.
void SketchEditorToolController::bindElementDimension(const std::string& id,
                                                      std::optional<ResolvedParameter> bound) {
    if (!editor_ || (id != "line_length" && id != "circle_radius"))
        return;
    const bool line = id == "line_length";
    const auto kind =
        line ? document::SketchConstraintKind::distance : document::SketchConstraintKind::radius;
    try {
        if (!bound) {
            if (const auto constraint = editor_->selectedDimension(kind))
                static_cast<void>(editor_->bindDimension(*constraint, std::nullopt));
        } else {
            if (bound->created)
                editor_->acceptParameterTableChange(); // only the table changed
            const auto* parameter = document::findParameter(session_.parameterTable(), bound->id);
            if (parameter == nullptr)
                return;
            static_cast<void>(line ? editor_->setSelectedLineLength(parameter->value)
                                   : editor_->setSelectedCircleRadius(parameter->value));
            const auto constraint = editor_->selectedDimension(kind);
            if (!constraint)
                throw std::invalid_argument("Maß: Das Element lässt sich nicht bemaßen");
            static_cast<void>(editor_->bindDimension(*constraint, bound->id));
            statusChanged_(QObject::tr("%1 an den Parameter „%2“ gebunden")
                               .arg(line ? QObject::tr("Linienlänge") : QObject::tr("Radius"),
                                    QString::fromStdString(parameter->name)));
        }
    } catch (const std::invalid_argument& error) {
        statusChanged_(QString::fromStdString(error.what()));
    }
    syncSelectedDimensionInputs();
    syncConstraintState();
    refreshPresentation();
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

// S18 (user decision 2026-10-14): the selected dimension between driving and reference.
void SketchEditorToolController::toggleReference() {
    const auto selected = editor_ ? editor_->selectedConstraint() : std::nullopt;
    if (!selected)
        return;
    try {
        editor_->toggleReference(*selected);
    } catch (const std::invalid_argument& refusal) {
        statusChanged_(QString::fromStdString(refusal.what()));
        return;
    }
    syncSelectedDimensionInputs();
    syncConstraintState();
    refreshPresentation();
    statusChanged_(editor_->isReferenceDimension(*selected)
                       ? QObject::tr("Referenzmaß: zeigt den Wert, bestimmt nichts")
                       : QObject::tr("Treibendes Maß"));
}

} // namespace voelund::desktop
