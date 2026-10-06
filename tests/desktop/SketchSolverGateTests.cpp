#include "IsolatedSettings.hpp"
#include "../../src/desktop/SketchEditorToolController.hpp"
#include "../../src/desktop/ToolPanelHost.hpp"
#include "../CommandResultAssertions.hpp"
#include <QApplication>
#include <QDir>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <iostream>
#include <stdexcept>
#include <voelund/application/CadSession.hpp>

namespace voelund::application {
class SketchSolverGateTestAccess {
  public:
    static void set(SketchEditorSession& editor, features::SketchSolveStatus state, int dof) {
        editor.lastSolveResult_ = {state, dof, {}};
    }
};
} // namespace voelund::application
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
    static void sync(SketchEditorToolController& controller) { controller.syncConstraintState(); }
};
} // namespace voelund::desktop
namespace {
using namespace voelund;
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
const document::SketchPlane plane{0, 0, 20, 0, 0, 1, 1, 0, 0};
void loadedConflict(application::CadSession& session) {
    using namespace document;
    const SketchGeometryReference reference{SketchElementId{1}, SketchPointPosition::start};
    SketchParameters parameters{
        plane,
        {{{SketchElementId{1}, SketchLine{{0, 0}, {3, 4}}}}, 2},
        {{{SketchConstraintId{40}, SketchConstraintKind::distance, reference, std::nullopt, 5},
          {SketchConstraintId{41}, SketchConstraintKind::horizontal, reference, std::nullopt, 0},
          {SketchConstraintId{42}, SketchConstraintKind::vertical, reference, std::nullopt, 0}},
         43}};
    application::SketchEditorSession loaded(session, FeatureId{99}, ModelDocument::initialBoxId(),
                                            parameters);
    require(loaded.lastSolveResult().status == features::SketchSolveStatus::conflicting &&
                !loaded.canAccept(),
            "existing conflicting sketch was accepted before its first solve");
}
void run() {
    application::CadSession session;
    loadedConflict(session);
    QMainWindow window;
    auto* panel = new desktop::ToolPanelHost(&window);
    window.setCentralWidget(new QLabel(QStringLiteral("Sketch solver acceptance regression")));
    viewport::CadViewport viewport; // No shown GL surface is needed for this panel contract.
    desktop::SketchEditorToolController controller(
        session, viewport, *panel, [] {}, [](const QString&) {});
    auto editor = std::make_unique<application::SketchEditorSession>(
        session, document::ModelDocument::initialBoxId(), plane);
    editor->setTool(application::SketchDrawingTool::rectangle);
    require(editor->setPreview({1, 1}, {8, 9}) && editor->confirmPreview().has_value(),
            "valid preview");
    desktop::SketchSolverGateTestAccess::install(controller, std::move(editor));
    auto& active = desktop::SketchSolverGateTestAccess::editor(controller);
    window.resize(1000, 800);
    window.show();
    QApplication::processEvents();
    QPushButton* accept = nullptr;
    for (auto* button : panel->findChildren<QPushButton*>())
        if (button->objectName() == "toolAcceptButton")
            accept = button;
    // The solver state has its own line (user request 2026-10-05).
    auto* guidance = panel->findChild<QLabel*>(QStringLiteral("toolStatus"));
    require(accept && guidance, "productive panel controls missing");
    const auto originalShape = session.currentShape();
    for (int dof : {-1, 0, 4}) {
        application::SketchSolverGateTestAccess::set(active, features::SketchSolveStatus::failed,
                                                     dof);
        desktop::SketchSolverGateTestAccess::sync(controller);
        require(!active.canAccept() && !accept->isEnabled() &&
                    guidance->text().contains("Löser fehlgeschlagen"),
                "failed solve shown as valid or accepted");
        const bool rejected =
            active.tryAccept().reason() == application::RejectionReason::invalidInput;
        require(rejected && active.isActive() && !session.isModified() && !session.canUndo() &&
                    session.currentShape().IsSame(originalShape),
                "failed accept changed or closed document/editor");
        accept->click();
        require(controller.isActive(), "disabled OK closed the editor");
    }
    const QString directory = qEnvironmentVariable("VOELUND_SOLVER_GATE_ARTIFACT_DIR");
    if (!directory.isEmpty()) {
        require(QDir().mkpath(directory), "capture directory");
        QApplication::processEvents();
        require(window.grab().save(directory + "/solver-failed.png"), "failed state capture");
    }
    application::SketchSolverGateTestAccess::set(active, features::SketchSolveStatus::conflicting,
                                                 0);
    desktop::SketchSolverGateTestAccess::sync(controller);
    require(!accept->isEnabled() && guidance->text().contains("Widerspruch"),
            "conflict caption");
    require(active.selectElement(document::SketchElementId{1}) && active.setSelectedLineLength(9),
            "repair failed");
    desktop::SketchSolverGateTestAccess::sync(controller);
    require(active.canAccept() && accept->isEnabled() && guidance->text().contains("Freiheitsgrade"),
            "repair did not enable OK");
    if (!directory.isEmpty()) {
        QApplication::processEvents();
        require(window.grab().save(directory + "/solver-repaired.png"), "repair capture");
    }
    accept->click();
    require(!controller.isActive() && session.features().size() == 3 && session.isModified(),
            "successful OK did not commit");
    require((session.undo().status() == voelund::application::CommandStatus::committed) &&
                session.features().size() == 2 && !session.isModified(),
            "repair undo");
}
void existingSketchErrorOffer() {
    application::CadSession session;
    session.newDocument();
    const auto box = voelund::test::requireCommittedFeature(session.createBoxFeature());
    const document::SketchPlane top{0, 0, 40, 0, 0, 1, 1, 0, 0};
    static_cast<void>(voelund::test::requireCommittedFeature(
        session.createSketchExtrusion(box, top, {1, 1, 5, 4}, 5)));
    const auto sketch = session.features()[1].id;
    const auto original = session.sketchParameters(sketch);
    require(session.selectFeature(sketch), "select existing sketch");
    QMainWindow window;
    auto* panel = new desktop::ToolPanelHost(&window);
    viewport::CadViewport viewport;
    desktop::SketchEditorToolController controller(
        session, viewport, *panel, [] {}, [](const QString&) {});
    require(controller.beginSelectedSketch(), "begin existing sketch");
    auto& editor = desktop::SketchSolverGateTestAccess::editor(controller);
    require(editor.selectElement(document::SketchElementId{1}) && editor.removeSelected(),
            "remove rectangle");
    editor.setTool(application::SketchDrawingTool::line);
    require(editor.setPreview({1, 1}, {4, 3}) && editor.confirmPreview().has_value(),
            "open-profile draft");
    desktop::SketchSolverGateTestAccess::sync(controller);
    window.resize(900, 700);
    window.show();
    QApplication::processEvents();
    QPushButton* accept = nullptr;
    for (auto* button : panel->findChildren<QPushButton*>())
        if (button->objectName() == "toolAcceptButton")
            accept = button;
    require(accept && accept->isEnabled(), "solver-valid draft must reach evaluation");
    accept->click();
    auto* keep = panel->findChild<QPushButton*>(QStringLiteral("keepSketchErrorsButton"));
    require(keep && keep->isVisible() && controller.isActive() &&
                session.sketchParameters(sketch) == original,
            "missing sketch error offer");
    require(editor.selectElement(editor.elements().front().id) && editor.translateSelected(1, 0),
            "change pending draft");
    keep->click();
    require(controller.isActive() && !keep->isVisible() &&
                session.sketchParameters(sketch) == original,
            "old offer committed a changed draft");
    accept->click();
    require(keep->isVisible(), "renewed sketch offer");
    const auto directory = qEnvironmentVariable("VOELUND_SOLVER_GATE_ARTIFACT_DIR");
    if (!directory.isEmpty()) {
        require(QDir().mkpath(directory), "capture directory");
        QApplication::processEvents();
        require(window.grab().save(directory + "/sketch-error-offer.png"), "capture sketch offer");
    }
    keep->click();
    require(!controller.isActive() && !session.sceneStatus().isCurrent() &&
                session.sketchParameters(sketch) != original,
            "explicit sketch error was not committed");
    require((session.undo().status() == voelund::application::CommandStatus::committed) &&
                session.sceneStatus().isCurrent() && session.sketchParameters(sketch) == original,
            "sketch error undo");
}
} // namespace
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    voelund::test::isolateSettings();
    try {
        run();
        existingSketchErrorOffer();
        std::cout << "Sketch solver acceptance and panel tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}