// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchEditorToolController.hpp"
#include "CommandFeedback.hpp"
#include <voelund/document/SketchPlaneValidation.hpp>

#include "SketchViewAdapter.hpp"
#include "ToolPanelHost.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/features/SketchSupportResolver.hpp>

#include <QInputDialog>
#include <QObject>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace voelund::desktop {
namespace {

application::SketchDrawingTool applicationTool(int index) {
    if (index == 1)
        return application::SketchDrawingTool::line;
    if (index == 2)
        return application::SketchDrawingTool::polyline;
    if (index == 3)
        return application::SketchDrawingTool::rectangle;
    if (index == 4)
        return application::SketchDrawingTool::circle;
    if (index == 5)
        return application::SketchDrawingTool::arc;
    if (index == 6)
        return application::SketchDrawingTool::threePointArc;
    if (index == 7)
        return application::SketchDrawingTool::ellipse;
    if (index == 8)
        return application::SketchDrawingTool::slot;
    if (index == 9)
        return application::SketchDrawingTool::polygon;
    if (index == centerRectangleTool)
        return application::SketchDrawingTool::rectangle;
    if (index == pointTool)
        return application::SketchDrawingTool::point;
    if (index == bsplineTool)
        return application::SketchDrawingTool::line; // S21: the controller collects points
    return application::SketchDrawingTool::select;
}

viewport::SketchElementTool viewportTool(int index) {
    if (index == 1)
        return viewport::SketchElementTool::line;
    if (index == 2)
        return viewport::SketchElementTool::polyline;
    if (index == 3)
        return viewport::SketchElementTool::rectangle;
    if (index == 4)
        return viewport::SketchElementTool::circle;
    if (index == 5)
        return viewport::SketchElementTool::arc;
    if (index == 6)
        return viewport::SketchElementTool::threePointArc;
    if (index == 7)
        return viewport::SketchElementTool::ellipse;
    if (index == 8)
        return viewport::SketchElementTool::slot;
    if (index == 9)
        return viewport::SketchElementTool::polygon;
    if (index == centerRectangleTool)
        return viewport::SketchElementTool::centerRectangle;
    if (index == pointTool)
        return viewport::SketchElementTool::point;
    if (index == bsplineTool)
        return viewport::SketchElementTool::polyline; // S21: a click per control point
    return viewport::SketchElementTool::select;
}

} // namespace

SketchEditorToolController::SketchEditorToolController(
    application::CadSession& session, viewport::CadViewport& viewport, ToolPanelHost& panel,
    std::function<void()> sceneChanged, std::function<void(const QString&)> statusChanged)
    : session_(session), viewport_(viewport), panel_(panel), sceneChanged_(std::move(sceneChanged)),
      statusChanged_(std::move(statusChanged)) {
    installDynamicInputEvaluator();
}

