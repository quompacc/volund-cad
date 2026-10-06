#include "IsolatedSettings.hpp"
#include "../../src/desktop/SketchEditorToolController.hpp"
#include "../../src/desktop/ToolPanelHost.hpp"
#include "../CommandResultAssertions.hpp"
#include <QApplication>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QMainWindow>
#include <iostream>
#include <stdexcept>
#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>
#include <voelund/viewport/CadViewport.hpp>

// U09 (A-TXN-03): a drag is one user action. 100 pointer moves give one undo step, Escape
// during the drag restores the starting state and keeps the editor open, a bare refresh
// records nothing, and a typed translation is one step when its editing ends.
namespace voelund::desktop {
class SketchSolverGateTestAccess {
  public:
    static void install(SketchEditorToolController& controller,
                        std::unique_ptr<application::SketchEditorSession> editor) {
        controller.editor_ = std::move(editor);
        controller.showPanel();
    }
    static application::SketchEditorSession& editor(SketchEditorToolController& controller) {
        return *controller.editor_;
    }
    static void escape(SketchEditorToolController& controller) { controller.stepBack(); }
    static void refresh(SketchEditorToolController& controller) {
        controller.refreshPresentation();
    }
};
} // namespace voelund::desktop

namespace {
using namespace voelund;
using Access = desktop::SketchSolverGateTestAccess;

void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
bool sameElements(std::span<const document::SketchElement> a,
                  const std::vector<document::SketchElement>& b) {
    return std::ranges::equal(a, b);
}
std::vector<document::SketchElement> copy(std::span<const document::SketchElement> elements) {
    return {elements.begin(), elements.end()};
}

void run() {
    application::CadSession session;
    session.newDocument();
    QMainWindow window;
    auto* panel = new desktop::ToolPanelHost(&window);
    window.setCentralWidget(new QLabel(QStringLiteral("Sketch drag")));
    viewport::CadViewport viewport; // No shown GL surface is needed for this contract.
    desktop::SketchEditorToolController controller(session, viewport, *panel, [] {},
                                                   [](const QString&) {});
    auto editor = std::make_unique<application::SketchEditorSession>(session,
                                                                     document::DatumPlane::xy);
    editor->setTool(application::SketchDrawingTool::line);
    require(editor->create({{0, 0}, {10, 3}}).has_value(), "line");
    editor->commitHistory();
    Access::install(controller, std::move(editor));
    window.resize(900, 700);
    window.show();
    QApplication::processEvents();
    auto& active = Access::editor(controller);
    const auto line = active.elements().front().id;
    const auto start = copy(active.elements());

    // A bare refresh records nothing.
    const bool couldUndo = active.canUndo();
    Access::refresh(controller);
    Access::refresh(controller);
    require(active.canUndo() == couldUndo, "A refresh is not a step");

    // 100 pointer moves, then pointer-up: one step.
    require(controller.selectElement({line.value(), 5, 1.5, false}), "select line");
    for (int move = 0; move < 100; ++move)
        controller.translateFromViewport(0.1, 0.05);
    require(!sameElements(active.elements(), start) && active.isDragging(), "dragged");
    controller.finishDrag();
    const auto dragged = copy(active.elements());
    require(active.undo() && sameElements(active.elements(), start),
            "One undo restores the state before the 100 moves");
    require(active.redo() && sameElements(active.elements(), dragged), "Redo repeats the drag");

    // Escape during a drag: back to the start of that drag, the editor stays open.
    for (int move = 0; move < 30; ++move)
        controller.translateFromViewport(-0.2, 0.0);
    Access::escape(controller);
    require(!active.isDragging() && sameElements(active.elements(), dragged) &&
                controller.isActive(),
            "Escape cancels the drag and keeps the editor");
    require(active.undo() && sameElements(active.elements(), start),
            "The cancelled drag left no step");
    require(active.redo(), "redo");

    // A typed translation: several value changes, one step when editing ends.
    QDoubleSpinBox* deltaX = nullptr;
    for (auto* input : panel->findChildren<QDoubleSpinBox*>(QStringLiteral("toolLength_translation_x")))
        deltaX = input;
    require(deltaX != nullptr, "translation input");
    require(controller.selectElement({line.value(), 5, 1.5, false}), "select line again");
    for (const double value : {1.0, 1.5, 2.0})
        deltaX->setValue(value);
    emit deltaX->editingFinished();
    const auto typed = copy(active.elements());
    require(!active.isDragging() && !sameElements(typed, dragged), "typed translation applied");
    require(active.undo() && sameElements(active.elements(), dragged),
            "One undo takes back the whole typed translation");
    std::cout << "PASS U09 sketch drag: 100 moves one step, Escape, refresh, typed input\n";
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    try {
        run();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
