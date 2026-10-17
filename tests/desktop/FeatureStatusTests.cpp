#include "../BoundSketchFixture.hpp"
#include "../CommandResultAssertions.hpp"
#include "IsolatedSettings.hpp"
#include "ModelTreeLookup.hpp"
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QLabel>
#include <QPushButton>
#include <QStatusBar>
#include <QThread>
#include <QToolButton>
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
// U15: a blocked step names its cause in German, without the evaluation's English text.
bool blockedCard(const QWidget& card) {
    const auto text = [&card](const char* name) {
        const auto* label = card.findChild<QLabel*>(QLatin1String(name));
        return label != nullptr ? label->text() : QString{};
    };
    const QString title = text("featureErrorTitle");
    const QString message = text("featureErrorMessage");
    return title.contains(QStringLiteral("Blockiert")) &&
           message.contains(QStringLiteral("sobald")) &&
           !message.contains(QStringLiteral("Blocked"));
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
    require(tree && session.features().size() == 6, "model tree");
    const auto row = [&](std::size_t index) { return test::featureRow(*tree, session, index); };
    require(row(2)->text(0).contains("[Fehler]") && row(3)->text(0).contains("[blockiert]") &&
                row(4)->text(0).contains("[blockiert]") && row(4)->toolTip(0).contains("Blocked") &&
                !row(5)->text(0).contains("[blockiert]") && !row(5)->text(0).contains("[Fehler]"),
            "Model tree lost failure/block distinction");
    const auto directory = qEnvironmentVariable("VOELUND_FEATURE_STATUS_ARTIFACT_DIR");
    if (!directory.isEmpty()) {
        require(QDir().mkpath(directory), "capture directory");
        QApplication::processEvents();
        require(window.grab().save(directory + "/feature-failed-blocked.png"), "capture tree");
    }
    // U10: markers, the error card with its way out, and the stale banner.
    require(!row(2)->icon(0).isNull() && !row(3)->icon(0).isNull() && row(5)->icon(0).isNull(),
            "Tree markers for failed and blocked features");
    const auto steps = window.findChildren<QToolButton*>(QStringLiteral("timelineStep"));
    const auto stepState = [&](std::size_t index) {
        for (auto* step : steps)
            if (step->isVisible() &&
                step->property("featureId").toULongLong() == session.features()[index].id.value())
                return step->property("featureState").toString();
        return QString{};
    };
    require(stepState(2) == QStringLiteral("error") && stepState(3) == QStringLiteral("blocked") &&
                stepState(5).isEmpty(),
            "Timeline markers for failed and blocked features");
    auto* banner = window.findChild<QWidget*>(QStringLiteral("staleGeometryBanner"));
    require(banner && banner->isVisible(), "Stale geometry banner shown");
    auto* card = window.findChild<QWidget*>(QStringLiteral("featureErrorCard"));
    const auto label = [&](const char* name) {
        return card->findChild<QLabel*>(QLatin1String(name))->text();
    };
    const auto button = [&](const char* name) {
        return card->findChild<QPushButton*>(QLatin1String(name));
    };
    tree->setCurrentItem(row(3));
    QApplication::processEvents();
    require(card->isVisible() && blockedCard(*card) && button("featureErrorCause")->isVisible() &&
                !button("featureErrorEdit")->isVisible(),
            "A blocked feature names its cause");
    if (!directory.isEmpty())
        require(window.grab().save(directory + "/u10-blocked.png"), "capture blocked");
    button("featureErrorCause")->click();
    QApplication::processEvents();
    // The failed feature is the extrusion of the opened sketch; its properties are the editor,
    // so the card offers no extra button.
    require(session.selectedFeature() == session.features()[2].id && card->isVisible() &&
                label("featureErrorTitle").contains(QStringLiteral("Fehler")) &&
                !label("featureErrorMessage").isEmpty() &&
                !button("featureErrorCause")->isVisible(),
            "The cause is the failed extrusion with its message");
    if (!directory.isEmpty())
        require(window.grab().save(directory + "/u10-failed.png"), "capture failed");
    window.refreshAfterExternalChange();
    QApplication::processEvents();
    require(card->isVisible() && banner->isVisible() && !row(2)->icon(0).isNull(),
            "A refresh does not clear the error");
    tree->setCurrentItem(row(5));
    QApplication::processEvents();
    require(!card->isVisible(), "A current feature shows no error card");
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
    {
        // U10: after the real repair the banner and the markers are gone.
        desktop::MainWindow repaired(session);
        repaired.show();
        QApplication::processEvents();
        auto* banner = repaired.findChild<QWidget*>(QStringLiteral("staleGeometryBanner"));
        auto* tree = repaired.findChild<QTreeWidget*>();
        require(banner && !banner->isVisible() &&
                    test::featureRow(*tree, session, 2)->icon(0).isNull(),
                "Repair clears banner and markers");
    }
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
    require(tree && session.features().size() == 3, "legacy model tree");
    const auto row = [&](std::size_t index) { return test::featureRow(*tree, session, index); };
    require(row(1)->text(0).contains("[Fehler]") && row(2)->text(0).contains("[blockiert]") &&
                row(1)->toolTip(0).contains("MigrationNeedsRepair"),
            "Legacy plane error was not visible");
    QAction* edit = nullptr;
    for (auto* action : window.findChildren<QAction*>())
        if (action->property("commandId").toString() == "solid.modify.edit_sketch")
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
    voelund::test::isolateSettings();
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