void SketchEditorToolController::showPanel() {
    toolIndex_ = 1;
    translationX_ = 0.0;
    translationY_ = 0.0;
    viewport_.setSketchAutomaticRelations(automaticRelations_);
    viewport_.setSketchRectangleOptions(rectangleOptions_.rounded, rectangleOptions_.frame);
    panel_.begin(ToolPanelDefinition{
        QObject::tr("Skizzeneditor"),
        QObject::tr("Wähle ein Werkzeug und setze seine Punkte. Füge beliebig viele Elemente hinzu "
                    "und bestätige dann."),
        {{"translation_x", QObject::tr("Gewählt ΔX"), 0.0, -1'000'000.0, 1'000'000.0},
         {"translation_y", QObject::tr("Gewählt ΔY"), 0.0, -1'000'000.0, 1'000'000.0},
         // PAR02: length and radius of the selected element also take "=Name".
         {"line_length", QObject::tr("Linienlänge"), 1.0, 0.001, 1'000'000.0, ToolValueKind::length,
          true},
         {"rectangle_width", QObject::tr("Rechteckbreite"), 1.0, 0.001, 1'000'000.0},
         {"rectangle_height", QObject::tr("Rechteckhöhe"), 1.0, 0.001, 1'000'000.0},
         {"circle_radius", QObject::tr("Kreis-/Bogenradius"), 1.0, 0.001, 1'000'000.0,
          ToolValueKind::length, true},
         {"polygon_sides", QObject::tr("Polygonseiten"), 6.0, 3.0, 64.0, ToolValueKind::count},
         {"fillet_radius", QObject::tr("Rundungsradius"), 2.0, 0.001, 1'000'000.0},
         {"chamfer_distance", QObject::tr("Fasenabstand"), 2.0, 0.001, 1'000'000.0},
         {"transform_center_x", QObject::tr("Bezugspunkt X"), 0.0, -1'000'000.0, 1'000'000.0},
         {"transform_center_y", QObject::tr("Bezugspunkt Y"), 0.0, -1'000'000.0, 1'000'000.0},
         {"rotate_angle", QObject::tr("Drehwinkel"), 90.0, -3600.0, 3600.0, ToolValueKind::angle},
         {"scale_factor", QObject::tr("Skalierfaktor"), 2.0, 0.001, 1000.0, ToolValueKind::factor},
         {"offset_distance", QObject::tr("Versatzabstand"), 1.0, -1'000'000.0, 1'000'000.0},
         {"pattern_count", QObject::tr("Anzahl im Muster"), 3.0, 2.0, 1000.0, ToolValueKind::count},
         {"pattern_spacing", QObject::tr("Linearer Abstand"), 10.0, -1'000'000.0, 1'000'000.0},
         {"pattern_angle", QObject::tr("Kreiswinkel"), 360.0, -3600.0, 3600.0,
          ToolValueKind::angle}},
        {{"tool",
          QObject::tr("Werkzeug"),
          {QObject::tr("Auswählen"), QObject::tr("Linie"), QObject::tr("Polylinie"),
           QObject::tr("Rechteck"), QObject::tr("Kreis"), QObject::tr("Bogen (Mitte)"),
           QObject::tr("Dreipunktbogen"), QObject::tr("Ellipse"), QObject::tr("Langloch"),
           QObject::tr("Polygon"), QObject::tr("Rechteck (Mitte)"), QObject::tr("Punkt"),
           QObject::tr("B-Spline"), QObject::tr("Bemaßung")},
          1,
          {"select", "line", "polyline", "rectangle", "circle", "center_arc", "three_point_arc",
           "ellipse", "slot", "polygon", "center_rectangle", "point", "bspline", "dimension"}},
         // S04: snapped points and suggested relations become constraints, or nothing does.
         {"relations",
          QObject::tr("Beziehungen"),
          {QObject::tr("Automatisch"), QObject::tr("Keine")},
          automaticRelations_ ? 0 : 1},
         // S13: view switches; they change no document state.
         {"show_grid",
          QObject::tr("Raster"),
          {QObject::tr("Ein"), QObject::tr("Aus")},
          gridShown_ && gridShown_() ? 0 : 1},
         {"show_dimensions",
          QObject::tr("Bemaßungen"),
          {QObject::tr("Ein"), QObject::tr("Aus")},
          showDimensions_ ? 0 : 1},
         {"show_projected",
          QObject::tr("Projizierte Geometrie"),
          {QObject::tr("Ein"), QObject::tr("Aus")},
          showProjected_ ? 0 : 1},
         // W13 (as in FreeCAD): rectangle options, shown while a rectangle tool is on.
         {"rounded_corners",
          QObject::tr("Abgerundete Ecken (U)"),
          {QObject::tr("Aus"), QObject::tr("Ein")},
          rectangleOptions_.rounded ? 1 : 0},
         {"frame",
          QObject::tr("Rahmen (J)"),
          {QObject::tr("Aus"), QObject::tr("Ein")},
          rectangleOptions_.frame ? 1 : 0}},
        {{"horizontal", QObject::tr("Linie waagrecht festlegen"), QObject::tr("Linien")},
         {"vertical", QObject::tr("Linie senkrecht festlegen"), QObject::tr("Linien")},
         {"free", QObject::tr("Richtungsbedingung der Linie entfernen"), QObject::tr("Linien")},
         {"parallel", QObject::tr("Gewählte Linien parallel machen"), QObject::tr("Linien")},
         {"perpendicular", QObject::tr("Gewählte Linien rechtwinklig machen"),
          QObject::tr("Linien")},
         {"collinear", QObject::tr("Gewählte Linien kollinear machen"), QObject::tr("Linien")},
         {"coincident", QObject::tr("Gewählte Endpunkte zusammenlegen"), QObject::tr("Punkte")},
         {"horizontal_points", QObject::tr("Gewählte Punkte waagrecht ausrichten"),
          QObject::tr("Punkte")},
         {"vertical_points", QObject::tr("Gewählte Punkte senkrecht ausrichten"),
          QObject::tr("Punkte")},
         {"symmetric", QObject::tr("Erste zwei Punkte an der dritten gewählten Linie spiegeln"),
          QObject::tr("Punkte")},
         {"midpoint", QObject::tr("Ersten gewählten Punkt auf die Mitte der gewählten Linie legen"),
          QObject::tr("Punkte")},
         {"perpendicular_bisector", QObject::tr("Erste Linie halbiert die zweite"),
          QObject::tr("Punkte")},
         {"point_on_object", QObject::tr("Ersten gewählten Punkt auf das zweite Objekt legen"),
          QObject::tr("Punkte")},
         {"fixed", QObject::tr("Gewählte Linie oder Kreis fixieren"), QObject::tr("Beziehungen")},
         {"equal", QObject::tr("Gewählte Längen oder Radien gleich machen"),
          QObject::tr("Beziehungen")},
         {"tangent", QObject::tr("Gewählte Objekte tangential machen"), QObject::tr("Beziehungen")},
         {"concentric", QObject::tr("Gewählte Kreise konzentrisch machen"),
          QObject::tr("Beziehungen")},
         {"diameter", QObject::tr("Durchmesser des gewählten Kreises bemaßen"),
          QObject::tr("Bemaßungen")},
         {"radius", QObject::tr("Radius des gewählten Kreises oder Bogens bemaßen"),
          QObject::tr("Bemaßungen")},
         {"point_distance", QObject::tr("Abstand der gewählten Punkte bemaßen"),
          QObject::tr("Bemaßungen")},
         {"point_line_distance",
          QObject::tr("Abstand des ersten Punkts von der zweiten Linie bemaßen"),
          QObject::tr("Bemaßungen")},
         {"point_distance_x", QObject::tr("Waagrechten Abstand der Punkte bemaßen"),
          QObject::tr("Bemaßungen")},
         {"point_distance_y", QObject::tr("Senkrechten Abstand der Punkte bemaßen"),
          QObject::tr("Bemaßungen")},
         {"angle", QObject::tr("Winkel zwischen den gewählten Linien bemaßen"),
          QObject::tr("Bemaßungen")},
         {"toggle_reference",
          QObject::tr("Gewähltes Maß zwischen treibend und Referenz umschalten"),
          QObject::tr("Bemaßungen")},
         {"construction", QObject::tr("Konstruktionsgeometrie der Auswahl umschalten"),
          QObject::tr("Bearbeiten")},
         {"lock_point", QObject::tr("Gewählten Punkt sperren"), QObject::tr("Bearbeiten")},
         {"unlock_point", QObject::tr("Gewählten Punkt freigeben"), QObject::tr("Bearbeiten")},
         {"remove_constraint", QObject::tr("Gewählte, sonst die letzte Bedingung entfernen"),
          QObject::tr("Bearbeiten")},
         {"undo_sketch", QObject::tr("Skizzenschritt rückgängig"), QObject::tr("Bearbeiten")},
         {"redo_sketch", QObject::tr("Skizzenschritt wiederholen"), QObject::tr("Bearbeiten")},
         {"trim",
          QObject::tr("Angeklicktes Stück einer Linie, eines Bogens oder Kreises bis zu den "
                      "nächsten Schnittpunkten entfernen"),
          QObject::tr("Bearbeiten")},
         {"extend",
          QObject::tr("Linie oder Bogen am angeklickten Ende bis zur nächsten Geometrie "
                      "verlängern"),
          QObject::tr("Bearbeiten")},
         {"split", QObject::tr("Linie oder Bogen an der angeklickten Stelle teilen"),
          QObject::tr("Bearbeiten")},
         {"fillet",
          QObject::tr("Ecke zweier gewählter Linien verrunden (nur Linien); Maße der Ecke "
                      "bleiben an einer virtuellen Ecke"),
          QObject::tr("Bearbeiten")},
         {"chamfer",
          QObject::tr("Ecke zweier gewählter Linien fasen (gleicher Abstand, nur Linien); Maße "
                      "der Ecke bleiben an einer virtuellen Ecke"),
          QObject::tr("Bearbeiten")},
         {"offset",
          QObject::tr("Gewählte Linien, Bögen, Kreise oder Konturen versetzen (keine Ellipsen; "
                      "Kreise und Rechtecke einzeln)"),
          QObject::tr("Bearbeiten")},
         {"project",
          QObject::tr("Körperkanten in die Skizze projizieren (an/aus; Kante anklicken)"),
          QObject::tr("Bearbeiten")},
         {"rotate", QObject::tr("Auswahl um den Bezugspunkt drehen"), QObject::tr("Bearbeiten")},
         {"scale", QObject::tr("Auswahl um den Bezugspunkt skalieren"), QObject::tr("Bearbeiten")},
         {"mirror", QObject::tr("Auswahl an der zuletzt gewählten Linie spiegeln (assoziativ)"),
          QObject::tr("Bearbeiten")},
         {"linear_pattern", QObject::tr("Lineares Muster entlang +X anlegen (assoziativ)"),
          QObject::tr("Bearbeiten")},
         {"circular_pattern", QObject::tr("Kreismuster um den Ursprung anlegen (assoziativ)"),
          QObject::tr("Bearbeiten")},
         {"edit_pattern", QObject::tr("Anzahl, Abstand und Winkel des gewählten Musters ändern"),
          QObject::tr("Bearbeiten")},
         {"dissolve_pattern",
          QObject::tr("Muster auflösen: Die Kopien werden unabhängige Elemente"),
          QObject::tr("Bearbeiten")},
         {"delete", QObject::tr("Gewählte Elemente oder Bedingung löschen (Entf)"),
          QObject::tr("Bearbeiten")}},
        [this](const std::string& id, double value) { applyParameter(id, value); },
        [this](const std::string& id, int index) { chooseOption(id, index); },
        [this](const std::string& id) { runAction(id); },
        [this]() { accept(); },
        [this]() { requestCancel(); },
        [this]() { stepBack(); },
        [this]() { finishStep(); },
        [this](const std::string& id) {
            if (id == "translation_x" || id == "translation_y")
                finishDrag();
        },
        [this](const std::string& id, std::optional<ResolvedParameter> bound) {
            bindElementDimension(id, bound);
        }});
    syncSelectedDimensionInputs();
    syncConstraintState();
}

