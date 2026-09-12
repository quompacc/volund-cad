#include "SketchEditorToolController.hpp"

#include "SketchConstraintLabel.hpp"
#include "SketchViewAdapter.hpp"
#include "ToolPanelHost.hpp"

#include <voelund/application/CadSession.hpp>

#include <QInputDialog>
#include <QObject>

#include <algorithm>
#include <utility>
#include <vector>

namespace voelund::desktop {
namespace {

document::SketchPlane sketchPlane(const viewport::PlanarFaceSelection& support) {
    return {support.originX,     support.originY,     support.originZ,
            support.normalX,     support.normalY,     support.normalZ,
            support.xDirectionX, support.xDirectionY, support.xDirectionZ};
}

application::SketchDrawingTool applicationTool(int index) {
    if (index == 1)
        return application::SketchDrawingTool::line;
    if (index == 2)
        return application::SketchDrawingTool::rectangle;
    if (index == 3)
        return application::SketchDrawingTool::circle;
    if (index == 4)
        return application::SketchDrawingTool::arc;
    return application::SketchDrawingTool::select;
}

viewport::SketchElementTool viewportTool(int index) {
    if (index == 1)
        return viewport::SketchElementTool::line;
    if (index == 2)
        return viewport::SketchElementTool::rectangle;
    if (index == 3)
        return viewport::SketchElementTool::circle;
    if (index == 4)
        return viewport::SketchElementTool::arc;
    return viewport::SketchElementTool::select;
}

} // namespace

SketchEditorToolController::SketchEditorToolController(
    application::CadSession& session, viewport::CadViewport& viewport, ToolPanelHost& panel,
    std::function<void()> sceneChanged, std::function<void(const QString&)> statusChanged)
    : session_(session), viewport_(viewport), panel_(panel), sceneChanged_(std::move(sceneChanged)),
      statusChanged_(std::move(statusChanged)) {}

void SketchEditorToolController::begin(const viewport::PlanarFaceSelection& support) {
    editing_ = false;
    editor_ = std::make_unique<application::SketchEditorSession>(
        session_, document::FeatureId{support.bodyId}, sketchPlane(support));
    showPanel();
    panel_.setAcceptEnabled(editor_->canAccept());
    viewport_.beginSketchEditor(support, viewport::SketchElementTool::line);
    refreshPresentation();
    statusChanged_(QObject::tr("Sketch editor: set two points for a line"));
}

bool SketchEditorToolController::beginSelectedSketch() {
    const auto selected = session_.selectedFeature();
    if (!selected.has_value())
        return false;
    const document::FeatureDescriptor& descriptor = session_.selectedFeatureDescriptor();
    if (descriptor.kind != document::FeatureKind::sketch || !descriptor.inputFeature.has_value())
        return false;
    const document::SketchParameters parameters = session_.sketchParameters(*selected);
    editing_ = true;
    editor_ = std::make_unique<application::SketchEditorSession>(
        session_, *selected, *descriptor.inputFeature, parameters);
    const auto& plane = parameters.plane;
    const viewport::PlanarFaceSelection support{descriptor.inputFeature->value(),
                                                plane.originX,
                                                plane.originY,
                                                plane.originZ,
                                                plane.normalX,
                                                plane.normalY,
                                                plane.normalZ,
                                                plane.xDirectionX,
                                                plane.xDirectionY,
                                                plane.xDirectionZ};
    showPanel();
    panel_.setAcceptEnabled(editor_->canAccept());
    viewport_.beginSketchEditor(support, viewport::SketchElementTool::line);
    refreshPresentation();
    statusChanged_(QObject::tr("Editing existing sketch"));
    return true;
}

void SketchEditorToolController::showPanel() {
    translationX_ = 0.0;
    translationY_ = 0.0;
    panel_.begin(ToolPanelDefinition{
        QObject::tr("Sketch Editor"),
        QObject::tr("Choose a tool and set its points. Add any number of elements, then confirm."),
        {{"translation_x", QObject::tr("Selected ΔX"), 0.0, -1'000'000.0, 1'000'000.0},
         {"translation_y", QObject::tr("Selected ΔY"), 0.0, -1'000'000.0, 1'000'000.0},
         {"line_length", QObject::tr("Line length"), 1.0, 0.001, 1'000'000.0},
         {"rectangle_width", QObject::tr("Rectangle width"), 1.0, 0.001, 1'000'000.0},
         {"rectangle_height", QObject::tr("Rectangle height"), 1.0, 0.001, 1'000'000.0},
         {"circle_radius", QObject::tr("Circle / arc radius"), 1.0, 0.001, 1'000'000.0}},
        {{"tool",
          QObject::tr("Tool"),
          {QObject::tr("Select"), QObject::tr("Line"), QObject::tr("Rectangle"),
           QObject::tr("Circle"), QObject::tr("Center arc")},
          1}},
        {{"horizontal", QObject::tr("Constrain line horizontally"), QObject::tr("Lines")},
         {"vertical", QObject::tr("Constrain line vertically"), QObject::tr("Lines")},
         {"free", QObject::tr("Remove line direction constraint"), QObject::tr("Lines")},
         {"parallel", QObject::tr("Make selected lines parallel"), QObject::tr("Lines")},
         {"perpendicular", QObject::tr("Make selected lines perpendicular"), QObject::tr("Lines")},
         {"collinear", QObject::tr("Make selected lines collinear"), QObject::tr("Lines")},
         {"coincident", QObject::tr("Make selected endpoints coincident"), QObject::tr("Points")},
         {"horizontal_points", QObject::tr("Align selected points horizontally"),
          QObject::tr("Points")},
         {"vertical_points", QObject::tr("Align selected points vertically"),
          QObject::tr("Points")},
         {"symmetric", QObject::tr("Mirror first two points about third selected line"),
          QObject::tr("Points")},
         {"midpoint", QObject::tr("Put first selected point at midpoint of selected line"),
          QObject::tr("Points")},
         {"perpendicular_bisector", QObject::tr("Make first line bisect second line"),
          QObject::tr("Points")},
         {"point_on_object", QObject::tr("Put first selected point on second object"),
          QObject::tr("Points")},
         {"fixed", QObject::tr("Fix selected line or circle"), QObject::tr("Relations")},
         {"equal", QObject::tr("Make selected lengths or radii equal"), QObject::tr("Relations")},
         {"tangent", QObject::tr("Make selected objects tangent"), QObject::tr("Relations")},
         {"concentric", QObject::tr("Make selected circles concentric"), QObject::tr("Relations")},
         {"diameter", QObject::tr("Add diameter dimension to selected circle"),
          QObject::tr("Dimensions")},
         {"point_distance", QObject::tr("Dimension distance between selected points"),
          QObject::tr("Dimensions")},
         {"point_line_distance", QObject::tr("Dimension first point from second line"),
          QObject::tr("Dimensions")},
         {"point_distance_x", QObject::tr("Dimension horizontal distance between points"),
          QObject::tr("Dimensions")},
         {"point_distance_y", QObject::tr("Dimension vertical distance between points"),
          QObject::tr("Dimensions")},
         {"angle", QObject::tr("Dimension angle between selected lines"),
          QObject::tr("Dimensions")},
         {"construction", QObject::tr("Toggle selected construction geometry"),
          QObject::tr("Edit")},
         {"lock_point", QObject::tr("Lock selected point"), QObject::tr("Edit")},
         {"unlock_point", QObject::tr("Unlock selected point"), QObject::tr("Edit")},
         {"remove_constraint", QObject::tr("Remove last constraint"), QObject::tr("Edit")},
         {"delete", QObject::tr("Delete selected element"), QObject::tr("Edit")}},
        [this](const std::string& id, double value) {
            if (id == "translation_x" || id == "translation_y")
                applyTranslation(id, value);
            else
                applyElementDimension(id, value);
        },
        [this](const std::string& id, int index) {
            if (id == "tool")
                setTool(index);
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
            } else if (id == "delete")
                removeSelected();
        },
        [this]() { accept(); },
        [this]() { cancel(); }});
    syncSelectedDimensionInputs();
    syncConstraintState();
}

