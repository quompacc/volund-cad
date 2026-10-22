// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "../BoundSketchFixture.hpp"
#include "../CommandResultAssertions.hpp"
#include "../TestPaths.hpp"
#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>

#include <filesystem>
#include <iostream>
#include <numbers>
#include <string>
#include <variant>
bool sketchEditorRelationCases();
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
    const auto support = voelund::test::requireCommittedFeature(session.createBoxFeature());
    voelund::application::SketchEditorSession editor(
        session, support, plane, voelund::test::topBinding(session, support, plane));

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

    const auto sketchId = voelund::test::requireCommittedFeature(editor.tryAccept());
    if (session.features().size() != 2 || session.selectedFeature() != sketchId ||
        session.selectedSketchParameters().elements.nextId != 7)
        return false;
    if (!(session.undo().status() == voelund::application::CommandStatus::committed) ||
        session.features().size() != 1)
        return false;
    return (session.redo().status() == voelund::application::CommandStatus::committed) &&
           session.features().size() == 2;
}
bool cancelLeavesNoFeatureOrUndoEntry() {
    voelund::application::CadSession session;
    session.newDocument();
    const auto support = voelund::test::requireCommittedFeature(session.createBoxFeature());
    voelund::application::SketchEditorSession editor(
        session, support, plane, voelund::test::topBinding(session, support, plane));
    if (!add(editor, {0.0, 0.0}, {2.0, 2.0}, 1))
        return false;
    editor.cancel();
    if (session.features().size() != 1 || editor.isActive())
        return false;
    return (session.undo().status() == voelund::application::CommandStatus::committed) &&
           session.features().empty();
}
bool toolChangesClearOnlyPreviewAndKeepStableIds() {
    voelund::application::CadSession session;
    session.newDocument();
    const auto support = voelund::test::requireCommittedFeature(session.createBoxFeature());
    voelund::application::SketchEditorSession editor(
        session, support, plane, voelund::test::topBinding(session, support, plane));
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
    const auto support = voelund::test::requireCommittedFeature(session.createBoxFeature());
    voelund::application::SketchEditorSession editor(
        session, support, plane, voelund::test::topBinding(session, support, plane));
    if (!add(editor, {0.0, 0.0}, {4.0, 0.0}, 1) || !add(editor, {4.0, 0.0}, {2.0, 3.0}, 2) ||
        editor.closedProfileCount() != 0 || !add(editor, {2.0, 3.0}, {0.0, 0.0}, 3) ||
        editor.closedProfileCount() != 1)
        return false;
    static_cast<void>(voelund::test::requireCommittedFeature(editor.tryAccept()));

    const std::filesystem::path path =
        voelund::test::temporaryFilePath("voelund-sketch-editor-roundtrip.volund");
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
    const auto support = voelund::test::requireCommittedFeature(session.createBoxFeature());
    voelund::application::SketchEditorSession editor(
        session, support, plane, voelund::test::topBinding(session, support, plane));
    if (!add(editor, {1.0, 1.0}, {6.0, 2.0}, 1))
        return false;
    const auto sketchId = voelund::test::requireCommittedFeature(editor.tryAccept());
    return session.featureStates().back().status ==
               voelund::application::FeatureRuntimeStatus::current &&
           session.selectFeature(sketchId) &&
           std::holds_alternative<voelund::document::SketchLine>(
               session.selectedSketchParameters().elements.values.front().geometry);
}

bool extrusionStillRequiresAClosedProfile() {
    voelund::application::CadSession session;
    session.newDocument();
    const auto support = voelund::test::requireCommittedFeature(session.createBoxFeature());
    const voelund::document::SketchParameters openSketch{
        plane,
        {{{voelund::document::SketchElementId{1},
           voelund::document::SketchLine{{0.0, 0.0}, {3.0, 1.0}}}},
         2}};
    const auto result = session.createSketchExtrusion(support, openSketch, 5.0);
    return result.status() == voelund::application::CommandStatus::rejected &&
           session.features().size() == 1 &&
           result.evaluation().features.back().message.find("closed profile") != std::string::npos;
}

bool existingSketchCanBeEditedAsOneTransaction() {
    voelund::application::CadSession session;
    session.newDocument();
    const auto support = voelund::test::requireCommittedFeature(session.createBoxFeature());
    voelund::application::SketchEditorSession create(
        session, support, plane, voelund::test::topBinding(session, support, plane));
    create.setTool(voelund::application::SketchDrawingTool::rectangle);
    if (!add(create, {0.0, 0.0}, {4.0, 3.0}, 1))
        return false;
    create.setTool(voelund::application::SketchDrawingTool::circle);
    if (!add(create, {8.0, 2.0}, {10.0, 2.0}, 5))
        return false;
    create.setTool(voelund::application::SketchDrawingTool::line);
    if (!add(create, {0.0, 6.0}, {3.0, 6.0}, 6))
        return false;
    const auto sketchId = voelund::test::requireCommittedFeature(create.tryAccept());
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
    static_cast<void>(voelund::test::requireCommittedFeature(edit.tryAccept()));

    const auto changed = session.sketchParameters(sketchId);
    if (changed.elements.values.size() != 6 || changed.elements.nextId != 8 || changed == original)
        return false;
    const auto& movedCircle =
        std::get<voelund::document::SketchCircle>(changed.elements.values[4].geometry);
    if (movedCircle.center != voelund::document::SketchPoint{7.0, 3.0})
        return false;
    if (!(session.undo().status() == voelund::application::CommandStatus::committed) ||
        session.sketchParameters(sketchId) != original ||
        !(session.redo().status() == voelund::application::CommandStatus::committed) ||
        session.sketchParameters(sketchId) != changed)
        return false;
    const std::filesystem::path path =
        voelund::test::temporaryFilePath("voelund-edited-sketch-roundtrip.volund");
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
    const auto support = voelund::test::requireCommittedFeature(session.createBoxFeature());
    voelund::application::SketchEditorSession create(
        session, support, plane, voelund::test::topBinding(session, support, plane));
    if (!add(create, {0.0, 0.0}, {4.0, 0.0}, 1))
        return false;
    const auto sketchId = voelund::test::requireCommittedFeature(create.tryAccept());
    const auto original = session.sketchParameters(sketchId);
    voelund::application::SketchEditorSession edit(session, sketchId, support, original);
    if (!edit.selectElement(voelund::document::SketchElementId{1}) ||
        !edit.translateSelected(5.0, 5.0))
        return false;
    edit.cancel();
    if (session.sketchParameters(sketchId) != original)
        return false;
    return (session.undo().status() == voelund::application::CommandStatus::committed) &&
           session.features().size() == 1;
}

