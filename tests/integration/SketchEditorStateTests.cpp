// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>

#include <iostream>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

// S02: the editor state stays whole. The accept gate follows the solve status (under-determined
// is valid, conflicting and failed are not); reopening solves at once; a failed or rejected
// step leaves nothing half changed; undo, redo and a cancelled drag bring back selection and
// solve status together.
namespace {
using namespace voelund;
using application::CommandStatus;
using application::SketchCreationInput;
using application::SketchDrawingTool;
using application::SketchEditorSession;
using document::SketchElementId;
using document::SketchPoint;
using features::SketchSolveStatus;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}

struct Observed final {
    std::vector<document::SketchElement> elements;
    std::vector<document::SketchConstraint> constraints;
    std::optional<SketchElementId> selected;
    std::vector<document::SketchGeometryReference> references;
    SketchSolveStatus status;
    int dof;
    bool canAccept;
    bool operator==(const Observed&) const = default;
};
Observed observe(const SketchEditorSession& editor) {
    return {{editor.elements().begin(), editor.elements().end()},
            {editor.constraints().begin(), editor.constraints().end()},
            editor.selectedElement(),
            {editor.selectedReferences().begin(), editor.selectedReferences().end()},
            editor.lastSolveResult().status,
            editor.lastSolveResult().degreesOfFreedom,
            editor.canAccept()};
}

SketchElementId line(SketchEditorSession& editor, SketchPoint from, SketchPoint to) {
    editor.setTool(SketchDrawingTool::line);
    const auto created = editor.create({from, to});
    require(created.has_value(), "draw a line");
    editor.commitHistory();
    return created->primary();
}

// Two lines both parallel and perpendicular: the solve fails or conflicts.
void makeConflict(SketchEditorSession& editor, SketchElementId first, SketchElementId second) {
    require(editor.selectNearestReference(first, {4, 1}, false) &&
                editor.selectNearestReference(second, {6, 2}, true) &&
                editor.addSelectedRelation(document::SketchConstraintKind::parallel) &&
                editor.addSelectedRelation(document::SketchConstraintKind::perpendicular),
            "conflict fixture");
    editor.commitHistory();
}

void acceptFollowsSolveStatus(application::CadSession& session) {
    SketchEditorSession editor(session, document::DatumPlane::xy);
    require(editor.lastSolveResult().status == SketchSolveStatus::success &&
                editor.lastSolveResult().degreesOfFreedom == 0 && !editor.canAccept(),
            "A new empty sketch reports 0 DOF and cannot be accepted");
    const auto first = line(editor, {0, 0}, {4, 1});
    require(editor.lastSolveResult().degreesOfFreedom > 0 && editor.canAccept(),
            "An under-determined valid sketch can be accepted");
    const auto second = line(editor, {6, 2}, {7, 6});
    makeConflict(editor, first, second);
    require(editor.lastSolveResult().status != SketchSolveStatus::success && !editor.canAccept() &&
                editor.tryAccept().status() == CommandStatus::rejected && editor.isActive(),
            "A contradicting (failed or conflicting) sketch cannot be accepted");
    editor.beginDrag();
    require(!editor.canAccept(), "No accept while a drag runs");
    editor.cancelDrag();
}

void reopenSolvesAtOnce(application::CadSession& session) {
    SketchEditorSession editor(session, document::DatumPlane::xy);
    static_cast<void>(line(editor, {0, 0}, {4, 1}));
    const int dof = editor.lastSolveResult().degreesOfFreedom;
    const auto result = editor.tryAccept();
    require(result.status() == CommandStatus::committed && result.feature(), "accept");
    const auto stored = session.identifiedSnapshot().snapshot().sketchParameters(*result.feature());
    SketchEditorSession reopened(session, *result.feature(), stored);
    require(reopened.lastSolveResult().status == SketchSolveStatus::success &&
                reopened.lastSolveResult().degreesOfFreedom == dof && reopened.canAccept(),
            "A reopened sketch reports its solve status and DOF at once");
}

void failedStepsChangeNothing(application::CadSession& session) {
    SketchEditorSession editor(session, document::DatumPlane::xy);
    const auto first = line(editor, {0, 0}, {4, 1});
    require(editor.selectElement(first), "select");
    const auto before = observe(editor);
    require(!editor.translateSelected(1000001, 0) && observe(editor) == before,
            "A rejected translation changes nothing");

    // A rejected snap after the shape was already added: nothing of the step remains.
    editor.setTool(SketchDrawingTool::circle);
    SketchCreationInput input{{10, 0}, {12, 0}};
    input.firstSnap = application::SketchSnap{application::SketchSnapKind::center, first};
    bool rejected = false;
    try {
        static_cast<void>(editor.create(input));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected && observe(editor) == before && editor.tool() == SketchDrawingTool::circle,
            "A rejected creation step changes nothing");

    SketchEditorSession centered(session, document::DatumPlane::xy);
    static_cast<void>(line(centered, {0, 0}, {4, 1}));
    centered.setTool(SketchDrawingTool::rectangle);
    const auto beforeCentered = observe(centered);
    SketchCreationInput around{{20, 0}, {24, 3}};
    around.centered = true;
    around.secondSnap =
        application::SketchSnap{application::SketchSnapKind::midpoint, SketchElementId{999}};
    rejected = false;
    try {
        static_cast<void>(centered.create(around));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected && observe(centered) == beforeCentered &&
                centered.tool() == SketchDrawingTool::rectangle,
            "A rejected centered rectangle restores elements and tool");
}

void undoRedoRestoreSelectionAndStatus(application::CadSession& session) {
    SketchEditorSession editor(session, document::DatumPlane::xy);
    const auto first = line(editor, {0, 0}, {4, 1});
    const auto second = line(editor, {6, 2}, {7, 6});
    const auto beforeConflict = observe(editor);
    makeConflict(editor, first, second);
    const auto conflict = observe(editor);
    require(editor.undo(), "undo the conflict");
    require(observe(editor) == beforeConflict,
            "Undo brings back geometry, selection and the successful solve");
    require(editor.redo() && observe(editor) == conflict,
            "Redo brings back the conflict with its selection");
    require(editor.undo() && editor.undo(), "undo to the first line");
    require(editor.selectedElement() == first && editor.elements().size() == 1 &&
                editor.lastSolveResult().status == SketchSolveStatus::success && editor.canAccept(),
            "Undo selects the element of its step");
}

void cancelledDragRestoresState(application::CadSession& session) {
    SketchEditorSession editor(session, document::DatumPlane::xy);
    const auto first = line(editor, {0, 0}, {4, 1});
    require(editor.selectElement(first), "select");
    const auto before = observe(editor);
    editor.beginDrag();
    require(editor.dragSelected(3, 2) && !editor.canAccept(), "drag");
    editor.cancelDrag();
    require(observe(editor) == before, "Escape restores geometry, selection and solve status");

    editor.beginDrag();
    require(editor.dragSelected(3, 2), "drag again");
    editor.cancel();
    require(!editor.isDragging() && !editor.canAccept() && !editor.selectedElement(),
            "Cancel ends a running drag");
}
} // namespace

int main() {
    try {
        application::CadSession session;
        session.newDocument();
        acceptFollowsSolveStatus(session);
        reopenSolvesAtOnce(session);
        failedStepsChangeNothing(session);
        undoRedoRestoreSelectionAndStatus(session);
        cancelledDragRestoresState(session);
        std::cout << "PASS S02 editor state survives solve, accept, cancel and undo\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