void SketchEditorToolController::setTool(int index) {
    if (!editor_)
        return;
    editor_->setTool(applicationTool(index));
    toolIndex_ = index;
    dimensionAnchor_.reset();
    bsplinePoles_.clear();
    setProjecting(false);
    problemElements_.clear(); // S13: red marks belong to the last refused edit
    viewport_.setSketchEditorTool(viewportTool(index));
    syncSelectedDimensionInputs();
    refreshPresentation();
    const QString guidance =
        index == 5   ? QObject::tr("Mittelpunktbogen: Mittelpunkt, Radiuspunkt, Endpunkt")
        : index == 6 ? QObject::tr("Dreipunktbogen: Anfang, Ende, Punkt auf dem Bogen")
        : index == 7 ? QObject::tr("Ellipse: Mittelpunkt, großer Radius, kleiner Radius")
        : index == 8 ? QObject::tr("Langloch: Mittellinie Anfang, Mittellinie Ende, Breite")
        : index == 9 ? QObject::tr("Polygon: Mittelpunkt und erste Ecke")
        : index == centerRectangleTool
            ? QObject::tr("Rechteck aus dem Mittelpunkt: Mittelpunkt, dann eine Ecke")
        : index == pointTool ? QObject::tr("Punkt: ein Klick setzt ihn")
        : index == bsplineTool
            ? QObject::tr("B-Spline: Kontrollpunkte anklicken (mindestens 4), Enter beendet")
            : QObject::tr("Skizzenwerkzeug gewechselt");
    statusChanged_(guidance);
}

