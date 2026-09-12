#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>

#include <filesystem>
#include <iostream>
#include <numbers>
#include <string>
#include <variant>
namespace {
constexpr voelund::document::SketchPlane plane{0.0, 0.0, 40.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0};
bool add(voelund::application::SketchEditorSession& editor, voelund::document::SketchPoint first,
         voelund::document::SketchPoint second, std::uint64_t expectedId) {
    return editor.setPreview(first, second) &&
           editor.confirmPreview() == voelund::document::SketchElementId{expectedId};
}
bool completeEditorFlowIsOneDocumentTransaction() {
    voelund::application::CadSession session;
    session.newDocument();
    const auto support = session.addBoxFeature();
    voelund::application::SketchEditorSession editor(session, support, plane);

    if (!add(editor, {0.0, 0.0}, {5.0, 0.0}, 1))
        return false;
    editor.setTool(voelund::application::SketchDrawingTool::rectangle);
    if (!add(editor, {1.0, 1.0}, {4.0, 3.0}, 2))
        return false;
    editor.setTool(voelund::application::SketchDrawingTool::circle);
    if (!add(editor, {8.0, 3.0}, {10.0, 3.0}, 6))
        return false;
    if (editor.elements().size() != 6 || editor.closedProfileCount() != 2 ||
        !editor.selectElement(voelund::document::SketchElementId{2}))
        return false;

    const auto sketchId = editor.accept();
    if (session.features().size() != 2 || session.selectedFeature() != sketchId ||
        session.selectedSketchParameters().elements.nextId != 7)
        return false;
    if (!session.undo() || session.features().size() != 1)
        return false;
    return session.redo() && session.features().size() == 2;
}
bool cancelLeavesNoFeatureOrUndoEntry() {
    voelund::application::CadSession session;
    session.newDocument();
    const auto support = session.addBoxFeature();
    voelund::application::SketchEditorSession editor(session, support, plane);
    if (!add(editor, {0.0, 0.0}, {2.0, 2.0}, 1))
        return false;
    editor.cancel();
    if (session.features().size() != 1 || editor.isActive())
        return false;
    return session.undo() && session.features().empty();
}
bool toolChangesClearOnlyPreviewAndKeepStableIds() {
    voelund::application::CadSession session;
    session.newDocument();
    const auto support = session.addBoxFeature();
    voelund::application::SketchEditorSession editor(session, support, plane);
    if (!editor.setPreview({0.0, 0.0}, {1.0, 0.0}))
        return false;
    editor.setTool(voelund::application::SketchDrawingTool::circle);
    if (!editor.evaluatedElements().empty())
        return false;
    if (!add(editor, {5.0, 5.0}, {7.0, 5.0}, 1))
        return false;
    editor.setTool(voelund::application::SketchDrawingTool::line);
    return add(editor, {0.0, 0.0}, {2.0, 0.0}, 2) &&
           editor.elements().front().id == voelund::document::SketchElementId{1};
}
bool lineClosureRecomputesProfilesAndOpenSketchRoundtrips() {
    voelund::application::CadSession session;
    session.newDocument();
    const auto support = session.addBoxFeature();
    voelund::application::SketchEditorSession editor(session, support, plane);
    if (!add(editor, {0.0, 0.0}, {4.0, 0.0}, 1) || !add(editor, {4.0, 0.0}, {2.0, 3.0}, 2) ||
        editor.closedProfileCount() != 0 || !add(editor, {2.0, 3.0}, {0.0, 0.0}, 3) ||
        editor.closedProfileCount() != 1)
        return false;
    static_cast<void>(editor.accept());

    const std::filesystem::path path =
        std::filesystem::current_path() / "voelund-sketch-editor-roundtrip.volund";
    session.saveDocument(path);
    voelund::application::CadSession loaded;
    loaded.loadDocument(path);
    std::error_code error;
    std::filesystem::remove(path, error);
    if (error || loaded.features().size() != 2)
        return false;
    const auto sketchId = loaded.features().back().id;
    return loaded.selectFeature(sketchId) &&
           loaded.selectedSketchParameters().elements.values.size() == 3;
}
bool exclusivelyOpenGeometryIsAValidSketch() {
    voelund::application::CadSession session;
    session.newDocument();
    const auto support = session.addBoxFeature();
    voelund::application::SketchEditorSession editor(session, support, plane);
    if (!add(editor, {1.0, 1.0}, {6.0, 2.0}, 1))
        return false;
    const auto sketchId = editor.accept();
    return session.featureStates().back().status ==
               voelund::application::FeatureRuntimeStatus::current &&
           session.selectFeature(sketchId) &&
           std::holds_alternative<voelund::document::SketchLine>(
               session.selectedSketchParameters().elements.values.front().geometry);
}

bool extrusionStillRequiresAClosedProfile() {
    voelund::application::CadSession session;
    session.newDocument();
    const auto support = session.addBoxFeature();
    const voelund::document::SketchParameters openSketch{
        plane,
        {{{voelund::document::SketchElementId{1},
           voelund::document::SketchLine{{0.0, 0.0}, {3.0, 1.0}}}},
         2}};
    static_cast<void>(session.addSketchExtrusion(support, openSketch, 5.0));
    return session.featureStates().back().status ==
               voelund::application::FeatureRuntimeStatus::error &&
           session.featureStates().back().message.find("closed profile") != std::string::npos;
}

bool existingSketchCanBeEditedAsOneTransaction() {
    voelund::application::CadSession session;
    session.newDocument();
    const auto support = session.addBoxFeature();
    voelund::application::SketchEditorSession create(session, support, plane);
    create.setTool(voelund::application::SketchDrawingTool::rectangle);
    if (!add(create, {0.0, 0.0}, {4.0, 3.0}, 1))
        return false;
    create.setTool(voelund::application::SketchDrawingTool::circle);
    if (!add(create, {8.0, 2.0}, {10.0, 2.0}, 5))
        return false;
    create.setTool(voelund::application::SketchDrawingTool::line);
    if (!add(create, {0.0, 6.0}, {3.0, 6.0}, 6))
        return false;
    const auto sketchId = create.accept();
    const auto original = session.sketchParameters(sketchId);

    voelund::application::SketchEditorSession edit(session, sketchId, support,
                                                   session.sketchParameters(sketchId));
    if (!edit.selectElement(voelund::document::SketchElementId{1}) ||
        !edit.translateSelected(1.0, 0.0) ||
        !edit.selectElement(voelund::document::SketchElementId{5}) ||
        !edit.translateSelected(-1.0, 1.0) ||
        !edit.selectElement(voelund::document::SketchElementId{6}) ||
        !edit.translateSelected(1.0, 0.0) || !edit.translateSelected(1.0, 1.0) ||
        !edit.removeSelected())
        return false;
    edit.setTool(voelund::application::SketchDrawingTool::line);
    if (!add(edit, {12.0, 4.0}, {13.0, 4.0}, 7) || edit.closedProfileCount() != 2)
        return false;
    static_cast<void>(edit.accept());

    const auto changed = session.sketchParameters(sketchId);
    if (changed.elements.values.size() != 6 || changed.elements.nextId != 8 || changed == original)
        return false;
    const auto& movedCircle =
        std::get<voelund::document::SketchCircle>(changed.elements.values[4].geometry);
    if (movedCircle.center != voelund::document::SketchPoint{7.0, 3.0})
        return false;
    if (!session.undo() || session.sketchParameters(sketchId) != original || !session.redo() ||
        session.sketchParameters(sketchId) != changed)
        return false;
    const std::filesystem::path path =
        std::filesystem::current_path() / "voelund-edited-sketch-roundtrip.volund";
    session.saveDocument(path);
    voelund::application::CadSession loaded;
    loaded.loadDocument(path);
    std::error_code error;
    std::filesystem::remove(path, error);
    return !error && loaded.selectFeature(sketchId) && loaded.selectedSketchParameters() == changed;
}

bool cancellingExistingSketchEditLeavesHistoryUntouched() {
    voelund::application::CadSession session;
    session.newDocument();
    const auto support = session.addBoxFeature();
    voelund::application::SketchEditorSession create(session, support, plane);
    if (!add(create, {0.0, 0.0}, {4.0, 0.0}, 1))
        return false;
    const auto sketchId = create.accept();
    const auto original = session.sketchParameters(sketchId);
    voelund::application::SketchEditorSession edit(session, sketchId, support, original);
    if (!edit.selectElement(voelund::document::SketchElementId{1}) ||
        !edit.translateSelected(5.0, 5.0))
        return false;
    edit.cancel();
    if (session.sketchParameters(sketchId) != original)
        return false;
    return session.undo() && session.features().size() == 1;
}

bool editingClosedGeometryRecomputesDependentExtrusion() {
    voelund::application::CadSession session;
    session.newDocument();
    const auto support = session.addBoxFeature();
    const voelund::document::SketchParameters sketch{
        plane,
        {{{voelund::document::SketchElementId{1},
           voelund::document::SketchRectangle{1.0, 1.0, 5.0, 4.0}}},
         2}};
    static_cast<void>(session.addSketchExtrusion(support, sketch, 5.0));
    const auto sketchId = session.features()[1].id;
    const auto original = session.sketchParameters(sketchId);
    voelund::application::SketchEditorSession edit(session, sketchId, support, original);
    if (!edit.selectElement(voelund::document::SketchElementId{1}) ||
        !edit.translateSelected(2.0, 1.0) || edit.closedProfileCount() != 1)
        return false;
    static_cast<void>(edit.accept());
    if (session.featureStates().back().status !=
        voelund::application::FeatureRuntimeStatus::current)
        return false;
    return session.undo() && session.sketchParameters(sketchId) == original &&
           session.featureStates().back().status ==
               voelund::application::FeatureRuntimeStatus::current;
}

bool lineDimensionAndConstraintPersistThroughEditing() {
    voelund::application::CadSession session;
    session.newDocument();
    const auto support = session.addBoxFeature();
    voelund::application::SketchEditorSession create(session, support, plane);
    if (!add(create, {0.0, 0.0}, {3.0, 4.0}, 1) ||
        !create.selectElement(voelund::document::SketchElementId{1}) ||
        !create.setSelectedLineLength(10.0) ||
        !create.setSelectedLineConstraint(voelund::document::SketchLineConstraint::horizontal))
        return false;
    const auto sketchId = create.accept();
    const auto constrained = session.sketchParameters(sketchId);
    const auto& initialLine =
        std::get<voelund::document::SketchLine>(constrained.elements.values.front().geometry);
    if (initialLine.end != voelund::document::SketchPoint{10.0, 0.0} ||
        initialLine.constraint != voelund::document::SketchLineConstraint::horizontal ||
        constrained.constraints.values.size() != 2 ||
        constrained.constraints.values[0].kind !=
            voelund::document::SketchConstraintKind::distance ||
        constrained.constraints.values[1].kind !=
            voelund::document::SketchConstraintKind::horizontal) {
        return false;
    }

    voelund::application::SketchEditorSession edit(session, sketchId, support, constrained);
    if (!edit.selectElement(voelund::document::SketchElementId{1}) ||
        !edit.translateSelected(2.0, 3.0))
        return false;
    static_cast<void>(edit.accept());
    const auto moved = session.sketchParameters(sketchId);
    const auto& movedLine =
        std::get<voelund::document::SketchLine>(moved.elements.values.front().geometry);
    if (movedLine.start != voelund::document::SketchPoint{2.0, 3.0} ||
        movedLine.end != voelund::document::SketchPoint{12.0, 3.0} ||
        movedLine.constraint != voelund::document::SketchLineConstraint::horizontal) {
        return false;
    }
    if (!session.undo() || session.sketchParameters(sketchId) != constrained || !session.redo() ||
        session.sketchParameters(sketchId) != moved)
        return false;

    const std::filesystem::path path =
        std::filesystem::current_path() / "voelund-constrained-sketch-roundtrip.volund";
    session.saveDocument(path);
    voelund::application::CadSession loaded;
    loaded.loadDocument(path);
    std::error_code error;
    std::filesystem::remove(path, error);
    return !error && loaded.sketchParameters(sketchId) == moved;
}

bool rectangleDecomposesAndCircleDimensionPersists() {
    voelund::application::CadSession session;
    session.newDocument();
    const auto support = session.addBoxFeature();
    voelund::application::SketchEditorSession editor(session, support, plane);
    editor.setTool(voelund::application::SketchDrawingTool::rectangle);
    if (!add(editor, {5.0, 5.0}, {1.0, 2.0}, 1) ||
        !editor.selectElement(voelund::document::SketchElementId{1}) ||
        editor.selectedLineLength() != std::optional<double>{4.0} ||
        editor.selectedRectangleWidth().has_value() || editor.elements().size() != 4 ||
        editor.constraints().size() != 8 || editor.closedProfileCount() != 1)
        return false;
    editor.setTool(voelund::application::SketchDrawingTool::circle);
    if (!add(editor, {12.0, 8.0}, {14.0, 8.0}, 5) ||
        !editor.selectElement(voelund::document::SketchElementId{5}) ||
        editor.selectedCircleRadius() != std::optional<double>{2.0} ||
        editor.selectedRectangleWidth().has_value() || !editor.setSelectedCircleRadius(3.5))
        return false;

    const auto sketchId = editor.accept();
    const auto changed = session.sketchParameters(sketchId);
    const auto& circle =
        std::get<voelund::document::SketchCircle>(changed.elements.values[4].geometry);
    if (circle.radius != 3.5 || changed.elements.nextId != 6 ||
        changed.constraints.values.size() != 9 ||
        changed.constraints.values.back().kind != voelund::document::SketchConstraintKind::radius ||
        !session.undo() || session.features().size() != 1 || !session.redo() ||
        session.sketchParameters(sketchId) != changed)
        return false;

    const std::filesystem::path path =
        std::filesystem::current_path() / "voelund-sized-sketch-roundtrip.volund";
    session.saveDocument(path);
    voelund::application::CadSession loaded;
    loaded.loadDocument(path);
    std::error_code error;
    std::filesystem::remove(path, error);
    return !error && loaded.sketchParameters(sketchId) == changed;
}

bool unchangedMeasurementCanBecomeDrivingConstraint() {
    voelund::application::CadSession session;
    session.newDocument();
    const auto support = session.addBoxFeature();
    voelund::application::SketchEditorSession editor(session, support, plane);
    if (!add(editor, {0.0, 0.0}, {3.0, 4.0}, 1) ||
        !editor.selectElement(voelund::document::SketchElementId{1}) ||
        !editor.setSelectedLineLength(5.0) || editor.constraints().size() != 1 ||
        editor.setSelectedLineLength(5.0))
        return false;
    const auto constraintId = editor.constraints().front().id;
    if (editor.constraintValue(constraintId) != std::optional<double>{5.0} ||
        editor.evaluatedConstraints().size() != 1 || !editor.setConstraintValue(constraintId, 8.0))
        return false;
    const auto& line = std::get<voelund::document::SketchLine>(editor.elements().front().geometry);
    return editor.lastSolveResult().status == voelund::features::SketchSolveStatus::success &&
           editor.constraintValue(constraintId) == std::optional<double>{8.0} &&
           std::abs(std::hypot(line.end.x - line.start.x, line.end.y - line.start.y) - 8.0) <
               1.0e-8;
}

bool endpointRelationsDriveGeometryAndConflictsStayRecoverable() {
    using namespace voelund;
    application::CadSession session;
    session.newDocument();
    const auto support = session.addBoxFeature();
    application::SketchEditorSession editor(session, support, plane);
    if (!add(editor, {0.0, 0.0}, {4.0, 1.0}, 1) || !add(editor, {6.0, 2.0}, {7.0, 6.0}, 2) ||
        !editor.selectElement(document::SketchElementId{1}) ||
        !editor.setSelectedLineLength(editor.selectedLineLength().value()) ||
        !editor.selectElement(document::SketchElementId{2}) ||
        !editor.setSelectedLineLength(editor.selectedLineLength().value()) ||
        !editor.selectNearestReference(document::SketchElementId{1}, {4.0, 1.0}, false) ||
        !editor.selectNearestReference(document::SketchElementId{2}, {6.0, 2.0}, true) ||
        !editor.canAddSelectedRelation(document::SketchConstraintKind::coincident) ||
        !editor.addSelectedRelation(document::SketchConstraintKind::coincident))
        return false;
    const auto& firstCoincident = std::get<document::SketchLine>(editor.elements()[0].geometry).end;
    const auto& secondCoincident =
        std::get<document::SketchLine>(editor.elements()[1].geometry).start;
    if (std::hypot(firstCoincident.x - secondCoincident.x, firstCoincident.y - secondCoincident.y) >
        1.0e-7)
        return false;

    if (!editor.selectNearestReference(document::SketchElementId{1}, firstCoincident, false) ||
        !editor.selectNearestReference(document::SketchElementId{2}, secondCoincident, true) ||
        !editor.addSelectedRelation(document::SketchConstraintKind::parallel) ||
        !editor.addSelectedRelation(document::SketchConstraintKind::perpendicular))
        return false;
    if (editor.lastSolveResult().status != features::SketchSolveStatus::conflicting ||
        editor.canAccept() || !editor.isElementConflicting(document::SketchElementId{1}) ||
        !editor.isElementConflicting(document::SketchElementId{2}) ||
        !editor.removeLastConstraint())
        return false;
    const auto sharedPoint = std::get<document::SketchLine>(editor.elements()[0].geometry).end;
    if (!editor.selectNearestReference(document::SketchElementId{1}, sharedPoint, false) ||
        !editor.translateSelected(1.0, 2.0))
        return false;
    const auto& draggedFirst = std::get<document::SketchLine>(editor.elements()[0].geometry).end;
    const auto& draggedSecond = std::get<document::SketchLine>(editor.elements()[1].geometry).start;
    return editor.lastSolveResult().status == features::SketchSolveStatus::success &&
           editor.canAccept() &&
           std::hypot(draggedFirst.x - draggedSecond.x, draggedFirst.y - draggedSecond.y) <= 1.0e-7;
}

bool pointLockAndPointOnObjectDriveEditorGeometry() {
    using namespace voelund;
    application::CadSession session;
    session.newDocument();
    const auto support = session.addBoxFeature();
    application::SketchEditorSession editor(session, support, plane);
    if (!add(editor, {1.0, 2.0}, {3.0, 4.0}, 1) ||
        !editor.selectNearestReference(document::SketchElementId{1}, {1.0, 2.0}, false) ||
        !editor.canLockSelectedReference() || !editor.lockSelectedReference() ||
        !editor.isSelectedReferenceLocked() || editor.constraints().size() != 2 ||
        !editor.translateSelected(5.0, 5.0))
        return false;
    const auto& locked = std::get<document::SketchLine>(editor.elements()[0].geometry).start;
    if (std::hypot(locked.x - 1.0, locked.y - 2.0) > 1.0e-7 || !editor.unlockSelectedReference() ||
        editor.isSelectedReferenceLocked() || !editor.translateSelected(5.0, 5.0) ||
        !add(editor, {0.0, 0.0}, {10.0, 0.0}, 2))
        return false;
    const auto source = std::get<document::SketchLine>(editor.elements()[0].geometry).start;
    if (!editor.selectNearestReference(document::SketchElementId{1}, source, false) ||
        !editor.selectNearestReference(document::SketchElementId{2}, {0.0, 0.0}, true) ||
        !editor.canAddSelectedRelation(document::SketchConstraintKind::pointOnObject) ||
        !editor.addSelectedRelation(document::SketchConstraintKind::pointOnObject))
        return false;
    const auto constrained = std::get<document::SketchLine>(editor.elements()[0].geometry).start;
    const auto& target = std::get<document::SketchLine>(editor.elements()[1].geometry);
    const double cross = (constrained.x - target.start.x) * (target.end.y - target.start.y) -
                         (constrained.y - target.start.y) * (target.end.x - target.start.x);
    return editor.lastSolveResult().status == features::SketchSolveStatus::success &&
           std::abs(cross) <= 1.0e-7;
}

bool equalAndTangentRelationsDriveEditorGeometry() {
    using namespace voelund;
    application::CadSession session;
    session.newDocument();
    const auto support = session.addBoxFeature();
    application::SketchEditorSession editor(session, support, plane);
    if (!add(editor, {0.0, 0.0}, {4.0, 0.0}, 1) || !add(editor, {0.0, 5.0}, {2.0, 5.0}, 2) ||
        !editor.selectNearestReference(document::SketchElementId{1}, {0.0, 0.0}, false) ||
        !editor.selectNearestReference(document::SketchElementId{2}, {0.0, 5.0}, true) ||
        !editor.canAddSelectedRelation(document::SketchConstraintKind::equal) ||
        editor.canAddSelectedRelation(document::SketchConstraintKind::tangent) ||
        !editor.addSelectedRelation(document::SketchConstraintKind::equal))
        return false;
    const auto& firstLine = std::get<document::SketchLine>(editor.elements()[0].geometry);
    const auto& secondLine = std::get<document::SketchLine>(editor.elements()[1].geometry);
    if (std::abs(
            std::hypot(firstLine.end.x - firstLine.start.x, firstLine.end.y - firstLine.start.y) -
            std::hypot(secondLine.end.x - secondLine.start.x,
                       secondLine.end.y - secondLine.start.y)) > 1.0e-7)
        return false;

    editor.setTool(application::SketchDrawingTool::circle);
    if (!add(editor, {8.0, 3.0}, {10.0, 3.0}, 3) || !add(editor, {14.0, 3.0}, {17.0, 3.0}, 4) ||
        !editor.selectNearestReference(document::SketchElementId{3}, {8.0, 3.0}, false) ||
        !editor.selectNearestReference(document::SketchElementId{4}, {14.0, 3.0}, true) ||
        !editor.canAddSelectedRelation(document::SketchConstraintKind::equal) ||
        !editor.canAddSelectedRelation(document::SketchConstraintKind::tangent) ||
        !editor.addSelectedRelation(document::SketchConstraintKind::equal))
        return false;
    const auto& firstCircle = std::get<document::SketchCircle>(editor.elements()[2].geometry);
    const auto& secondCircle = std::get<document::SketchCircle>(editor.elements()[3].geometry);
    if (std::abs(firstCircle.radius - secondCircle.radius) > 1.0e-7)
        return false;

    const auto linePoint = std::get<document::SketchLine>(editor.elements()[0].geometry).start;
    const auto circlePoint = std::get<document::SketchCircle>(editor.elements()[2].geometry).center;
    if (!editor.selectNearestReference(document::SketchElementId{1}, linePoint, false) ||
        !editor.selectNearestReference(document::SketchElementId{3}, circlePoint, true) ||
        editor.canAddSelectedRelation(document::SketchConstraintKind::equal) ||
        !editor.canAddSelectedRelation(document::SketchConstraintKind::tangent) ||
        !editor.addSelectedRelation(document::SketchConstraintKind::tangent))
        return false;
    return editor.lastSolveResult().status == features::SketchSolveStatus::success;
}

bool symmetricRelationUsesTwoPointsAndAnAxis() {
    using namespace voelund;
    application::CadSession session;
    session.newDocument();
    const auto support = session.addBoxFeature();
    application::SketchEditorSession editor(session, support, plane);
    if (!add(editor, {1.0, 2.0}, {2.0, 2.0}, 1) || !add(editor, {7.0, 3.0}, {8.0, 3.0}, 2) ||
        !add(editor, {4.0, -5.0}, {4.0, 5.0}, 3) ||
        !editor.selectNearestReference(document::SketchElementId{1}, {1.0, 2.0}, false) ||
        !editor.selectNearestReference(document::SketchElementId{2}, {7.0, 3.0}, true) ||
        editor.canAddSelectedRelation(document::SketchConstraintKind::symmetric) ||
        !editor.selectNearestReference(document::SketchElementId{3}, {4.0, -5.0}, true) ||
        !editor.canAddSelectedRelation(document::SketchConstraintKind::symmetric) ||
        editor.canAddSelectedRelation(document::SketchConstraintKind::equal) ||
        !editor.addSelectedRelation(document::SketchConstraintKind::symmetric))
        return false;

    const auto& first = std::get<document::SketchLine>(editor.elements()[0].geometry).start;
    const auto& second = std::get<document::SketchLine>(editor.elements()[1].geometry).start;
    const auto& axis = std::get<document::SketchLine>(editor.elements()[2].geometry);
    const document::SketchPoint midpoint{(first.x + second.x) * 0.5, (first.y + second.y) * 0.5};
    const double axisX = axis.end.x - axis.start.x;
    const double axisY = axis.end.y - axis.start.y;
    const double cross = axisX * (midpoint.y - axis.start.y) - axisY * (midpoint.x - axis.start.x);
    const double dot = (second.x - first.x) * axisX + (second.y - first.y) * axisY;
    return editor.lastSolveResult().status == features::SketchSolveStatus::success &&
           std::abs(cross) <= 1.0e-6 && std::abs(dot) <= 1.0e-6;
}

bool pointDistanceAndAngleDimensionsRemainEditable() {
    using namespace voelund;
    application::CadSession session;
    session.newDocument();
    const auto support = session.addBoxFeature();
    application::SketchEditorSession editor(session, support, plane);
    if (!add(editor, {0.0, 0.0}, {1.0, 0.0}, 1) || !add(editor, {3.0, 4.0}, {4.0, 4.0}, 2) ||
        !add(editor, {10.0, 0.0}, {14.0, 0.0}, 3) || !add(editor, {10.0, 3.0}, {13.0, 4.0}, 4) ||
        !editor.selectNearestReference(document::SketchElementId{1}, {0.0, 0.0}, false) ||
        !editor.selectNearestReference(document::SketchElementId{2}, {3.0, 4.0}, true) ||
        !editor.canAddSelectedRelation(document::SketchConstraintKind::pointDistance) ||
        !editor.addSelectedRelation(document::SketchConstraintKind::pointDistance))
        return false;
    const auto distanceId = editor.constraints().back().id;
    if (editor.constraintValue(distanceId) != std::optional<double>{5.0} ||
        !editor.setConstraintValue(distanceId, 8.0))
        return false;
    const auto firstPoint = std::get<document::SketchLine>(editor.elements()[0].geometry).start;
    const auto secondPoint = std::get<document::SketchLine>(editor.elements()[1].geometry).start;
    if (std::abs(std::hypot(secondPoint.x - firstPoint.x, secondPoint.y - firstPoint.y) - 8.0) >
        1.0e-7)
        return false;

    const auto thirdPoint = std::get<document::SketchLine>(editor.elements()[2].geometry).start;
    const auto fourthPoint = std::get<document::SketchLine>(editor.elements()[3].geometry).start;
    if (!editor.selectNearestReference(document::SketchElementId{3}, thirdPoint, false) ||
        !editor.selectNearestReference(document::SketchElementId{4}, fourthPoint, true) ||
        !editor.canAddSelectedRelation(document::SketchConstraintKind::angle) ||
        !editor.addSelectedRelation(document::SketchConstraintKind::angle))
        return false;
    const auto angleId = editor.constraints().back().id;
    if (!editor.setConstraintValue(angleId, 60.0))
        return false;
    const auto& firstLine = std::get<document::SketchLine>(editor.elements()[2].geometry);
    const auto& secondLine = std::get<document::SketchLine>(editor.elements()[3].geometry);
    const double firstX = firstLine.end.x - firstLine.start.x;
    const double firstY = firstLine.end.y - firstLine.start.y;
    const double secondX = secondLine.end.x - secondLine.start.x;
    const double secondY = secondLine.end.y - secondLine.start.y;
    const double angle =
        std::atan2(firstX * secondY - firstY * secondX, firstX * secondX + firstY * secondY) *
        180.0 / std::numbers::pi;
    return editor.constraintValue(angleId) == std::optional<double>{60.0} &&
           std::abs(angle - 60.0) <= 1.0e-7;
}

} // namespace