bool editingClosedGeometryRecomputesDependentExtrusion() {
    voelund::application::CadSession session;
    session.newDocument();
    const auto support = voelund::test::requireCommittedFeature(session.createBoxFeature());
    const voelund::document::SketchParameters sketch{
        plane,
        {{{voelund::document::SketchElementId{1},
           voelund::document::SketchRectangle{1.0, 1.0, 5.0, 4.0}}},
         2}};
    static_cast<void>(voelund::test::requireCommittedFeature(
        session.createSketchExtrusion(support, sketch, 5.0)));
    const auto sketchId = session.features()[1].id;
    const auto original = session.sketchParameters(sketchId);
    voelund::application::SketchEditorSession edit(session, sketchId, support, original);
    if (!edit.selectElement(voelund::document::SketchElementId{1}) ||
        !edit.translateSelected(2.0, 1.0) || edit.closedProfileCount() != 1)
        return false;
    static_cast<void>(voelund::test::requireCommittedFeature(edit.tryAccept()));
    if (session.featureStates().back().status !=
        voelund::application::FeatureRuntimeStatus::current)
        return false;
    return (session.undo().status() == voelund::application::CommandStatus::committed) &&
           session.sketchParameters(sketchId) == original &&
           session.featureStates().back().status ==
               voelund::application::FeatureRuntimeStatus::current;
}

bool lineDimensionAndConstraintPersistThroughEditing() {
    voelund::application::CadSession session;
    session.newDocument();
    const auto support = voelund::test::requireCommittedFeature(session.createBoxFeature());
    voelund::application::SketchEditorSession create(
        session, support, plane, voelund::test::topBinding(session, support, plane));
    if (!add(create, {0.0, 0.0}, {3.0, 4.0}, 1) ||
        !create.selectElement(voelund::document::SketchElementId{1}) ||
        !create.setSelectedLineLength(10.0) ||
        !create.setSelectedLineConstraint(voelund::document::SketchLineConstraint::horizontal))
        return false;
    const auto sketchId = voelund::test::requireCommittedFeature(create.tryAccept());
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
    static_cast<void>(voelund::test::requireCommittedFeature(edit.tryAccept()));
    const auto moved = session.sketchParameters(sketchId);
    const auto& movedLine =
        std::get<voelund::document::SketchLine>(moved.elements.values.front().geometry);
    if (movedLine.start != voelund::document::SketchPoint{2.0, 3.0} ||
        movedLine.end != voelund::document::SketchPoint{12.0, 3.0} ||
        movedLine.constraint != voelund::document::SketchLineConstraint::horizontal) {
        return false;
    }
    if (!(session.undo().status() == voelund::application::CommandStatus::committed) ||
        session.sketchParameters(sketchId) != constrained ||
        !(session.redo().status() == voelund::application::CommandStatus::committed) ||
        session.sketchParameters(sketchId) != moved)
        return false;

    const std::filesystem::path path =
        voelund::test::temporaryFilePath("voelund-constrained-sketch-roundtrip.volund");
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
    const auto support = voelund::test::requireCommittedFeature(session.createBoxFeature());
    voelund::application::SketchEditorSession editor(
        session, support, plane, voelund::test::topBinding(session, support, plane));
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

    const auto sketchId = voelund::test::requireCommittedFeature(editor.tryAccept());
    const auto changed = session.sketchParameters(sketchId);
    const auto& circle =
        std::get<voelund::document::SketchCircle>(changed.elements.values[4].geometry);
    if (circle.radius != 3.5 || changed.elements.nextId != 6 ||
        changed.constraints.values.size() != 9 ||
        changed.constraints.values.back().kind != voelund::document::SketchConstraintKind::radius ||
        !(session.undo().status() == voelund::application::CommandStatus::committed) ||
        session.features().size() != 1 ||
        !(session.redo().status() == voelund::application::CommandStatus::committed) ||
        session.sketchParameters(sketchId) != changed)
        return false;

    const std::filesystem::path path =
        voelund::test::temporaryFilePath("voelund-sized-sketch-roundtrip.volund");
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
    const auto support = voelund::test::requireCommittedFeature(session.createBoxFeature());
    voelund::application::SketchEditorSession editor(
        session, support, plane, voelund::test::topBinding(session, support, plane));
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
    if (!sketchEditorRelationCases()) {
        std::cerr << "Sketch editor relation cases failed\n";
        return 1;
    }
    return 0;
}