void SketchEditorToolController::updatePreview(
    const std::optional<viewport::SketchElementIntent>& intent) {
    if (!editor_)
        return;
    if (intent.has_value() && toolIndex_ == centerRectangleTool) {
        // Preview the full rectangle around the picked center (S04a).
        previewAutomatic_ = intent->automaticConstraint;
        static_cast<void>(editor_->setPreview(
            {2 * intent->firstX - intent->secondX, 2 * intent->firstY - intent->secondY},
            {intent->secondX, intent->secondY}));
    } else if (intent.has_value()) {
        previewAutomatic_ = intent->automaticConstraint;
        const auto third =
            intent->third.has_value()
                ? std::optional{document::SketchPoint{intent->third->first, intent->third->second}}
                : std::nullopt;
        static_cast<void>(editor_->setPreview({intent->firstX, intent->firstY},
                                              {intent->secondX, intent->secondY}, third));
    } else {
        previewAutomatic_.reset();
        editor_->clearPreview();
    }
    refreshPresentation();
}

bool SketchEditorToolController::selectElement(const viewport::SketchSelectionIntent& intent) {
    if (editor_ && toolIndex_ == dimensionTool)
        return pickForDimension(intent);
    if (!editor_ ||
        !editor_->selectNearestReference(document::SketchElementId{intent.elementId},
                                         {intent.localX, intent.localY}, intent.additive))
        return false;
    editor_->clearPreview(); // S09: an offset preview belongs to the previous selection
    problemElements_.clear();
    resetTranslationInputs();
    syncSelectedDimensionInputs();
    syncConstraintState();
    refreshPresentation();
    const auto references = editor_->selectedReferences();
    const char* pointName =
        references.empty()                                                ? "element"
        : references.back().point == document::SketchPointPosition::start ? "start point"
        : references.back().point == document::SketchPointPosition::end   ? "end point"
                                                                          : "center point";
    statusChanged_(QObject::tr("Skizze %1 gewählt (%2 Referenz(en))")
                       .arg(QObject::tr(pointName))
                       .arg(references.size()));
    return true;
}

