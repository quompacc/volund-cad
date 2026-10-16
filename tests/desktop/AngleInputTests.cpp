#include "IsolatedSettings.hpp"
#include "../../src/desktop/FeaturePropertiesPanel.hpp"
#include "../../src/desktop/SketchEditorToolController.hpp"
#include "../../src/desktop/ToolPanelHost.hpp"
#include "../../src/viewport/RotationInput.hpp"
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QInputDialog>
#include <QPushButton>
#include <QThread>
#include <QTimer>
#include <iostream>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>
#include <voelund/viewport/CadViewport.hpp>

namespace {
using namespace voelund;
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
void near(double actual, double expected, const char* message) {
    require(std::abs(actual - expected) < 1e-10, message);
}
void snap() {
    const gp_Dir axes[] = {gp_Dir(1, 0, 0), gp_Dir(0, 1, 0), gp_Dir(0, 0, 1)};
    for (int axis = 0; axis < 3; ++axis) {
        for (double degrees : {-92.6, -92.4, 92.4, 92.6}) {
            const gp_Pnt pivot(7, -3, 9);
            gp_Trsf raw;
            raw.SetRotation(gp_Ax1(pivot, axes[axis]),
                            kernel::toRadians(kernel::Degrees(degrees)).value());
            const auto snapped = viewport::detail::snapRotation(raw, pivot, kernel::Degrees(5));
            const auto delta = viewport::detail::rotationDeltaDegrees(snapped, axis);
            require(delta.has_value(), "snapped rotation lost axis");
            near(delta->value(), std::round(degrees / 5) * 5, "signed 5-degree snap changed units");
            near(pivot.Transformed(snapped).Distance(pivot), 0, "snap moved pivot");
            near(viewport::detail::rotationDeltaDegrees(raw, axis)->value(), degrees,
                 "unsnapped rotation changed units");
        }
    }
}
void panel() {
    desktop::FeaturePropertiesPanel panel;
    for (double degrees : {-720.0, -90.0, 90.0, 450.0, 720.0}) {
        const auto angle = kernel::toRadians(kernel::Degrees(degrees));
        const kernel::RigidTransform placement{1, 2, 3, angle, angle, angle};
        document::BoxParameters box{{20, 30, 40}, placement};
        panel.showBox(box);
        require(panel.boxParameters() == box, "box properties roundtrip changed internal radians");
        panel.previewRotation(placement, 1, -5);
        near(kernel::toDegrees(panel.boxParameters().placement.rotationY).value(), degrees - 5,
             "box preview mixed degrees and radians");
        document::CylinderParameters cylinder{4, 10, placement,
                                              document::CylinderResultMode::separate};
        panel.showCylinder(cylinder, {});
        require(panel.cylinderParameters() == cylinder,
                "cylinder properties roundtrip changed internal radians");
        panel.previewRotation(placement, 2, 5);
        near(kernel::toDegrees(panel.cylinderParameters().placement.rotationZ).value(), degrees + 5,
             "cylinder preview wrapped complete turns");
    }
    require(features::sketchConstraintLabel(document::SketchConstraintKind::angle,
                                            -std::numbers::pi / 3)
                    .find("-60.000") != std::string::npos,
            "angle annotation no longer displays signed degrees");
}
void window() {
    application::CadSession session;
    session.newDocument();
    require(session.createBoxFeature().status() == application::CommandStatus::committed,
            "create box");
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
    auto* input = window.findChild<QDoubleSpinBox*>("boxRotation2Input");
    require(input, "box degree input missing");
    QAction* undo = nullptr;
    QAction* redo = nullptr;
    for (auto* action : window.findChildren<QAction*>()) {
        if (action->property("commandId").toString() == "history.undo")
            undo = action;
        if (action->property("commandId").toString() == "history.redo")
            redo = action;
    }
    require(undo && redo, "history actions missing");
    for (double degrees : {-90.0, 450.0, 720.0}) {
        const auto previous = session.selectedFeaturePlacement();
        input->setValue(degrees);
        require(QMetaObject::invokeMethod(input, "editingFinished", Qt::DirectConnection),
                "degree input dispatch");
        const auto changed = session.selectedFeaturePlacement();
        near(changed.rotationZ.value(), kernel::toRadians(kernel::Degrees(degrees)).value(),
             "MainWindow degree input did not commit radians");
        require(session.sceneStatus().isCurrent(), "rotation lost current geometry");
        undo->trigger();
        require(session.selectedFeaturePlacement() == previous, "rotation undo lost placement");
        near(input->value(), kernel::toDegrees(previous.rotationZ).value(),
             "undo displayed radians");
        redo->trigger();
        require(session.selectedFeaturePlacement() == changed, "rotation redo lost turns");
        near(input->value(), degrees, "redo displayed radians");
        if (degrees == 450) {
            const auto directory = qEnvironmentVariable("VOELUND_ANGLE_ARTIFACT_DIR");
            if (!directory.isEmpty()) {
                require(QDir().mkpath(directory), "capture directory");
                QApplication::processEvents();
                require(window.grab().save(directory + "/rotation-450-degrees.png"),
                        "capture rotation");
            }
        }
    }
}
void dimensionDialog() {
    using namespace document;
    application::CadSession session;
    session.newDocument();
    require(session.createBoxFeature().status() == application::CommandStatus::committed,
            "dialog support");
    const auto box = *session.selectedFeature();
    const SketchGeometryReference first{SketchElementId{1}, SketchPointPosition::start};
    const SketchGeometryReference second{SketchElementId{2}, SketchPointPosition::start};
    const SketchParameters sketch{
        {0, 0, 20, 0, 0, 1, 1, 0, 0},
        {{{SketchElementId{1}, SketchLine{{1, 10}, {11, 10}}},
          {SketchElementId{2}, SketchLine{{10, 10}, {15, 10 - std::sqrt(75.0)}}}},
         3},
        {{{SketchConstraintId{1}, SketchConstraintKind::horizontal, first, std::nullopt},
          {SketchConstraintId{2}, SketchConstraintKind::angle, first, second,
           kernel::Radians(-std::numbers::pi / 3)}},
         3}};
    require(session.createSketchFeature(box, sketch).status() ==
                application::CommandStatus::committed,
            "dialog sketch");
    const auto id = *session.selectedFeature();
    QMainWindow window;
    auto* host = new desktop::ToolPanelHost(&window);
    viewport::CadViewport viewport;
    desktop::SketchEditorToolController controller(
        session, viewport, *host, [] {}, [](const QString&) {});
    require(controller.beginSelectedSketch(), "begin angle sketch");
    window.resize(1000, 800);
    window.show();
    QApplication::processEvents();
    bool displayedDegrees = false;
    bool captured = true;
    QTimer::singleShot(0, [&] {
        auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
        if (!dialog)
            return;
        displayedDegrees =
            std::abs(dialog->textValue().toDouble() + 60) < 1e-10 && dialog->labelText().contains("Grad");
        dialog->setTextValue(QStringLiteral("60"));
        const auto directory = qEnvironmentVariable("VOELUND_ANGLE_ARTIFACT_DIR");
        if (!directory.isEmpty())
            captured = QDir().mkpath(directory) &&
                       dialog->grab().save(directory + "/angle-dimension-degrees.png");
        dialog->accept();
    });
    controller.editConstraint(2);
    require(displayedDegrees && captured, "dimension dialog displayed radians or capture failed");
    require(session.sketchParameters(id) == sketch, "dimension dialog committed without OK");
    QPushButton* accept = nullptr;
    for (auto* button : host->findChildren<QPushButton*>())
        if (button->objectName() == "toolAcceptButton")
            accept = button;
    require(accept && accept->isEnabled(), "angle sketch OK disabled");
    accept->click();
    require(!controller.isActive(), "angle sketch not committed");
    const auto changed = session.sketchParameters(id);
    near(changed.constraints.values[1].angleValue.value(), std::numbers::pi / 3,
         "dialog did not commit radians");
    const auto& a = std::get<SketchLine>(changed.elements.values[0].geometry);
    const auto& b = std::get<SketchLine>(changed.elements.values[1].geometry);
    const double ax = a.end.x - a.start.x, ay = a.end.y - a.start.y;
    const double bx = b.end.x - b.start.x, by = b.end.y - b.start.y;
    require(std::abs(std::atan2(ax * by - ay * bx, ax * bx + ay * by) - std::numbers::pi / 3) <
                1e-7,
            "dialog solved wrong directed angle");
    require(session.undo().status() == application::CommandStatus::committed &&
                session.sketchParameters(id) == sketch,
            "dialog undo lost original angle");
    require(session.redo().status() == application::CommandStatus::committed &&
                session.sketchParameters(id) == changed,
            "dialog redo lost solved angle");
}
} // namespace
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    voelund::test::isolateSettings();
    try {
        snap();
        panel();
        window();
        dimensionDialog();
        std::cout << "PASS degree inputs, typed placements, signed snap, annotation, undo/redo\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
