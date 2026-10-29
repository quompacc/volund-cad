// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchEditorToolController.hpp"
#include "SketchText.hpp"
#include "ToolPanelHost.hpp"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLineEdit>

#include <algorithm>
#include <stdexcept>

// N15c: "Text" asks for the text, its height and font, and places its outline at the selected
// point (else at the sketch origin) as one step.
namespace voelund::desktop {
namespace {

struct TextInput final {
    QString text;
    double height;
    QString family;
};

std::optional<TextInput> askForText(QWidget& parent, const TextInput& previous) {
    QDialog dialog(&parent);
    dialog.setWindowTitle(QObject::tr("Text"));
    auto* layout = new QFormLayout(&dialog);
    auto* text = new QLineEdit(previous.text, &dialog);
    auto* height = new QDoubleSpinBox(&dialog);
    height->setRange(0.1, 1000.0);
    height->setDecimals(2);
    height->setSuffix(QObject::tr(" mm"));
    height->setValue(previous.height);
    auto* family = new QComboBox(&dialog);
    family->addItems(sketchTextFamilies());
    family->setCurrentText(previous.family);
    layout->addRow(QObject::tr("Text"), text);
    layout->addRow(QObject::tr("Höhe der Großbuchstaben"), height);
    layout->addRow(QObject::tr("Schrift"), family);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addRow(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted)
        return std::nullopt;
    return TextInput{text->text(), height->value(), family->currentText()};
}

} // namespace

void SketchEditorToolController::placeText() {
    static TextInput last{QStringLiteral("Text"), 10.0, sketchTextFamilies().front()};
    const auto input = askForText(viewport_, last);
    if (!input)
        return;
    last = *input;
    // The selected point, else the origin.
    document::SketchPoint origin{0.0, 0.0};
    if (const auto& picks = editor_->selectedReferences(); !picks.empty()) {
        const auto& elements = editor_->elements();
        const auto element =
            std::ranges::find(elements, picks.front().element, &document::SketchElement::id);
        if (element != elements.end())
            origin = document::sketchReferencePoint(element->geometry, picks.front().point)
                         .value_or(origin);
    }
    try {
        const auto count = editor_->addTextOutline(
            sketchTextCurves(input->text, input->family, input->height, origin));
        statusChanged_(QObject::tr("Text mit %n Kurve(n) angelegt", "", static_cast<int>(count)));
    } catch (const std::invalid_argument& error) {
        statusChanged_(QString::fromUtf8(error.what()));
        return;
    }
    panel_.setAcceptEnabled(editor_->canAccept());
    syncConstraintState();
    refreshPresentation();
}

} // namespace voelund::desktop