void SketchEditorToolController::setTool(int index) {
    if (!editor_)
        return;
    editor_->setTool(applicationTool(index));
    viewport_.setSketchEditorTool(viewportTool(index));
    refreshPresentation();
    statusChanged_(index == 4 ? QObject::tr("Center arc: set center, radius point and end point")
                              : QObject::tr("Sketch tool changed"));
}

void SketchEditorToolController::updatePreview(
    const std::optional<viewport::SketchElementIntent>& intent) {
    if (!editor_)
        return;
    if (intent.has_value()) {
        const auto third =
            intent->third.has_value()
                ? std::optional{document::SketchPoint{intent->third->first, intent->third->second}}
                : std::nullopt;
        static_cast<void>(editor_->setPreview({intent->firstX, intent->firstY},
                                              {intent->secondX, intent->secondY}, third));
    } else {
        editor_->clearPreview();
    }
    refreshPresentation();
}

void SketchEditorToolController::translateFromViewport(double deltaX, double deltaY) {
    if (!editor_ || !editor_->translateSelected(deltaX, deltaY))
        return;
    translationX_ += deltaX;
    translationY_ += deltaY;
    panel_.setLengthValue("translation_x", translationX_);
    panel_.setLengthValue("translation_y", translationY_);
    syncConstraintState();
    refreshPresentation();
    statusChanged_(QObject::tr("Sketch element moved: ΔX %1 mm, ΔY %2 mm")
                       .arg(translationX_, 0, 'f', 3)
                       .arg(translationY_, 0, 'f', 3));
}

