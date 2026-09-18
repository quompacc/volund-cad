#include "../BoundSketchFixture.hpp"
#include "../CommandResultAssertions.hpp"
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QStatusBar>
#include <QThread>
#include <QTreeWidget>
#include <fstream>
#include <iostream>
#include <random>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>
#include <voelund/viewport/CadViewport.hpp>

namespace {
using namespace voelund;
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
void checkStates(const application::CadSession& session) {
    const auto states = session.featureStates();
    require(states.size() == 6 && states[0].status == application::FeatureRuntimeStatus::current &&
                states[1].status == application::FeatureRuntimeStatus::current &&
                states[2].status == application::FeatureRuntimeStatus::error &&
                states[3].status == application::FeatureRuntimeStatus::blocked &&
                states[4].status == application::FeatureRuntimeStatus::blocked &&
                states[5].status == application::FeatureRuntimeStatus::current &&
                !states[2].message.empty() &&
                states[4].message.find("Blocked") != std::string::npos,
            "Failed and blocked projections were conflated");
}
void checkReport(const application::CommandResult& result) {
    const auto& states = result.evaluation().features;
    require(result.status() == application::CommandStatus::committedWithErrors &&
                states.size() == 6 && states[2].status == features::EvaluationStatus::failed &&
                states[3].status == features::EvaluationStatus::blockedByDependency &&
                states[4].status == features::EvaluationStatus::blockedByDependency &&
                states[5].status == features::EvaluationStatus::valid,
            "History/command report lost blocked status");
}
void inspectWindow(application::CadSession& session) {
    desktop::MainWindow window(session);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    require(viewport, "viewport");
    QElapsedTimer timer;
    timer.start();
    while (!viewport->isRenderingReady() && timer.elapsed() < 10000) {
        QApplication::processEvents();
        QThread::msleep(10);
    }
    require(viewport->isRenderingReady(), "visible viewport readiness");
    auto* tree = window.findChild<QTreeWidget*>();
    require(tree && tree->topLevelItemCount() == 1, "model tree");
    auto* body = tree->topLevelItem(0);
    require(body->childCount() == 6 && body->child(2)->text(0).contains("[error]") &&
                body->child(3)->text(0).contains("[blocked]") &&
                body->child(4)->text(0).contains("[blocked]") &&
                body->child(4)->toolTip(0).contains("Blocked") &&
                !body->child(5)->text(0).contains("[blocked]") &&
                !body->child(5)->text(0).contains("[error]"),
            "Model tree lost failure/block distinction");
    const auto directory = qEnvironmentVariable("VOELUND_FEATURE_STATUS_ARTIFACT_DIR");
    if (!directory.isEmpty()) {
        require(QDir().mkpath(directory), "capture directory");
        QApplication::processEvents();
        require(window.grab().save(directory + "/feature-failed-blocked.png"), "capture tree");
    }
    tree->setCurrentItem(body->child(5));
    QApplication::processEvents();
    require(session.selectedFeature() == document::FeatureId{6} &&
                session.canTransformFeature(document::FeatureId{6}),
            "Independent tree selection/editing was disabled");
    if (!directory.isEmpty())
        require(window.grab().save(directory + "/feature-independent-current.png"),
                "capture independent body");
}
void run(const std::filesystem::path& directory) {
    struct Fixture {
        std::filesystem::path path;
        ~Fixture() {
            std::error_code error;
            std::filesystem::remove(path, error);
        }
    } fixture{directory / ("d07f-" + std::to_string(std::random_device{}()) + ".volund")};
    application::CadSession session;
    session.newDocument();
    const auto box = test::requireCommittedFeature(session.createBoxFeature());
    const document::SketchPlane firstPlane{0, 0, 40, 0, 0, 1, 1, 0, 0};
    const auto extrusion = test::requireCommittedFeature(
        session.createSketchExtrusion(box, firstPlane, {2, 3, 8, 9}, 10));
    test::requireCommittedFeature(session.createSketchExtrusion(
        extrusion, document::SketchPlane{0, 0, 50, 0, 0, 1, 1, 0, 0}, {3, 4, 7, 8}, 10));
    test::requireCommittedFeature(session.createBoxFeature());
    auto independent = session.selectedBoxParameters();
    independent.placement.translationX = 60;
    require(session.editSelectedBoxParameters(independent).status() ==
                application::CommandStatus::committed,
            "Independent body placement");
    const auto sketchId = session.features()[1].id;
    test::bindTop(session, sketchId);
    const auto original = session.sketchParameters(sketchId);
    auto broken = original;
    broken.elements.values.front().geometry = document::SketchLine{{2, 3}, {8, 9}};
    checkReport(session.editSketchParameters(sketchId, broken,
                                             application::ErrorDisposition::keepEditableErrors));
    checkStates(session);
    session.saveDocument(fixture.path);
    application::CadSession loaded;
    loaded.loadDocument(fixture.path);
    checkStates(loaded);
    require(loaded.displayShape().IsNull(), "load reused stale geometry");
    require(session.editSketchParameters(sketchId, original).status() ==
                application::CommandStatus::committed,
            "repair");
    checkReport(session.undo());
    checkStates(session);
    inspectWindow(session);
    require(session.redo().status() == application::CommandStatus::committed &&
                session.sceneStatus().isCurrent(),
            "redo repair");
    for (const auto& state : session.featureStates())
        require(state.status == application::FeatureRuntimeStatus::current && state.message.empty(),
                "repair retained error status");
}
void inspectLegacyPlane(const std::filesystem::path& directory) {
    const auto path = directory / "d10b-desktop-legacy.volund";
    {
        std::ofstream output(path, std::ios::binary);
        output << "VOLUND 1\nNEXT_ID 4\nFEATURES 3\n"
                  "BOX 1 \"Support\" 20 20 30 0 0 0 0 0 0\n"
                  "SKETCH 2 \"Legacy plane\" 1 0 0 30 0 0 2 3 0 0 1 2 6 8\n"
                  "EXTRUSION 3 \"Extrusion\" 2 5\n";
    }
    application::CadSession session;
    session.loadDocument(path);
    require(session.selectFeature(document::FeatureId{2}), "select legacy sketch");
    desktop::MainWindow window(session);
    window.resize(1400, 900);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    require(viewport, "legacy viewport");
    QElapsedTimer timer;
    timer.start();
    while (!viewport->isRenderingReady() && timer.elapsed() < 10000) {
        QApplication::processEvents();
        QThread::msleep(10);
    }
    require(viewport->isRenderingReady(), "legacy viewport readiness");
    auto* tree = window.findChild<QTreeWidget*>();
    require(tree && tree->topLevelItemCount() == 1, "legacy model tree");
    auto* body = tree->topLevelItem(0);
    require(body->childCount() == 3 && body->child(1)->text(0).contains("[error]") &&
                body->child(2)->text(0).contains("[blocked]") &&
                body->child(1)->toolTip(0).contains("MigrationNeedsRepair"),
            "Legacy plane error was not visible");
    QAction* edit = nullptr;
    for (auto* action : window.findChildren<QAction*>())
        if (action->text() == "Edit Sketch")
            edit = action;
    require(edit && edit->isEnabled(), "edit sketch action");
    const auto stamp = session.sceneStatus().document;
    edit->trigger();
    QApplication::processEvents();
    require(window.statusBar()->currentMessage().contains("MigrationNeedsRepair") &&
                session.sceneStatus().document == stamp && !session.isModified() &&
                !session.sceneStatus().isCurrent(),
            "Editing legacy plane silently repaired data or lost diagnostic");
    const auto capture = qEnvironmentVariable("VOELUND_FEATURE_STATUS_ARTIFACT_DIR");
    if (!capture.isEmpty()) {
        require(QDir().mkpath(capture), "legacy capture directory");
        require(window.grab().save(capture + "/legacy-plane-migration.png"), "legacy screenshot");
    }
}
} // namespace
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    try {
        require(argc == 2, "fixture directory argument");
        run(argv[1]);
        inspectLegacyPlane(argv[1]);
        std::cout << "Failed/blocked publication, persistence, history and tree passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
