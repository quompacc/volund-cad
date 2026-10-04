#include "../../src/features/FeatureEvaluationDetail.hpp"
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QLabel>
#include <QPushButton>
#include <QStatusBar>
#include <QThread>
#include <Standard_ProgramError.hxx>
#include <iostream>
#include <new>
#include <stdexcept>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>
#include <voelund/viewport/CadViewport.hpp>

namespace {
using namespace voelund;
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
void finish(QDoubleSpinBox& input, double value) {
    input.setValue(value);
    require(QMetaObject::invokeMethod(&input, "editingFinished", Qt::DirectConnection),
            "editingFinished dispatch");
}
void run() {
    application::CadSession session;
    require(session.selectFeature(document::ModelDocument::initialCylinderId()), "select cylinder");
    const auto original = session.selectedCylinderParameters();
    const auto shape = session.currentShape();
    desktop::MainWindow window(session);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    require(viewport, "viewport missing");
    QElapsedTimer timer;
    timer.start();
    while (!viewport->isRenderingReady() && timer.elapsed() < 10000) {
        QApplication::processEvents();
        QThread::msleep(10);
    }
    require(viewport->isRenderingReady(), "visible viewport readiness");
    auto* moveZ = window.findChild<QDoubleSpinBox*>(QStringLiteral("cylinderTranslation2Input"));
    auto* radius = window.findChild<QDoubleSpinBox*>(QStringLiteral("cylinderRadiusInput"));
    require(moveZ && radius, "productive cylinder inputs");
    finish(*moveZ, 10);
    require(session.selectedCylinderParameters() == original && !session.isModified() &&
                !session.canUndo() && session.currentShape().IsSame(shape) &&
                moveZ->value() == original.placement.translationZ &&
                window.statusBar()->currentMessage().contains("abgelehnt"),
            "rejected parameter was published or input was not restored");
    QApplication::processEvents();
    auto* diagnostic = window.findChild<QLabel*>(QStringLiteral("cylinderCommandError"));
    require(diagnostic && diagnostic->isVisible() && diagnostic->text().contains("abgelehnt") &&
                diagnostic->text().contains("Feature"),
            "persistent rejection diagnostic");
    const auto directory = qEnvironmentVariable("VOELUND_PARAMETER_FAILURE_ARTIFACT_DIR");
    if (!directory.isEmpty()) {
        require(QDir().mkpath(directory), "capture directory");
        QApplication::processEvents();
        require(window.grab().save(directory + "/parameter-rejected.png"), "capture rejection");
    }
    QPushButton* keep = nullptr;
    for (auto* button :
         window.findChildren<QPushButton*>(QStringLiteral("keepEditableErrorsButton")))
        if (button->isVisible())
            keep = button;
    require(keep, "missing explicit error-commit action");
    finish(*moveZ, original.placement.translationZ);
    require(!keep->isVisible() && !session.canUndo() && !session.isModified(),
            "no-change input retained obsolete error offer");
    keep->click(); // A queued stale activation must also be harmless.
    require(!session.canUndo() && session.sceneStatus().isCurrent(),
            "obsolete handler still committed errors");
    finish(*moveZ, 10);
    require(keep->isVisible(), "fresh failed input did not renew the offer");
    keep->click();
    auto* geometryStatus = window.findChild<QLabel*>(QStringLiteral("geometryStatus"));
    require(!session.sceneStatus().isCurrent() &&
                session.selectedCylinderParameters().placement.translationZ == 10 &&
                session.displayShape().IsSame(shape) && geometryStatus &&
                geometryStatus->isVisible() &&
                geometryStatus->text().contains("Letzte gültige Geometrie"),
            "explicit error commit was not labelled stale");
    if (!directory.isEmpty()) {
        QApplication::processEvents();
        require(window.grab().save(directory + "/parameter-kept-stale.png"), "capture stale scene");
    }
    QAction* undo = nullptr;
    for (auto* action : window.findChildren<QAction*>())
        if (action->property("commandId").toString() == "history.undo")
            undo = action;
    require(undo && undo->isEnabled(), "undo action");
    finish(*moveZ, 0);
    require(session.sceneStatus().isCurrent() && !session.isModified() &&
                !geometryStatus->isVisible(),
            "productive repair did not restore current geometry");
    undo->trigger();
    require(!session.sceneStatus().isCurrent() && geometryStatus->isVisible() &&
                diagnostic->isVisible() && diagnostic->text().contains("mit Fehlern übernommen") &&
                moveZ->value() == 10,
            "undo repair lost typed error result or stale presentation");
    if (!directory.isEmpty()) {
        QApplication::processEvents();
        require(window.grab().save(directory + "/parameter-undo-repair.png"),
                "capture undo repair");
    }
    undo->trigger();
    require(session.sceneStatus().isCurrent() && !session.isModified() &&
                !geometryStatus->isVisible(),
            "undo did not clear stale warning");
    const auto previousHook = features::detail::beforeFeatureEvaluation;
    features::detail::beforeFeatureEvaluation = [](document::FeatureId) { throw std::bad_alloc{}; };
    finish(*radius, 9);
    features::detail::beforeFeatureEvaluation = previousHook;
    require(!session.isModified() && radius->value() == original.radius &&
                window.statusBar()->currentMessage().contains("Vorgang fehlgeschlagen"),
            "allocation exception escaped Qt callback or changed parameters");
    features::detail::beforeFeatureEvaluation = [](document::FeatureId) {
        throw Standard_ProgramError("injected OCCT program error");
    };
    finish(*radius, 9);
    features::detail::beforeFeatureEvaluation = previousHook;
    require(!session.isModified() && radius->value() == original.radius &&
                window.statusBar()->currentMessage().contains("injected OCCT program error"),
            "OCCT exception escaped Qt callback or changed parameters");
    finish(*radius, 9);
    require(session.isModified() && session.selectedCylinderParameters().radius == 9 &&
                (session.undo().status() == voelund::application::CommandStatus::committed) &&
                !session.isModified() && session.selectedCylinderParameters() == original,
            "repair and single undo");
    finish(*moveZ, 10);
    require(keep->isVisible(), "new error offer");
    session.newDocument(); // Deliberate external document replacement before the queued choice.
    keep->click();
    require(session.features().empty() && session.sceneStatus().isCurrent() && !session.canUndo() &&
                !session.isModified(),
            "stale error offer changed the replacement document");
}
} // namespace
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    try {
        run();
        std::cout << "Parameter rejection and Qt exception containment tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        features::detail::beforeFeatureEvaluation = nullptr;
        std::cerr << error.what() << '\n';
        return 1;
    }
}