bool SketchEditorToolController::selectElement(const viewport::SketchSelectionIntent& intent) {
    if (!editor_ ||
        !editor_->selectNearestReference(document::SketchElementId{intent.elementId},
                                         {intent.localX, intent.localY}, intent.additive))
        return false;
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
    statusChanged_(QObject::tr("Sketch %1 selected (%2 reference(s))")
                       .arg(QObject::tr(pointName))
                       .arg(references.size()));
    return true;
}

void SketchEditorToolController::editConstraint(std::uint64_t constraintId) {
    if (!editor_)
        return;
    const document::SketchConstraintId id{constraintId};
    const auto current = editor_->constraintValue(id);
    if (!current.has_value())
        return;
    const auto annotations = editor_->evaluatedConstraints();
    const auto annotation =
        std::ranges::find(annotations, id, &features::SketchConstraintAnnotation::id);
    const bool angle = annotation != annotations.end() &&
                       annotation->kind == document::SketchConstraintKind::angle;
    const bool directional = annotation != annotations.end() &&
                             (annotation->kind == document::SketchConstraintKind::pointDistanceX ||
                              annotation->kind == document::SketchConstraintKind::pointDistanceY);
    bool accepted = false;
    const double value = QInputDialog::getDouble(
        &panel_, QObject::tr("Edit sketch dimension"),
        angle ? QObject::tr("Angle (degrees)") : QObject::tr("Value (mm)"), *current,
        angle         ? -179.999
        : directional ? -1'000'000.0
                      : 0.001,
        angle ? 179.999 : 1'000'000.0, 3, &accepted);
    if (!accepted || !editor_->setConstraintValue(id, value))
        return;
    syncSelectedDimensionInputs();
    syncConstraintState();
    refreshPresentation();
    statusChanged_(angle ? QObject::tr("Sketch angle set to %1°").arg(value, 0, 'f', 3)
                         : QObject::tr("Sketch dimension set to %1 mm").arg(value, 0, 'f', 3));
}

void SketchEditorToolController::applyElementDimension(const std::string& id, double value) {
    if (!editor_)
        return;
    bool changed = false;
    QString label;
    if (id == "line_length") {
        changed = editor_->setSelectedLineLength(value);
        label = QObject::tr("Line length");
    } else if (id == "rectangle_width") {
        changed = editor_->setSelectedRectangleWidth(value);
        label = QObject::tr("Rectangle width");
    } else if (id == "rectangle_height") {
        changed = editor_->setSelectedRectangleHeight(value);
        label = QObject::tr("Rectangle height");
    } else if (id == "circle_radius") {
        changed = editor_->setSelectedCircleRadius(value);
        label = QObject::tr("Circle / arc radius");
    }
    if (!changed)
        return;
    syncConstraintState();
    refreshPresentation();
    statusChanged_(QObject::tr("%1 set to %2 mm").arg(label).arg(value, 0, 'f', 3));
}

