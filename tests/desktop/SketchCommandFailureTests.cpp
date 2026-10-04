#include "../../src/desktop/SketchExtrusionToolController.hpp"
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

namespace {
using namespace voelund;
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
QPushButton* button(QWidget& widget, const QString& text) {
    for (auto* item : widget.findChildren<QPushButton*>())
        if (item->objectName() == text)
            return item;
    throw std::runtime_error("Productive command button missing");
}
void run() {
    application::CadSession session;
    session.newDocument();
    const auto box = voelund::test::requireCommittedFeature(session.createBoxFeature());
    document::SketchParameters sketch{
        {0, 0, 40, 0, 0, 1, 1, 0, 0},
        {{{document::SketchElementId{1}, document::SketchRectangle{100, 100, 105, 105}}}, 2}};
    const auto sketchId =
        voelund::test::requireCommittedFeature(session.createSketchFeature(box, sketch));
    require(session.selectSketchProfileByElement(document::SketchElementId{1}, false),
            "select profile");
    const auto shape = session.currentShape();
    QMainWindow window;
    auto* panel = new desktop::ToolPanelHost(&window);
    window.setCentralWidget(new QLabel(QStringLiteral("Extrusion rejection regression")));
    unsigned sceneChanges = 0;
    QString status;
    desktop::SketchExtrusionToolController controller(
        session, *panel, [&] { ++sceneChanges; },
        [&](const QString& message) { status = message; });
    controller.beginSelectedProfiles();
    window.resize(1000, 700);
    window.show();
    QApplication::processEvents();
    button(*panel, QStringLiteral("toolAcceptButton"))->click();
    auto* guidance = panel->findChild<QLabel*>(QStringLiteral("toolGuidance"));
    require(controller.isActive() && session.features().size() == 2 && sceneChanges == 0 &&
                session.currentShape().IsSame(shape) && session.selectedFeature() == sketchId &&
                guidance && guidance->text().contains("abgelehnt") && status.contains("Feature"),
            "failed extrusion closed tool, published state, or hid diagnostic");
    const auto directory = qEnvironmentVariable("VOELUND_COMMAND_FAILURE_ARTIFACT_DIR");
    if (!directory.isEmpty()) {
        require(QDir().mkpath(directory), "capture directory");
        QApplication::processEvents();
        require(window.grab().save(directory + "/extrusion-rejected.png"), "capture rejection");
    }
    button(*panel, QStringLiteral("toolCancelButton"))->click();
    require(!controller.isActive() && session.features().size() == 2, "cancel rejected tool");
    // Repair the source fixture, then confirm through the same productive OK callback.
    sketch.elements.values.front().geometry = document::SketchRectangle{1, 1, 5, 5};
    require(session.editSketchParameters(sketchId, sketch).status() ==
                    application::CommandStatus::committed &&
                session.selectSketchProfileByElement(document::SketchElementId{1}, false),
            "repair source sketch");
    controller.beginSelectedProfiles();
    button(*panel, QStringLiteral("toolAcceptButton"))->click();
    require(!controller.isActive() && session.features().size() == 3 && sceneChanges == 1 &&
                status.contains("extrudiert"),
            "valid extrusion was not confirmed");
    require((session.undo().status() == voelund::application::CommandStatus::committed) &&
                session.features().size() == 2 &&
                (session.undo().status() == voelund::application::CommandStatus::committed) &&
                session.sketchParameters(sketchId).elements.values.front().geometry ==
                    document::SketchGeometry{document::SketchRectangle{100, 100, 105, 105}},
            "rejection added a hidden history entry");
}
} // namespace
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    try {
        run();
        std::cout << "Productive extrusion rejection/repair tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
