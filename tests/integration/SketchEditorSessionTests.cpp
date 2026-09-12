#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>

#include <filesystem>
#include <iostream>
#include <string>
#include <variant>

namespace {

constexpr voelund::document::SketchPlane plane{
    0.0, 0.0, 40.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0};

bool add(voelund::application::SketchEditorSession& editor,
         voelund::document::SketchPoint first,
         voelund::document::SketchPoint second,
         std::uint64_t expectedId) {
    return editor.setPreview(first, second)
        && editor.confirmPreview() == voelund::document::SketchElementId{expectedId};
}

bool completeEditorFlowIsOneDocumentTransaction() {
    voelund::application::CadSession session;
    session.newDocument();
    const auto support = session.addBoxFeature();
    voelund::application::SketchEditorSession editor(session, support, plane);

    if (!add(editor, {0.0, 0.0}, {5.0, 0.0}, 1)) return false;
    editor.setTool(voelund::application::SketchDrawingTool::rectangle);
    if (!add(editor, {1.0, 1.0}, {4.0, 3.0}, 2)) return false;
    editor.setTool(voelund::application::SketchDrawingTool::circle);
    if (!add(editor, {8.0, 3.0}, {10.0, 3.0}, 3)) return false;
    if (editor.elements().size() != 3 || editor.closedProfileCount() != 2
        || !editor.selectElement(voelund::document::SketchElementId{2})) return false;

    const auto sketchId = editor.accept();
    if (session.features().size() != 2 || session.selectedFeature() != sketchId
        || session.selectedSketchParameters().elements.nextId != 4) return false;
    if (!session.undo() || session.features().size() != 1) return false;
    return session.redo() && session.features().size() == 2;
}

bool cancelLeavesNoFeatureOrUndoEntry() {
    voelund::application::CadSession session;
    session.newDocument();
    const auto support = session.addBoxFeature();
    voelund::application::SketchEditorSession editor(session, support, plane);
    if (!add(editor, {0.0, 0.0}, {2.0, 2.0}, 1)) return false;
    editor.cancel();
    if (session.features().size() != 1 || editor.isActive()) return false;
    return session.undo() && session.features().empty();
}

bool toolChangesClearOnlyPreviewAndKeepStableIds() {
    voelund::application::CadSession session;
    session.newDocument();
    const auto support = session.addBoxFeature();
    voelund::application::SketchEditorSession editor(session, support, plane);
    if (!editor.setPreview({0.0, 0.0}, {1.0, 0.0})) return false;
    editor.setTool(voelund::application::SketchDrawingTool::circle);
    if (!editor.evaluatedElements().empty()) return false;
    if (!add(editor, {5.0, 5.0}, {7.0, 5.0}, 1)) return false;
    editor.setTool(voelund::application::SketchDrawingTool::line);
    return add(editor, {0.0, 0.0}, {2.0, 0.0}, 2)
        && editor.elements().front().id == voelund::document::SketchElementId{1};
}

bool lineClosureRecomputesProfilesAndOpenSketchRoundtrips() {
    voelund::application::CadSession session;
    session.newDocument();
    const auto support = session.addBoxFeature();
    voelund::application::SketchEditorSession editor(session, support, plane);
    if (!add(editor, {0.0, 0.0}, {4.0, 0.0}, 1)
        || !add(editor, {4.0, 0.0}, {2.0, 3.0}, 2)
        || editor.closedProfileCount() != 0
        || !add(editor, {2.0, 3.0}, {0.0, 0.0}, 3)
        || editor.closedProfileCount() != 1) return false;
    static_cast<void>(editor.accept());

    const std::filesystem::path path =
        std::filesystem::current_path() / "voelund-sketch-editor-roundtrip.volund";
    session.saveDocument(path);
    voelund::application::CadSession loaded;
    loaded.loadDocument(path);
    std::error_code error;
    std::filesystem::remove(path, error);
    if (error || loaded.features().size() != 2) return false;
    const auto sketchId = loaded.features().back().id;
    return loaded.selectFeature(sketchId)
        && loaded.selectedSketchParameters().elements.values.size() == 3;
}

bool exclusivelyOpenGeometryIsAValidSketch() {
    voelund::application::CadSession session;
    session.newDocument();
    const auto support = session.addBoxFeature();
    voelund::application::SketchEditorSession editor(session, support, plane);
    if (!add(editor, {1.0, 1.0}, {6.0, 2.0}, 1)) return false;
    const auto sketchId = editor.accept();
    return session.featureStates().back().status
            == voelund::application::FeatureRuntimeStatus::current
        && session.selectFeature(sketchId)
        && std::holds_alternative<voelund::document::SketchLine>(
            session.selectedSketchParameters().elements.values.front().geometry);
}

bool extrusionStillRequiresAClosedProfile() {
    voelund::application::CadSession session;
    session.newDocument();
    const auto support = session.addBoxFeature();
    const voelund::document::SketchParameters openSketch{
        plane, {{{voelund::document::SketchElementId{1},
                  voelund::document::SketchLine{{0.0, 0.0}, {3.0, 1.0}}}}, 2}};
    static_cast<void>(session.addSketchExtrusion(support, openSketch, 5.0));
    return session.featureStates().back().status
            == voelund::application::FeatureRuntimeStatus::error
        && session.featureStates().back().message.find("closed profile") != std::string::npos;
}

} // namespace

int main() {
    if (!completeEditorFlowIsOneDocumentTransaction()) {
        std::cerr << "Complete sketch editor flow failed\n"; return 1;
    }
    if (!cancelLeavesNoFeatureOrUndoEntry()) {
        std::cerr << "Sketch editor cancellation changed history\n"; return 1;
    }
    if (!toolChangesClearOnlyPreviewAndKeepStableIds()) {
        std::cerr << "Sketch editor tool switching lost state\n"; return 1;
    }
    if (!lineClosureRecomputesProfilesAndOpenSketchRoundtrips()) {
        std::cerr << "Sketch editor profile or persistence flow failed\n"; return 1;
    }
    if (!exclusivelyOpenGeometryIsAValidSketch()) {
        std::cerr << "Open sketch geometry was rejected\n"; return 1;
    }
    if (!extrusionStillRequiresAClosedProfile()) {
        std::cerr << "Open sketch was incorrectly extruded\n"; return 1;
    }
    return 0;
}