void SketchEditorToolController::applyLineConstraint(document::SketchLineConstraint constraint) {
    if (!editor_ || !editor_->setSelectedLineConstraint(constraint))
        return;
    syncSelectedDimensionInputs();
    syncConstraintState();
    refreshPresentation();
    const char* text = constraint == document::SketchLineConstraint::horizontal
                           ? "Line constrained horizontally"
                       : constraint == document::SketchLineConstraint::vertical
                           ? "Line constrained vertically"
                           : "Line direction constraint removed";
    statusChanged_(QObject::tr(text));
}

void SketchEditorToolController::applyRelation(document::SketchConstraintKind kind) {
    if (!editor_ || !editor_->addSelectedRelation(kind))
        return;
    syncSelectedDimensionInputs();
    syncConstraintState();
    refreshPresentation();
    const char* text =
        kind == document::SketchConstraintKind::coincident      ? "Coincident constraint added"
        : kind == document::SketchConstraintKind::parallel      ? "Parallel constraint added"
        : kind == document::SketchConstraintKind::perpendicular ? "Perpendicular constraint added"
        : kind == document::SketchConstraintKind::collinear     ? "Collinear constraint added"
        : kind == document::SketchConstraintKind::horizontalPoints
            ? "Horizontal point alignment added"
        : kind == document::SketchConstraintKind::verticalPoints ? "Vertical point alignment added"
        : kind == document::SketchConstraintKind::fixed          ? "Geometry fixed"
        : kind == document::SketchConstraintKind::equal          ? "Equal constraint added"
        : kind == document::SketchConstraintKind::tangent        ? "Tangent constraint added"
        : kind == document::SketchConstraintKind::symmetric      ? "Symmetric constraint added"
        : kind == document::SketchConstraintKind::midpoint       ? "Midpoint constraint added"
        : kind == document::SketchConstraintKind::perpendicularBisector
            ? "Perpendicular bisector constraint added"
        : kind == document::SketchConstraintKind::concentric        ? "Concentric constraint added"
        : kind == document::SketchConstraintKind::diameter          ? "Diameter dimension added"
        : kind == document::SketchConstraintKind::pointDistance     ? "Point distance added"
        : kind == document::SketchConstraintKind::pointLineDistance ? "Point-to-line distance added"
        : kind == document::SketchConstraintKind::pointDistanceX    ? "Horizontal distance added"
        : kind == document::SketchConstraintKind::pointDistanceY    ? "Vertical distance added"
        : kind == document::SketchConstraintKind::angle             ? "Angle dimension added"
                                                        : "Point-on-object constraint added";
    statusChanged_(QObject::tr(text));
}

void SketchEditorToolController::toggleConstruction() {
    if (!editor_ || !editor_->toggleSelectedConstruction())
        return;
    syncConstraintState();
    refreshPresentation();
    statusChanged_(editor_->isSelectedConstruction()
                       ? QObject::tr("Construction geometry enabled")
                       : QObject::tr("Construction geometry disabled"));
}

void SketchEditorToolController::lockSelectedReference() {
    if (!editor_ || !editor_->lockSelectedReference())
        return;
    syncConstraintState();
    refreshPresentation();
    statusChanged_(QObject::tr("Selected sketch point locked"));
}

void SketchEditorToolController::unlockSelectedReference() {
    if (!editor_ || !editor_->unlockSelectedReference())
        return;
    syncConstraintState();
    refreshPresentation();
    statusChanged_(QObject::tr("Selected sketch point unlocked"));
}

void SketchEditorToolController::applyTranslation(const std::string& id, double value) {
    if (!editor_ || !editor_->selectedElement().has_value())
        return;
    double deltaX = 0.0;
    double deltaY = 0.0;
    if (id == "translation_x") {
        deltaX = value - translationX_;
        translationX_ = value;
    } else if (id == "translation_y") {
        deltaY = value - translationY_;
        translationY_ = value;
    } else {
        return;
    }
    if (editor_->translateSelected(deltaX, deltaY)) {
        syncConstraintState();
        refreshPresentation();
    }
}

