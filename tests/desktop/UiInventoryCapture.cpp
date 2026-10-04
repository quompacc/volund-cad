#include "../BoundSketchFixture.hpp"
#include "ViewportLifecycleSupport.hpp"
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QTreeWidget>
#include <iostream>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// UX01 inventory helper (not a test): captures the current product shell in its main states
// into VOELUND_SMOKE_ARTIFACT_DIR for the redesign discussion.
namespace {
using namespace voelund;
QString directory;
void capture(QWidget& window, const char* name) {
    QApplication::processEvents();
    window.grab().save(directory + "/ux01-" + name + ".png");
    std::cout << "captured " << name << '\n';
}
void click(viewport::CadViewport& viewport, double x, double y, double z) {
    const QPoint point = viewport::ViewportTestAccess::project(viewport, x, y, z);
    test::mouse(viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
    test::mouse(viewport, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
    test::mouse(viewport, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
    test::render(viewport);
}
viewport::CadViewport& open(desktop::MainWindow& window) {
    window.resize(1400, 860);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    test::render(*viewport);
    viewport->setCameraView(viewport::CameraView::isometric);
    test::render(*viewport);
    return *viewport;
}
void emptyStart() {
    application::CadSession session;
    session.newDocument(); // The product starts empty since U01.
    desktop::MainWindow window(session);
    open(window);
    capture(window, "00-empty-start");
}
// U03: the new shell in both themes with a ring body (rectangle with hole on XY).
void themes() {
    application::CadSession session;
    session.newDocument();
    document::SketchParameters sketch{
        document::datumSketchPlane(document::DatumPlane::xy),
        document::SketchElements{
            {{document::SketchElementId{1}, document::SketchRectangle{-20, -15, 20, 15}},
             {document::SketchElementId{2}, document::SketchCircle{{0, 0}, 6}}},
            3}};
    sketch.datum = document::DatumPlane::xy;
    const auto sketchId = *session.createDatumSketchFeature(sketch).feature();
    static_cast<void>(session.selectSketchProfileByElement(document::SketchElementId{1}, false));
    static_cast<void>(
        session.createSketchProfileExtrusion(sketchId, session.selectedSketchProfiles(), 12));
    desktop::MainWindow window(session);
    auto& viewport = open(window);
    capture(window, "u03-light");
    QAction* theme = nullptr;
    for (auto* action : window.findChildren<QAction*>())
        if (action->property("commandId").toString() == QStringLiteral("view.theme"))
            theme = action;
    theme->trigger();
    test::render(viewport);
    capture(window, "u03-dark");
    theme->trigger(); // Leave the stored preference on light.
}
QAction* command(QWidget& window, const char* id) {
    for (auto* action : window.findChildren<QAction*>())
        if (action->property("commandId").toString() == QLatin1String(id))
            return action;
    throw std::runtime_error(std::string("missing command ") + id);
}
// U04: tool rail, context bar and command search.
void commandSurfaces() {
    application::CadSession session;
    session.newDocument();
    desktop::MainWindow window(session);
    auto& viewport = open(window);
    capture(window, "u04-empty");
    command(window, "solid.create.box")->trigger();
    test::render(viewport);
    capture(window, "u04-box-selected");
    command(window, "solid.modify.transform")->trigger();
    QApplication::processEvents();
    test::render(viewport);
    capture(window, "u04-transform");
    command(window, "solid.modify.transform")->trigger();
    command(window, "view.command_search")->trigger();
    QApplication::processEvents();
    if (auto* search = window.findChild<QWidget*>(QStringLiteral("commandSearch"))) {
        search->grab().save(directory + "/ux01-u04-search.png");
        search->hide();
    }
}
void defaultDocument() {
    application::CadSession session;
    desktop::MainWindow window(session);
    auto& viewport = open(window);
    capture(window, "01-start");
    click(viewport, 4, 4, 20);
    capture(window, "02-face-selected");
    window.findChild<QAction*>(QStringLiteral("sketchEditorAction"))->trigger();
    test::render(viewport);
    capture(window, "03-sketch-editor");
}
void existingSketch() {
    application::CadSession session;
    session.newDocument();
    const auto box = *session.createBoxFeature().feature();
    const document::SketchPlane plane{0, 0, 20, 0, 0, 1, 1, 0, 0};
    const auto sketch =
        *session
             .createSketchFeature(
                 box, {plane,
                       {{{document::SketchElementId{1}, document::SketchRectangle{5, 5, 30, 25}},
                         {document::SketchElementId{2}, document::SketchCircle{{17.5, 15}, 4}}},
                        3}})
             .feature();
    test::bindTop(session, sketch);
    desktop::MainWindow window(session);
    auto& viewport = open(window);
    auto* tree = window.findChild<QTreeWidget*>();
    tree->setCurrentItem(tree->topLevelItem(0)->child(1));
    test::render(viewport);
    capture(window, "04-sketch-selected");
    click(viewport, 5, 15, 20);
    capture(window, "05-profile-picked");
    for (auto* action : window.findChildren<QAction*>())
        if (action->property("commandId").toString() ==
                QStringLiteral("solid.create.extrude_profiles") &&
            action->isEnabled()) {
            action->trigger();
            break;
        }
    test::render(viewport);
    capture(window, "06-extrude-panel");
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("VoelundCapture"));
    directory = qEnvironmentVariable("VOELUND_SMOKE_ARTIFACT_DIR");
    if (directory.isEmpty())
        return 2;
    QDir().mkpath(directory);
    try {
        emptyStart();
        themes();
        commandSurfaces();
        defaultDocument();
        existingSketch();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