void SketchEditorToolController::clearSelection() {
    dimensionAnchor_.reset();
    if (!editor_)
        return;
    editor_->clearSelection();
    editor_->clearPreview();
    problemElements_.clear();
    resetTranslationInputs();
    syncSelectedDimensionInputs();
    syncConstraintState();
    refreshPresentation();
}

void SketchEditorToolController::applyElementDimension(const std::string& id, double value) {
    if (!editor_)
        return;
    bool changed = false;
    QString label;
    if (id == "line_length") {
        changed = editor_->setSelectedLineLength(value);
        label = QObject::tr("Linienlänge");
    } else if (id == "rectangle_width") {
        changed = editor_->setSelectedRectangleWidth(value);
        label = QObject::tr("Rechteckbreite");
    } else if (id == "rectangle_height") {
        changed = editor_->setSelectedRectangleHeight(value);
        label = QObject::tr("Rechteckhöhe");
    } else if (id == "circle_radius") {
        changed = editor_->setSelectedCircleRadius(value);
        label = QObject::tr("Kreis-/Bogenradius");
    }
    if (!changed) {
        // S13: a refused value says so and the field shows the current value again.
        syncSelectedDimensionInputs();
        if (!label.isEmpty())
            statusChanged_(QObject::tr("%1: %2 mm nicht übernommen (ungültig oder nicht lösbar)")
                               .arg(label)
                               .arg(value, 0, 'f', 3));
        return;
    }
    syncConstraintState();
    refreshPresentation();
    statusChanged_(QObject::tr("%1 auf %2 mm gesetzt").arg(label).arg(value, 0, 'f', 3));
}

