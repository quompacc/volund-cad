#include "../../src/desktop/ShellState.hpp"
#include "../CommandResultAssertions.hpp"
#include <iostream>
#include <stdexcept>
#include <voelund/application/CadSession.hpp>

// U02: the shell projection derives title, history, selection and command availability from
// the session plus the view-only picks, with the most specific pick winning.
namespace {
using namespace voelund;
using desktop::ShellSelection;
using desktop::ShellTool;
using document::DatumPlane;

void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

void emptyDocument() {
    application::CadSession session;
    session.newDocument();
    const auto state = desktop::projectShell(session, {});
    require(state.documentName == QStringLiteral("Untitled") && !state.modified && !state.canUndo &&
                !state.canRedo && state.modelCurrent,
            "Empty document header state");
    require(state.selection == ShellSelection::nothing && !state.canSketch() &&
                !state.canExtrude() && !state.canEditSketch() && state.tool == ShellTool::none,
            "Nothing is selected or available in an empty document");
    require(state.windowTitle() == QStringLiteral("Untitled — Völund CAD"), "Clean title");
}

void viewPicks() {
    application::CadSession session;
    session.newDocument();
    const auto plane = desktop::projectShell(session, {DatumPlane::xz, false, ShellTool::none});
    require(plane.selection == ShellSelection::basePlane && plane.basePlane == DatumPlane::xz &&
                plane.canSketch(),
            "A picked base plane offers a sketch");
    const auto both = desktop::projectShell(session, {DatumPlane::xy, true, ShellTool::none});
    require(both.selection == ShellSelection::basePlane, "Base plane wins over a stale face");
    const auto face = desktop::projectShell(session, {std::nullopt, true, ShellTool::sketchEditor});
    require(face.selection == ShellSelection::planarFace && face.canSketch() &&
                face.tool == ShellTool::sketchEditor,
            "A picked face offers a sketch and the active tool passes through");
}

void documentSelection() {
    application::CadSession session;
    session.newDocument();
    test::requireCommittedFeature(session.createBoxFeature());
    auto state = desktop::projectShell(session, {});
    require(state.selection == ShellSelection::feature &&
                state.featureKind == document::FeatureKind::box && state.modified &&
                state.canUndo && state.windowTitle() == QStringLiteral("Untitled * — Völund CAD"),
            "A new box is the selected, unsaved feature");
    session.newDocument();
    document::SketchParameters sketch{
        document::datumSketchPlane(DatumPlane::xy),
        document::SketchElements{
            {{document::SketchElementId{1}, document::SketchRectangle{0, 0, 20, 10}}}, 2}};
    sketch.datum = DatumPlane::xy;
    test::requireCommittedFeature(session.createDatumSketchFeature(sketch));
    state = desktop::projectShell(session, {});
    require(state.selection == ShellSelection::sketch && state.canEditSketch() &&
                !state.canExtrude(),
            "A selected sketch can be edited but not yet extruded");
    require(session.selectSketchProfileByElement(document::SketchElementId{1}, false),
            "Select the rectangle region");
    state = desktop::projectShell(session, {});
    require(state.selection == ShellSelection::sketchRegion && state.canExtrude(),
            "A selected region can be extruded");
}
} // namespace

int main() {
    try {
        emptyDocument();
        viewPicks();
        documentSelection();
        std::cout << "PASS shell state projection\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