void SketchEditorToolController::removeSelected() {
    if (!editor_ || !editor_->removeSelected())
        return;
    resetTranslationInputs();
    syncSelectedDimensionInputs();
    syncConstraintState();
    refreshPresentation();
    statusChanged_(QObject::tr("Sketch element deleted"));
}

void SketchEditorToolController::removeLastConstraint() {
    if (!editor_ || !editor_->removeLastConstraint())
        return;
    syncSelectedDimensionInputs();
    syncConstraintState();
    refreshPresentation();
    statusChanged_(QObject::tr("Last sketch constraint removed"));
}

void SketchEditorToolController::syncSelectedDimensionInputs() {
    if (!editor_)
        return;
    const auto length = editor_->selectedLineLength();
    const auto width = editor_->selectedRectangleWidth();
    const auto height = editor_->selectedRectangleHeight();
    const auto radius = editor_->selectedCircleRadius();
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
    translationX_ = 0.0;
    translationY_ = 0.0;
    panel_.setLengthValue("translation_x", 0.0);
    panel_.setLengthValue("translation_y", 0.0);
}

void SketchEditorToolController::refreshPresentation() {
    if (!editor_)
        return;
    const auto evaluated = editor_->evaluatedElements();
    std::vector<viewport::SceneSketchElement> elements;
    elements.reserve(evaluated.size());
    for (const features::EvaluatedSketchElement& element : evaluated) {
        auto presentation =
            makeSceneSketchElement(element, editor_->isElementConflicting(element.id));
        if (std::ranges::find(editor_->elements(), element.id, &document::SketchElement::id) ==
            editor_->elements().end())
            presentation.points.clear();
        elements.push_back(std::move(presentation));
    }
    viewport_.setSketchElements(elements);
    std::vector<std::uint64_t> selectedIds;
    for (const auto& reference : editor_->selectedReferences()) {
        if (std::ranges::find(selectedIds, reference.element.value()) == selectedIds.end())
            selectedIds.push_back(reference.element.value());
    }
    if (selectedIds.empty() && editor_->selectedElement().has_value())
        selectedIds.push_back(editor_->selectedElement()->value());
    viewport_.setSelectedSketchElements(selectedIds);
    const auto annotations = editor_->evaluatedConstraints();
    std::vector<viewport::SceneSketchConstraint> constraints;
    constraints.reserve(annotations.size());
    for (const features::SketchConstraintAnnotation& annotation : annotations) {
        const bool editable =
            annotation.kind == document::SketchConstraintKind::distance ||
            annotation.kind == document::SketchConstraintKind::radius ||
            annotation.kind == document::SketchConstraintKind::diameter ||
            annotation.kind == document::SketchConstraintKind::pointDistance ||
            annotation.kind == document::SketchConstraintKind::pointLineDistance ||
            annotation.kind == document::SketchConstraintKind::pointDistanceX ||
            annotation.kind == document::SketchConstraintKind::pointDistanceY ||
            annotation.kind == document::SketchConstraintKind::angle;
        constraints.push_back({annotation.id.value(), sketchConstraintLabel(annotation),
                               annotation.position.X(), annotation.position.Y(),
                               annotation.position.Z(),
                               editor_->isConstraintConflicting(annotation.id), editable});
    }
    viewport_.setSketchConstraints(constraints);
}

void SketchEditorToolController::accept() {
    if (!editor_ || !editor_->canAccept())
        return;
    const bool wasEditing = editing_;
    static_cast<void>(editor_->accept());
    editor_.reset();
    editing_ = false;
    viewport_.cancelSketch();
    panel_.end();
    sceneChanged_();
    statusChanged_(wasEditing ? QObject::tr("Sketch updated") : QObject::tr("Sketch created"));
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
    statusChanged_(QObject::tr("Sketch editor cancelled"));
}

bool SketchEditorToolController::isActive() const noexcept { return editor_ != nullptr; }

} // namespace voelund::desktop