int main() {
    if (!completeEditorFlowIsOneDocumentTransaction()) {
        std::cerr << "Complete sketch editor flow failed\n";
        return 1;
    }
    if (!cancelLeavesNoFeatureOrUndoEntry()) {
        std::cerr << "Sketch editor cancellation changed history\n";
        return 1;
    }
    if (!toolChangesClearOnlyPreviewAndKeepStableIds()) {
        std::cerr << "Sketch editor tool switching lost state\n";
        return 1;
    }
    if (!lineClosureRecomputesProfilesAndOpenSketchRoundtrips()) {
        std::cerr << "Sketch editor profile or persistence flow failed\n";
        return 1;
    }
    if (!exclusivelyOpenGeometryIsAValidSketch()) {
        std::cerr << "Open sketch geometry was rejected\n";
        return 1;
    }
    if (!extrusionStillRequiresAClosedProfile()) {
        std::cerr << "Open sketch was incorrectly extruded\n";
        return 1;
    }
    if (!existingSketchCanBeEditedAsOneTransaction()) {
        std::cerr << "Existing sketch edit transaction failed\n";
        return 1;
    }
    if (!cancellingExistingSketchEditLeavesHistoryUntouched()) {
        std::cerr << "Cancelled existing sketch edit changed history\n";
        return 1;
    }
    if (!editingClosedGeometryRecomputesDependentExtrusion()) {
        std::cerr << "Edited sketch did not recompute its extrusion\n";
        return 1;
    }
    if (!lineDimensionAndConstraintPersistThroughEditing()) {
        std::cerr << "Sketch line dimension or constraint did not persist\n";
        return 1;
    }
    if (!rectangleDecomposesAndCircleDimensionPersists()) {
        std::cerr << "Rectangle decomposition or circle dimension did not persist\n";
        return 1;
    }
    if (!unchangedMeasurementCanBecomeDrivingConstraint()) {
        std::cerr << "Unchanged measurement did not become a driving constraint\n";
        return 1;
    }
    if (!endpointRelationsDriveGeometryAndConflictsStayRecoverable()) {
        std::cerr << "Endpoint relations or conflict recovery failed\n";
        return 1;
    }
    if (!pointLockAndPointOnObjectDriveEditorGeometry()) {
        std::cerr << "Point locking or point-on-object editor flow failed\n";
        return 1;
    }
    if (!equalAndTangentRelationsDriveEditorGeometry()) {
        std::cerr << "Equal or tangent editor relation failed\n";
        return 1;
    }
    if (!symmetricRelationUsesTwoPointsAndAnAxis()) {
        std::cerr << "Symmetric editor relation failed\n";
        return 1;
    }
    if (!pointDistanceAndAngleDimensionsRemainEditable()) {
        std::cerr << "Point distance or angle editing failed\n";
        return 1;
    }
    return 0;
}
