#include "../CommandResultAssertions.hpp"
#include "CadSessionTestSupport.hpp"
#include <random>
#include <stdexcept>
#include <voelund/application/SketchEditorSession.hpp>

namespace {
using namespace voelund;
void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
const document::SketchPlane plane{0, 0, 40, 0, 0, 1, 1, 0, 0};
struct Fixture {
    std::filesystem::path path =
        test::temporaryFilePath("d06-" + std::to_string(std::random_device{}()) + ".volund");
    ~Fixture() {
        std::error_code error;
        std::filesystem::remove(path, error);
    }
};
void compoundSaveHistory() {
    Fixture fixture;
    application::CadSession session;
    session.newDocument();
    const auto box = voelund::test::requireCommittedFeature(session.createBoxFeature());
    session.saveDocument(fixture.path);
    session.loadDocument(fixture.path);
    const auto volume = test::volumeOf(session.currentShape());
    const auto extrusion = voelund::test::requireCommittedFeature(
        session.createSketchExtrusion(box, plane, document::SketchRectangle{2, 3, 8, 9}, 10));
    require(session.features().size() == 3 && session.isModified() &&
                std::abs(test::volumeOf(session.currentShape()) - volume - 360) < 1e-6,
            "compound result");
    const auto sketch = session.features()[1].id;
    require((session.undo().status() == voelund::application::CommandStatus::committed) &&
                session.features().size() == 1 && !session.isModified() && !session.canUndo() &&
                session.selectedFeature() == box,
            "one undo did not restore saved base");
    require((session.redo().status() == voelund::application::CommandStatus::committed) &&
                session.features().size() == 3 && session.features()[1].id == sketch &&
                session.features()[2].id == extrusion && session.isModified() &&
                std::abs(test::volumeOf(session.currentShape()) - volume - 360) < 1e-6,
            "redo identities/shape");
    require(session.selectFeature(extrusion), "select extrusion");
    session.saveDocument(fixture.path);
    require(!(session.editSelectedExtrusionParameters({10, {}}).status() ==
              voelund::application::CommandStatus::committed) &&
                !session.isModified(),
            "no-op dirtied saved file");
    require((session.editSelectedExtrusionParameters({15, {}}).status() ==
             voelund::application::CommandStatus::committed) &&
                session.isModified(),
            "edit not committed");
    require((session.undo().status() == voelund::application::CommandStatus::committed) &&
                !session.isModified() && session.selectedExtrusionParameters().distance == 10,
            "edit undo did not reach saved state");
    const auto shape = session.currentShape();
    const bool rejected = session.editSelectedExtrusionParameters({-1, {}}).reason() ==
                          application::RejectionReason::invalidInput;
    require(rejected && !session.isModified() && session.canRedo() &&
                session.currentShape().IsSame(shape),
            "invalid edit changed state/redo");
}
void separateToolsAndPreview() {
    Fixture fixture;
    application::CadSession session;
    session.newDocument();
    const auto box = voelund::test::requireCommittedFeature(session.createBoxFeature());
    session.saveDocument(fixture.path);
    session.loadDocument(fixture.path);
    {
        application::SketchEditorSession preview(session, box, plane);
        preview.setTool(application::SketchDrawingTool::rectangle);
        for (int i = 0; i < 100; ++i) {
            require(preview.setPreview({2, 3}, {8, 9}), "preview failed");
            preview.clearPreview();
            require(session.selectFeature(box) && !session.isModified() && !session.canUndo(),
                    "preview/selection generated a document command");
        }
        preview.cancel();
    }
    application::SketchEditorSession editor(session, box, plane);
    editor.setTool(application::SketchDrawingTool::rectangle);
    require(editor.setPreview({2, 3}, {8, 9}) && editor.confirmPreview().has_value(),
            "sketch rectangle");
    const auto sketch = voelund::test::requireCommittedFeature(editor.tryAccept());
    require(session.features().size() == 2 &&
                session.selectSketchProfileByElement(document::SketchElementId{1}, false),
            "separate sketch acceptance");
    const auto extrusion = voelund::test::requireCommittedFeature(
        session.createSketchProfileExtrusion(sketch, session.selectedSketchProfiles(), 10));
    require((session.undo().status() == voelund::application::CommandStatus::committed) &&
                session.features().size() == 2 && session.features().back().id == sketch &&
                session.isModified(),
            "separate extrusion undo removed sketch");
    require((session.undo().status() == voelund::application::CommandStatus::committed) &&
                session.features().size() == 1 && !session.isModified() && !session.canUndo(),
            "separate sketch undo");
    require((session.redo().status() == voelund::application::CommandStatus::committed) &&
                session.features().size() == 2 &&
                (session.redo().status() == voelund::application::CommandStatus::committed) &&
                session.features().back().id == extrusion,
            "separate confirmations redo");
}
} // namespace
int main() {
    try {
        compoundSaveHistory();
        separateToolsAndPreview();
        std::cout << "Session transaction tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}