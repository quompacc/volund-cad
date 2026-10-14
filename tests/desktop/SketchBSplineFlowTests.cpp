#include "IsolatedSettings.hpp"
#include "UiDriver.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QPushButton>
#include <QStatusBar>
#include <cmath>
#include <iostream>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// S21 (user decision 2026-10-14): the B-spline tool (shortcut B) in a sketch on XY. Three clicks
// and Enter are refused (four control points at least); then four clicks and Enter make one
// B-spline; a second one right after with five clicks; OK stores two B-splines.
namespace {
using namespace voelund;
using test::require;

QAction& command(QWidget& window, const char* id) {
    for (auto* action : window.findChildren<QAction*>())
        if (action->property("commandId").toString() == QLatin1String(id))
            return *action;
    throw std::runtime_error(std::string("Command is missing: ") + id);
}
void settle() {
    for (int i = 0; i < 3; ++i)
        QApplication::processEvents();
}
void click(viewport::CadViewport& viewport, double x, double y) {
    const QPoint point = viewport::ViewportTestAccess::project(viewport, x, y, 0);
    test::mouse(viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
    settle();
    test::mouse(viewport, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
    test::mouse(viewport, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
    test::render(viewport);
    settle();
}
void enter(QWidget& window, viewport::CadViewport& viewport) {
    test::activate(window); // window shortcuts need the active window
    QKeyEvent press(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QApplication::sendEvent(&viewport, &press);
    settle();
    test::render(viewport);
}
QString status(QWidget& window) { return window.findChild<QStatusBar*>()->currentMessage(); }

void run() {
    application::CadSession session;
    session.newDocument();
    desktop::MainWindow window(session);
    window.resize(1300, 860);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    require(viewport != nullptr, "viewport");
    test::render(*viewport);
    viewport->setCameraView(viewport::CameraView::top);
    test::render(*viewport);
    click(*viewport, 30, 30);
    command(window, "solid.create.sketch").trigger();
    settle();
    test::render(*viewport);

    QAction& tool = command(window, "sketch.tool.bspline");
    require(tool.shortcut() == QKeySequence(QStringLiteral("B")), "Shortcut B");
    tool.trigger();
    settle();
    click(*viewport, 0, 0);
    click(*viewport, 5, 10);
    click(*viewport, 15, 10);
    enter(window, *viewport);
    require(viewport->displayedSketchElementCount() == 0 &&
                status(window).contains(QStringLiteral("4 bis 64")),
            "Three control points are refused");

    click(*viewport, 0, 0);
    click(*viewport, 5, 10);
    click(*viewport, 15, 10);
    click(*viewport, 20, 0);
    enter(window, *viewport);
    require(viewport->displayedSketchElementCount() == 1, "Four control points make a B-spline");
    click(*viewport, 0, -20);
    click(*viewport, 5, -10);
    click(*viewport, 10, -25);
    click(*viewport, 15, -10);
    click(*viewport, 20, -20);
    enter(window, *viewport);
    require(viewport->displayedSketchElementCount() == 2, "A second one right after");

    auto* panel = window.findChild<QWidget*>(QStringLiteral("activeToolPanel"));
    panel->findChild<QPushButton*>(QStringLiteral("toolAcceptButton"))->click();
    settle();
    int splines = 0;
    std::size_t poles = 0;
    for (const auto& element :
         session.sketchParameters(session.features().back().id).elements.values)
        if (const auto* spline = std::get_if<document::SketchBSpline>(&element.geometry)) {
            ++splines;
            poles += spline->poles.size();
        }
    require(splines == 2 && poles == 9, "OK stores two B-splines with 4 and 5 control points");
    std::cout << "PASS S21 B-spline flow: refused, four, five, OK\n";
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