void SketchEditorToolController::applyLineConstraint(document::SketchLineConstraint constraint) {
    if (!editor_ || !editor_->setSelectedLineConstraint(constraint))
        return;
    syncSelectedDimensionInputs();
    syncConstraintState();
    refreshPresentation();
    const char* text = constraint == document::SketchLineConstraint::horizontal
                           ? QT_TRANSLATE_NOOP("QObject", "Linie waagrecht festgelegt")
                       : constraint == document::SketchLineConstraint::vertical
                           ? QT_TRANSLATE_NOOP("QObject", "Linie senkrecht festgelegt")
                           : QT_TRANSLATE_NOOP("QObject", "Richtungsbedingung der Linie entfernt");
    statusChanged_(QObject::tr(text));
}

void SketchEditorToolController::applyRelation(document::SketchConstraintKind kind) {
    if (!editor_ || !editor_->addSelectedRelation(kind))
        return;
    syncSelectedDimensionInputs();
    syncConstraintState();
    refreshPresentation();
    const char* text = kind == document::SketchConstraintKind::coincident
                           ? QT_TRANSLATE_NOOP("QObject", "Deckungsgleich-Bedingung hinzugefügt")
                       : kind == document::SketchConstraintKind::parallel
                           ? QT_TRANSLATE_NOOP("QObject", "Parallel-Bedingung hinzugefügt")
                       : kind == document::SketchConstraintKind::perpendicular
                           ? QT_TRANSLATE_NOOP("QObject", "Rechtwinklig-Bedingung hinzugefügt")
                       : kind == document::SketchConstraintKind::collinear
                           ? QT_TRANSLATE_NOOP("QObject", "Kollinear-Bedingung hinzugefügt")
                       : kind == document::SketchConstraintKind::horizontalPoints
                           ? QT_TRANSLATE_NOOP("QObject", "Waagrechte Punktausrichtung hinzugefügt")
                       : kind == document::SketchConstraintKind::verticalPoints
                           ? QT_TRANSLATE_NOOP("QObject", "Senkrechte Punktausrichtung hinzugefügt")
                       : kind == document::SketchConstraintKind::fixed
                           ? QT_TRANSLATE_NOOP("QObject", "Geometrie fixiert")
                       : kind == document::SketchConstraintKind::equal
                           ? QT_TRANSLATE_NOOP("QObject", "Gleich-Bedingung hinzugefügt")
                       : kind == document::SketchConstraintKind::tangent
                           ? QT_TRANSLATE_NOOP("QObject", "Tangential-Bedingung hinzugefügt")
                       : kind == document::SketchConstraintKind::symmetric
                           ? QT_TRANSLATE_NOOP("QObject", "Symmetrie-Bedingung hinzugefügt")
                       : kind == document::SketchConstraintKind::midpoint
                           ? QT_TRANSLATE_NOOP("QObject", "Mittelpunkt-Bedingung hinzugefügt")
                       : kind == document::SketchConstraintKind::perpendicularBisector
                           ? QT_TRANSLATE_NOOP("QObject", "Mittelsenkrechte-Bedingung hinzugefügt")
                       : kind == document::SketchConstraintKind::concentric
                           ? QT_TRANSLATE_NOOP("QObject", "Konzentrisch-Bedingung hinzugefügt")
                       : kind == document::SketchConstraintKind::diameter
                           ? QT_TRANSLATE_NOOP("QObject", "Durchmessermaß hinzugefügt")
                       : kind == document::SketchConstraintKind::radius
                           ? QT_TRANSLATE_NOOP("QObject", "Radiusmaß hinzugefügt")
                       : kind == document::SketchConstraintKind::pointDistance
                           ? QT_TRANSLATE_NOOP("QObject", "Punktabstand hinzugefügt")
                       : kind == document::SketchConstraintKind::pointLineDistance
                           ? QT_TRANSLATE_NOOP("QObject", "Abstand Punkt–Linie hinzugefügt")
                       : kind == document::SketchConstraintKind::pointDistanceX
                           ? QT_TRANSLATE_NOOP("QObject", "Waagrechter Abstand hinzugefügt")
                       : kind == document::SketchConstraintKind::pointDistanceY
                           ? QT_TRANSLATE_NOOP("QObject", "Senkrechter Abstand hinzugefügt")
                       : kind == document::SketchConstraintKind::angle
                           ? QT_TRANSLATE_NOOP("QObject", "Winkelmaß hinzugefügt")
                           : QT_TRANSLATE_NOOP("QObject", "Punkt-auf-Objekt-Bedingung hinzugefügt");
    statusChanged_(QObject::tr(text));
}

