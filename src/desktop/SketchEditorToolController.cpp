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
    return viewport::SketchElementTool::select;
}

} // namespace

SketchEditorToolController::SketchEditorToolController(
    application::CadSession& session, viewport::CadViewport& viewport, ToolPanelHost& panel,
    std::function<void()> sceneChanged, std::function<void(const QString&)> statusChanged)
    : session_(session), viewport_(viewport), panel_(panel), sceneChanged_(std::move(sceneChanged)),
      statusChanged_(std::move(statusChanged)) {}

void SketchEditorToolController::showPanel() {
    toolIndex_ = 1;
    translationX_ = 0.0;
    translationY_ = 0.0;
    viewport_.setSketchAutomaticRelations(automaticRelations_);
    panel_.begin(ToolPanelDefinition{
        QObject::tr("Skizzeneditor"),
        QObject::tr("Wähle ein Werkzeug und setze seine Punkte. Füge beliebig viele Elemente hinzu "
                    "und bestätige dann."),
        {{"translation_x", QObject::tr("Gewählt ΔX"), 0.0, -1'000'000.0, 1'000'000.0},
         {"translation_y", QObject::tr("Gewählt ΔY"), 0.0, -1'000'000.0, 1'000'000.0},
         {"line_length", QObject::tr("Linienlänge"), 1.0, 0.001, 1'000'000.0},
         {"rectangle_width", QObject::tr("Rechteckbreite"), 1.0, 0.001, 1'000'000.0},
         {"rectangle_height", QObject::tr("Rechteckhöhe"), 1.0, 0.001, 1'000'000.0},
         {"circle_radius", QObject::tr("Kreis-/Bogenradius"), 1.0, 0.001, 1'000'000.0},
         {"polygon_sides", QObject::tr("Polygonseiten"), 6.0, 3.0, 64.0},
         {"fillet_radius", QObject::tr("Rundungsradius"), 2.0, 0.001, 1'000'000.0},
         {"offset_distance", QObject::tr("Versatzabstand"), 1.0, -1'000'000.0, 1'000'000.0},
         {"pattern_count", QObject::tr("Anzahl im Muster"), 3.0, 2.0, 1000.0},
         {"pattern_spacing", QObject::tr("Linearer Abstand"), 10.0, -1'000'000.0, 1'000'000.0},
         {"pattern_angle", QObject::tr("Kreiswinkel"), 360.0, -3600.0, 3600.0}},
        {{"tool",
          QObject::tr("Werkzeug"),
          {QObject::tr("Auswählen"), QObject::tr("Linie"), QObject::tr("Polylinie"),
           QObject::tr("Rechteck"), QObject::tr("Kreis"), QObject::tr("Bogen (Mitte)"),
           QObject::tr("Dreipunktbogen"), QObject::tr("Ellipse"), QObject::tr("Langloch"),
           QObject::tr("Polygon"), QObject::tr("Rechteck (Mitte)")},
          1,
          {"select", "line", "polyline", "rectangle", "circle", "center_arc", "three_point_arc",
           "ellipse", "slot", "polygon", "center_rectangle"}},
         // S04: snapped points and suggested relations become constraints, or nothing does.
         {"relations",
          QObject::tr("Beziehungen"),
          {QObject::tr("Automatisch"), QObject::tr("Keine")},
          automaticRelations_ ? 0 : 1}},
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
         {"construction", QObject::tr("Konstruktionsgeometrie der Auswahl umschalten"),
          QObject::tr("Bearbeiten")},
         {"lock_point", QObject::tr("Gewählten Punkt sperren"), QObject::tr("Bearbeiten")},
         {"unlock_point", QObject::tr("Gewählten Punkt freigeben"), QObject::tr("Bearbeiten")},
         {"remove_constraint", QObject::tr("Letzte Bedingung entfernen"),
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
          QObject::tr("Ecke zweier gewählter Linien abrunden; Maße der Ecke bleiben an einer "
                      "virtuellen Ecke"),
          QObject::tr("Bearbeiten")},
         {"offset", QObject::tr("Gewählte Geometrie versetzen"), QObject::tr("Bearbeiten")},
         {"mirror", QObject::tr("Auswahl an der zuletzt gewählten Linie spiegeln"),
          QObject::tr("Bearbeiten")},
         {"linear_pattern", QObject::tr("Lineares Muster anlegen"), QObject::tr("Bearbeiten")},
         {"circular_pattern", QObject::tr("Kreismuster um den Ursprung anlegen"),
          QObject::tr("Bearbeiten")},
         {"delete", QObject::tr("Gewähltes Element löschen"), QObject::tr("Bearbeiten")}},
        [this](const std::string& id, double value) {
            if (id == "translation_x" || id == "translation_y")
                applyTranslation(id, value);
            else if (id == "polygon_sides")
                static_cast<void>(editor_->setPolygonSides(static_cast<int>(std::lround(value))));
            else if (id == "fillet_radius")
                filletRadius_ = value;
            else if (id == "offset_distance") {
                offsetDistance_ = value;
                // S09: the preview is the geometry "Versetzen" adds.
                static_cast<void>(editor_->previewOffset(value));
                refreshPresentation();
            }
            else if (id == "pattern_count")
                patternCount_ = value;
            else if (id == "pattern_spacing")
                patternSpacing_ = value;
            else if (id == "pattern_angle")
                patternAngle_ = value;
            else
                applyElementDimension(id, value);
        },
        [this](const std::string& id, int index) {
            if (id == "tool")
                setTool(index);
            else if (id == "relations") {
                automaticRelations_ = index == 0;
                viewport_.setSketchAutomaticRelations(automaticRelations_);
            }
        },
        [this](const std::string& id) {
            if (id == "horizontal") {
                applyLineConstraint(document::SketchLineConstraint::horizontal);
            } else if (id == "vertical") {
                applyLineConstraint(document::SketchLineConstraint::vertical);
            } else if (id == "free") {
                applyLineConstraint(document::SketchLineConstraint::none);
            } else if (id == "coincident") {
                applyRelation(document::SketchConstraintKind::coincident);
            } else if (id == "parallel") {
                applyRelation(document::SketchConstraintKind::parallel);
            } else if (id == "perpendicular") {
                applyRelation(document::SketchConstraintKind::perpendicular);
            } else if (id == "collinear") {
                applyRelation(document::SketchConstraintKind::collinear);
            } else if (id == "horizontal_points") {
                applyRelation(document::SketchConstraintKind::horizontalPoints);
            } else if (id == "vertical_points") {
                applyRelation(document::SketchConstraintKind::verticalPoints);
            } else if (id == "fixed") {
                applyRelation(document::SketchConstraintKind::fixed);
            } else if (id == "equal") {
                applyRelation(document::SketchConstraintKind::equal);
            } else if (id == "tangent") {
                applyRelation(document::SketchConstraintKind::tangent);
            } else if (id == "symmetric") {
                applyRelation(document::SketchConstraintKind::symmetric);
            } else if (id == "midpoint") {
                applyRelation(document::SketchConstraintKind::midpoint);
            } else if (id == "perpendicular_bisector") {
                applyRelation(document::SketchConstraintKind::perpendicularBisector);
            } else if (id == "concentric") {
                applyRelation(document::SketchConstraintKind::concentric);
            } else if (id == "diameter") {
                applyRelation(document::SketchConstraintKind::diameter);
            } else if (id == "point_distance") {
                applyRelation(document::SketchConstraintKind::pointDistance);
            } else if (id == "point_line_distance") {
                applyRelation(document::SketchConstraintKind::pointLineDistance);
            } else if (id == "point_distance_x") {
                applyRelation(document::SketchConstraintKind::pointDistanceX);
            } else if (id == "point_distance_y") {
                applyRelation(document::SketchConstraintKind::pointDistanceY);
            } else if (id == "angle") {
                applyRelation(document::SketchConstraintKind::angle);
            } else if (id == "construction") {
                toggleConstruction();
            } else if (id == "point_on_object") {
                applyRelation(document::SketchConstraintKind::pointOnObject);
            } else if (id == "lock_point") {
                lockSelectedReference();
            } else if (id == "unlock_point") {
                unlockSelectedReference();
            } else if (id == "remove_constraint") {
                removeLastConstraint();
            } else if (id == "undo_sketch") {
                if (editor_->undo()) {
                    // S02: selection and solve status come back with the step.
                    syncSelectedDimensionInputs();
                    syncConstraintState();
                    refreshPresentation();
                }
            } else if (id == "redo_sketch") {
                if (editor_->redo()) {
                    // S02: selection and solve status come back with the step.
                    syncSelectedDimensionInputs();
                    syncConstraintState();
                    refreshPresentation();
                }
            } else if (id == "trim" || id == "extend" || id == "split" || id == "fillet" ||
                       id == "offset" || id == "mirror" || id == "linear_pattern" ||
                       id == "circular_pattern") {
                applyEditOperation(id);
            } else if (id == "delete")
                removeSelected();
        },
        [this]() { accept(); },
        [this]() { cancel(); },
        [this]() { stepBack(); },
        [this](const std::string& id) {
            if (id == "translation_x" || id == "translation_y")
                finishDrag();
        }});
    syncSelectedDimensionInputs();
    syncConstraintState();
}

