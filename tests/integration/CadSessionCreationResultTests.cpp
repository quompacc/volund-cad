// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "../CommandResultAssertions.hpp"
#include "CadSessionTestSupport.hpp"
#include <iostream>
#include <stdexcept>
#include <voelund/application/SketchEditorSession.hpp>

namespace {
using namespace voelund;
using application::CommandStatus;
using application::RejectionReason;
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
const document::SketchPlane plane{0, 0, 40, 0, 0, 1, 1, 0, 0};
document::SketchParameters openSketch() {
    return {plane, {{{document::SketchElementId{1}, document::SketchLine{{1, 1}, {4, 3}}}}, 2}};
}
void rejectedCreationPreservesState() {
    application::CadSession session;
    session.newDocument();
    const auto boxResult = session.createBoxFeature();
    require(boxResult.status() == CommandStatus::committed && boxResult.feature(), "create box");
    const auto box = *boxResult.feature();
    const auto next = session.createBoxFeature();
    require(next.feature().has_value() &&
                (session.undo().status() == voelund::application::CommandStatus::committed),
            "create redo branch");
    const auto shape = session.currentShape();
    const auto selection = session.selectedFeature();
    const auto rejected = session.createSketchExtrusion(box, openSketch(), 5);
    require(rejected.status() == CommandStatus::rejected &&
                rejected.reason() == RejectionReason::evaluationFailed && !rejected.feature() &&
                session.features().size() == 1 && session.currentShape().IsSame(shape) &&
                session.selectedFeature() == selection && session.canRedo(),
            "rejected compound creation published state or destroyed redo");
    require(rejected.evaluation().features.back().status == features::EvaluationStatus::failed,
            "missing failed feature diagnostic");
    require((session.redo().status() == voelund::application::CommandStatus::committed) &&
                session.features().back().id == *next.feature() &&
                (session.undo().status() == voelund::application::CommandStatus::committed),
            "rejected creation changed redo identity");
    const auto sketch = session.createSketchFeature(box, openSketch());
    require(sketch.status() == CommandStatus::committed &&
                sketch.feature() == document::FeatureId{next.feature()->value() + 1},
            "rejection consumed IDs or undo reused a published ID");
    require((session.undo().status() == voelund::application::CommandStatus::committed) &&
                session.features().size() == 1 &&
                (session.undo().status() == voelund::application::CommandStatus::committed) &&
                session.features().empty() && !session.isModified(),
            "rejection added history or changed clean baseline");
}
void editorRejectsFailedDependentExtrusion() {
    application::CadSession session;
    session.newDocument();
    const auto box = voelund::test::requireCommittedFeature(session.createBoxFeature());
    const auto extrusion =
        session.createSketchExtrusion(box, plane, document::SketchRectangle{1, 1, 5, 4}, 5);
    require(extrusion.status() == CommandStatus::committed, "extrusion fixture");
    const auto sketchId = session.features()[1].id;
    const auto original = session.sketchParameters(sketchId);
    const auto shape = session.currentShape();
    application::SketchEditorSession editor(session, sketchId, box, original);
    require(editor.selectElement(document::SketchElementId{1}) && editor.removeSelected(),
            "remove rectangle");
    editor.setTool(application::SketchDrawingTool::line);
    require(editor.setPreview({1, 1}, {4, 3}) && editor.confirmPreview().has_value() &&
                editor.canAccept(),
            "open sketch is solver-valid");
    const auto rejected = editor.tryAccept();
    require(rejected.status() == CommandStatus::rejected && editor.isActive() &&
                session.sketchParameters(sketchId) == original &&
                session.currentShape().IsSame(shape),
            "dependent failure changed document or closed editor");
    require(editor.selectElement(editor.elements().front().id) && editor.removeSelected(),
            "remove open line");
    editor.setTool(application::SketchDrawingTool::rectangle);
    require(editor.setPreview({1, 1}, {7, 6}) && editor.confirmPreview().has_value(), "repair");
    const auto accepted = editor.tryAccept();
    require(accepted.status() == CommandStatus::committed && !editor.isActive() &&
                (session.undo().status() == voelund::application::CommandStatus::committed) &&
                session.sketchParameters(sketchId) == original,
            "repair must produce one reversible edit");
    application::SketchEditorSession unchanged(session, sketchId, box, original);
    require(unchanged.tryAccept().reason() == RejectionReason::noChange && !unchanged.isActive() &&
                session.canRedo(),
            "unchanged confirmation must preserve redo");
}
void explicitEditableError() {
    application::CadSession session;
    session.newDocument();
    const auto box = voelund::test::requireCommittedFeature(session.createBoxFeature());
    static_cast<void>(voelund::test::requireCommittedFeature(
        session.createSketchExtrusion(box, plane, {1, 1, 5, 4}, 5)));
    const auto sketch = session.features()[1].id;
    const auto original = session.sketchParameters(sketch);
    const auto rejected = session.editSketchParameters(sketch, openSketch());
    require(rejected.status() == CommandStatus::rejected &&
                session.sketchParameters(sketch) == original,
            "default edit must reject errors");
    const auto kept = session.editSketchParameters(
        sketch, openSketch(), application::ErrorDisposition::keepEditableErrors);
    require(kept.status() == CommandStatus::committedWithErrors && kept.feature() == sketch &&
                session.sketchParameters(sketch) == openSketch(),
            "explicit error commit");
    const auto newFeature = session.createBoxFeature();
    require(newFeature.status() == CommandStatus::rejected && session.features().size() == 3,
            "new feature cannot implicitly inherit an invalid model");
    require((session.undo().status() == voelund::application::CommandStatus::committed) &&
                session.sketchParameters(sketch) == original,
            "one undo repairs explicit error commit");
}
} // namespace
int main() {
    try {
        rejectedCreationPreservesState();
        editorRejectsFailedDependentExtrusion();
        explicitEditableError();
        std::cout << "Creation result and editor rollback tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
