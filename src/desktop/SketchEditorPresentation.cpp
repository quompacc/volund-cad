// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchEditorToolController.hpp"

#include "SketchViewAdapter.hpp"
#include "ToolPanelHost.hpp"

#include <QFile>
#include <QFileDialog>
#include <QObject>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>
#include <vector>

namespace voelund::desktop {
namespace {

QString relationName(document::SketchConstraintKind kind) {
    using Kind = document::SketchConstraintKind;
    switch (kind) {
    case Kind::horizontal:
        return QObject::tr("Waagrecht");
    case Kind::vertical:
        return QObject::tr("Senkrecht");
    case Kind::distance:
        return QObject::tr("Länge");
    case Kind::radius:
        return QObject::tr("Radius");
    case Kind::diameter:
        return QObject::tr("Durchmesser");
    case Kind::coincident:
        return QObject::tr("Deckungsgleich");
    case Kind::parallel:
        return QObject::tr("Parallel");
    case Kind::perpendicular:
        return QObject::tr("Rechtwinklig");
    case Kind::coordinateX:
        return QObject::tr("Koordinate X");
    case Kind::coordinateY:
        return QObject::tr("Koordinate Y");
    case Kind::pointOnObject:
        return QObject::tr("Punkt auf Objekt");
    case Kind::equal:
        return QObject::tr("Gleich");
    case Kind::tangent:
        return QObject::tr("Tangential");
    case Kind::symmetric:
        return QObject::tr("Symmetrisch");
    case Kind::pointDistance:
        return QObject::tr("Distanz");
    case Kind::angle:
        return QObject::tr("Winkel");
    case Kind::pointDistanceX:
        return QObject::tr("Abstand X");
    case Kind::pointDistanceY:
        return QObject::tr("Abstand Y");
    case Kind::midpoint:
        return QObject::tr("Mittelpunkt");
    case Kind::perpendicularBisector:
        return QObject::tr("Halbierend");
    case Kind::concentric:
        return QObject::tr("Konzentrisch");
    case Kind::collinear:
        return QObject::tr("Kollinear");
    case Kind::horizontalPoints:
        return QObject::tr("Waagrecht ausgerichtet");
    case Kind::verticalPoints:
        return QObject::tr("Senkrecht ausgerichtet");
    case Kind::fixed:
        return QObject::tr("Fixiert");
    case Kind::pointLineDistance:
        return QObject::tr("Punkt–Linie");
    }
    return {};
}

// W12: a dimension's drawing for the view; a bound one keeps its parameter's name in front
// ("Breite = 80 mm").
std::optional<viewport::SceneDimension>
sceneDimension(const features::SketchConstraintAnnotation& annotation, const std::string& label) {
    if (!annotation.dimension)
        return std::nullopt;
    const auto& shape = *annotation.dimension;
    std::string text = shape.text;
    if (const auto named = label.find(" = "); annotation.parameter && named != std::string::npos)
        text = label.substr(0, named) + " = " + text;
    const auto style = static_cast<viewport::SceneDimensionStyle>(static_cast<int>(shape.style));
    return viewport::SceneDimension{style,           shape.first,     shape.second, shape.vertex,
                                    shape.along,     shape.outward,   shape.normal, shape.offset,
                                    std::move(text), shape.textBeside};
}

// W18 (as SolidCraft): a relation shows symbol boxes on its geometry instead of "[H]" beside a
// point; the symbol is its label without the brackets.
void setGlyphs(viewport::SceneSketchConstraint& scene,
               const features::SketchConstraintAnnotation& annotation) {
    if (annotation.glyphs.empty())
        return;
    for (const auto& anchor : annotation.glyphs)
        scene.glyphs.push_back({anchor.at.X(), anchor.at.Y(), anchor.at.Z(), anchor.along.X(),
                                anchor.along.Y(), anchor.along.Z()});
    for (const auto element : annotation.glyphs.front().elements)
        scene.elements.push_back(element.value());
    std::string key = annotation.label;
    std::erase_if(key, [](char character) {
        return character == '[' || character == ']' || character == '(' || character == ')';
    });
    scene.glyph = key;
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
            done = editor_->createPattern(kind, count, patternSpacing_, patternAngle_,
                                          patternFromLast_)
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
    if (id == "chamfer") { // S19
        applyCurveEdit([this] { return editor_->chamferCorner(chamferDistance_); });
        return;
    }
    if (id == "rotate" || id == "scale") { // S20
        applyCurveEdit([this, &id] {
            return id == "rotate" ? editor_->rotateSelected(
                                        transformCenter_,
                                        kernel::toRadians(kernel::Degrees(rotateAngle_)).value())
                                  : editor_->scaleSelected(transformCenter_, scaleFactor_);
        });
        return;
    }
    // Operations without a selection: S17 projecting on/off, N15a section curves.
    static const std::map<std::string, void (SketchEditorToolController::*)()> plain{
        {"project", &SketchEditorToolController::toggleProjecting},
        {"section", &SketchEditorToolController::addSectionCurves},
        {"import_dxf", &SketchEditorToolController::importDxf},
        {"text", &SketchEditorToolController::placeText}};
    if (const auto operation = plain.find(id); operation != plain.end()) {
        (this->*operation->second)();
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
    syncRectangleOptions(); // W13
    panel_.setLengthVisible("fillet_radius",
                            selecting && editor_->selectedReferences().size() >= 2);
    panel_.setLengthVisible("chamfer_distance",
                            selecting && editor_->selectedReferences().size() >= 2);
    panel_.setLengthVisible("offset_distance", selecting && hasSelection);
    // S20: the center starts at the middle of the selection.
    for (const char* field :
         {"transform_center_x", "transform_center_y", "rotate_angle", "scale_factor"})
        panel_.setLengthVisible(field, selecting && hasSelection);
    if (const auto center = selecting ? editor_->selectionCenter() : std::nullopt) {
        transformCenter_ = *center;
        panel_.setLengthValue("transform_center_x", center->x);
        panel_.setLengthValue("transform_center_y", center->y);
    }
    for (const char* field : {"pattern_count", "pattern_spacing", "pattern_angle"})
        panel_.setLengthVisible(field, selecting && hasSelection);
    panel_.setChoiceVisible("pattern_reference", selecting && hasSelection); // N14a
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
    // PAR02: a bound length or radius shows its parameter.
    const auto boundName = [this](document::SketchConstraintKind kind) -> std::optional<QString> {
        const auto constraint = editor_->selectedDimension(kind);
        const auto parameter = constraint ? editor_->dimensionParameter(*constraint) : std::nullopt;
        const auto* entry =
            parameter ? document::findParameter(session_.parameterTable(), *parameter) : nullptr;
        return entry ? std::optional{QString::fromStdString(entry->name)} : std::nullopt;
    };
    panel_.setLengthBinding(
        "line_length", length ? boundName(document::SketchConstraintKind::distance) : std::nullopt);
    panel_.setLengthBinding(
        "circle_radius", radius ? boundName(document::SketchConstraintKind::radius) : std::nullopt);
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
        auto presentation = makeSceneSketchElement(
            element,
            editor_->isElementConflicting(element.id) ||
                std::ranges::find(problemElements_, element.id.value()) != problemElements_.end(),
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
    for (const features::SketchConstraintAnnotation& annotation : annotations) {
        // S13: "Bemaßungen: Aus" hides the dimensions, relations stay.
        if (hiddenAnnotation(annotation))
            continue;
        // PAR02: a bound dimension shows its parameter, "A = 60.000 mm".
        std::string label = annotation.label;
        if (annotation.parameter)
            if (const auto* parameter =
                    document::findParameter(session_.parameterTable(), *annotation.parameter))
                label = parameter->name + " = " + label;
        constraints.push_back(
            {annotation.id.value(), label, annotation.position.X(), annotation.position.Y(),
             annotation.position.Z(), editor_->isConstraintConflicting(annotation.id),
             annotation.editable, editor_->isConstraintRedundant(annotation.id),
             editor_->selectedConstraint() == annotation.id, sceneDimension(annotation, label)});
        setGlyphs(constraints.back(), annotation); // W18
    }
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

namespace voelund::desktop {
void SketchEditorToolController::setProjecting(bool on) {
    if (!editor_ || projecting_ == on)
        return;
    projecting_ = on;
    viewport_.setEdgePicking(on);
    statusChanged_(on ? QObject::tr("Projizieren: Körperkante anklicken; erneut wählen beendet")
                      : QObject::tr("Projizieren beendet"));
}

// N15b: a DXF file chosen in a dialog comes in as one step; the status line says how many
// curves came and which entity types were skipped.
void SketchEditorToolController::importDxf() {
    const QString path = QFileDialog::getOpenFileName(&viewport_, QObject::tr("DXF importieren"),
                                                      {}, QObject::tr("DXF-Zeichnung (*.dxf)"));
    if (path.isEmpty())
        return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        statusChanged_(QObject::tr("DXF importieren: Die Datei lässt sich nicht lesen"));
        return;
    }
    const QByteArray bytes = file.readAll();
    try {
        const auto result = editor_->importDxf(
            std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size())));
        QStringList skipped;
        for (const auto& [type, count] : result.skipped)
            skipped << QStringLiteral("%1 × %2").arg(count).arg(QString::fromStdString(type));
        statusChanged_(QObject::tr("%n Kurve(n) aus der DXF-Datei übernommen", "",
                                   static_cast<int>(result.added)) +
                       (skipped.isEmpty()
                            ? QString{}
                            : QObject::tr("; übersprungen: %1").arg(skipped.join(", "))));
    } catch (const std::invalid_argument& error) {
        statusChanged_(QString::fromUtf8(error.what()));
        return;
    }
    panel_.setAcceptEnabled(editor_->canAccept());
    syncConstraintState();
    refreshPresentation();
}

// N15a: one step; the status line says how many curves came or why none.
void SketchEditorToolController::addSectionCurves() {
    try {
        const auto count = editor_->addSectionCurves();
        statusChanged_(QObject::tr("%n Schnittkurve(n) hinzugefügt", "", static_cast<int>(count)));
    } catch (const std::invalid_argument& error) {
        statusChanged_(QString::fromUtf8(error.what()));
        return;
    }
    panel_.setAcceptEnabled(editor_->canAccept());
    syncConstraintState();
    refreshPresentation();
}

void SketchEditorToolController::projectEdge(std::uint64_t bodyResult, const TopoDS_Shape& edge) {
    if (!projecting())
        return;
    applyCurveEdit([&] { return editor_->projectEdge(document::FeatureId{bodyResult}, edge); });
}
} // namespace voelund::desktop