void SketchEditorToolController::toggleConstruction() {
    if (!editor_ || !editor_->toggleSelectedConstruction())
        return;
    syncConstraintState();
    refreshPresentation();
    statusChanged_(editor_->isSelectedConstruction() ? QObject::tr("Konstruktionsgeometrie ein")
                                                     : QObject::tr("Konstruktionsgeometrie aus"));
}

void SketchEditorToolController::lockSelectedReference() {
    if (!editor_ || !editor_->lockSelectedReference())
        return;
    syncConstraintState();
    refreshPresentation();
    statusChanged_(QObject::tr("Gewählter Skizzenpunkt gesperrt"));
}

void SketchEditorToolController::unlockSelectedReference() {
    if (!editor_ || !editor_->unlockSelectedReference())
        return;
    syncConstraintState();
    refreshPresentation();
    statusChanged_(QObject::tr("Gewählter Skizzenpunkt freigegeben"));
}

void SketchEditorToolController::removeSelected() {
    if (!editor_ || removeSelectedConstraint())
        return;
    if (const auto lock = editor_->patternLock(true)) {
        statusChanged_(QString::fromStdString(*lock));
        return;
    }
    if (!editor_->removeSelected())
        return;
    resetTranslationInputs();
    syncSelectedDimensionInputs();
    syncConstraintState();
    refreshPresentation();
    statusChanged_(QObject::tr("Skizzenelement gelöscht"));
}

void SketchEditorToolController::removeLastConstraint() {
    if (!editor_ || removeSelectedConstraint() || !editor_->removeLastConstraint())
        return;
    syncSelectedDimensionInputs();
    syncConstraintState();
    refreshPresentation();
    statusChanged_(QObject::tr("Letzte Skizzenbedingung entfernt"));
}

void SketchEditorToolController::cancel() {
    if (editor_)
        editor_->cancel();
    editor_.reset();
    editing_ = false;
    viewport_.cancelSketch();
    const auto selected = session_.selectedSketchElement();
    viewport_.setSketchElements(
        makeSceneSketchElements(session_),
        selected.has_value() ? std::optional<std::uint64_t>{selected->value()} : std::nullopt);
    panel_.end();
    statusChanged_(QObject::tr("Skizzeneditor abgebrochen"));
}

bool SketchEditorToolController::isActive() const noexcept { return editor_ != nullptr; }

} // namespace voelund::desktop
