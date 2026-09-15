#include "../CommandResultAssertions.hpp"
#include "../TestPaths.hpp"
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
bool endpointRelationsDriveGeometryAndConflictsStayRecoverable() {
    using namespace voelund;
    application::CadSession session;
    session.newDocument();
    const auto support = voelund::test::requireCommittedFeature(session.createBoxFeature());
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
    const auto support = voelund::test::requireCommittedFeature(session.createBoxFeature());
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
    const auto support = voelund::test::requireCommittedFeature(session.createBoxFeature());
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
    const auto support = voelund::test::requireCommittedFeature(session.createBoxFeature());
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
    const auto support = voelund::test::requireCommittedFeature(session.createBoxFeature());
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
    editor.commitHistory();
    if (!editor.setConstraintAngle(angleId, kernel::Radians(std::numbers::pi / 3.0)))
        return false;
    editor.commitHistory();
    if (!editor.setConstraintAngle(angleId, kernel::Radians(-std::numbers::pi / 3.0)))
        return false;
    editor.commitHistory();
    if (editor.constraints().back().angleValue != kernel::Radians(-std::numbers::pi / 3.0) ||
        !editor.undo() ||
        editor.constraintAngle(angleId) != kernel::Radians(std::numbers::pi / 3.0) ||
        !editor.redo() ||
        editor.constraintAngle(angleId) != kernel::Radians(-std::numbers::pi / 3.0) ||
        !editor.undo())
        return false;
    const auto preserved = editor.draftParameters();
    if (editor.setConstraintValue(angleId, 60.0) ||
        editor.setConstraintAngle(angleId, kernel::Radians(std::numbers::pi)) ||
        editor.setConstraintAngle(angleId, kernel::Radians(0.0)) ||
        editor.draftParameters() != preserved)
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
    return editor.constraintAngle(angleId) == kernel::Radians(std::numbers::pi / 3.0) &&
           std::abs(angle - 60.0) <= 1.0e-7;
}

} // namespace
bool sketchEditorRelationCases() {
    return endpointRelationsDriveGeometryAndConflictsStayRecoverable() &&
           pointLockAndPointOnObjectDriveEditorGeometry() &&
           equalAndTangentRelationsDriveEditorGeometry() &&
           symmetricRelationUsesTwoPointsAndAnAxis() &&
           pointDistanceAndAngleDimensionsRemainEditable();
}