void SketchEditorToolController::setTool(int index) {
    if (!editor_)
        return;
    editor_->setTool(applicationTool(index));
    toolIndex_ = index;
    viewport_.setSketchEditorTool(viewportTool(index));
    syncSelectedDimensionInputs();
    refreshPresentation();
    const QString guidance =
        index == 5   ? QObject::tr("Mittelpunktbogen: Mittelpunkt, Radiuspunkt, Endpunkt")
        : index == 6 ? QObject::tr("Dreipunktbogen: Anfang, Punkt auf dem Bogen, Ende")
        : index == 7 ? QObject::tr("Ellipse: Mittelpunkt, großer Radius, kleiner Radius")
        : index == 8 ? QObject::tr("Langloch: Mittellinie Anfang, Mittellinie Ende, Breite")
        : index == 9 ? QObject::tr("Polygon: Mittelpunkt und erste Ecke")
        : index == centerRectangleTool
            ? QObject::tr("Rechteck aus dem Mittelpunkt: Mittelpunkt, dann eine Ecke")
            : QObject::tr("Skizzenwerkzeug gewechselt");
    statusChanged_(guidance);
}

void SketchEditorToolController::stepBack() {
    if (!editor_)
        return;
    // U09: Escape during a drag restores the state before it and keeps the editor open.
    if (editor_->isDragging()) {
        static_cast<void>(viewport_.cancelSketchElementDrag());
        editor_->cancelDrag();
        translationX_ = dragBaseX_;
        translationY_ = dragBaseY_;
        panel_.setLengthValue("translation_x", translationX_);
        panel_.setLengthValue("translation_y", translationY_);
        syncConstraintState();
        refreshPresentation();
        statusChanged_(QObject::tr("Verschieben abgebrochen"));
        return;
    }
    if (viewport_.cancelSketchEditorStep()) {
        editor_->clearPreview();
        refreshPresentation();
        statusChanged_(QObject::tr("Aktueller Skizzenschritt abgebrochen"));
        return;
    }
    cancel();
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
    if (!changed)
        return;
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
    if (!editor_ || !editor_->removeSelected())
        return;
    resetTranslationInputs();
    syncSelectedDimensionInputs();
    syncConstraintState();
    refreshPresentation();
    statusChanged_(QObject::tr("Skizzenelement gelöscht"));
}

void SketchEditorToolController::removeLastConstraint() {
    if (!editor_ || !editor_->removeLastConstraint())
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
