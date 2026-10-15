#include "IsolatedSettings.hpp"
#include "UiDriver.hpp"
#include "ViewportLifecycleSupport.hpp"
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <cmath>
#include <iostream>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// M12 (A-MEASURE-01, user decision 2026-10-15): a box 20 x 20 x 10. "Messen" (I): a click on
// a vertical edge shows its length 10 and the body volume 4000; a click on the top face adds its
// area, the distance 0 and 90 degrees; a third click starts over with the top face alone, and the
// top view measures the same area; "Werte kopieren" copies the full values; closing the panel
// leaves the document untouched.
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
QWidget* panel(QWidget& window) {
    auto* found = window.findChild<QWidget*>(QStringLiteral("activeToolPanel"));
    return found != nullptr && found->isVisible() ? found : nullptr;
}
QPushButton& accept(QWidget& window) {
    auto* found = panel(window)->findChild<QPushButton*>(QStringLiteral("toolAcceptButton"));
    require(found != nullptr, "OK button");
    return *found;
}
void click(viewport::CadViewport& viewport, double x, double y, double z, QPoint nudge = {}) {
    test::render(viewport);
    const QPoint point = viewport::ViewportTestAccess::project(viewport, x, y, z) + nudge;
    test::mouse(viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
    test::mouse(viewport, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
    test::mouse(viewport, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
    test::render(viewport);
    settle();
}

QString guidance(QWidget& window) {
    for (auto* label : panel(window)->findChildren<QLabel*>())
        if (label->objectName() == QLatin1String("toolGuidance"))
            return label->text();
    throw std::runtime_error("Panel guidance missing");
}
QString value(double number, int decimals) { return QLocale().toString(number, 'f', decimals); }

void run() {
    application::CadSession session;
    desktop::MainWindow window(session);
    window.resize(1300, 860);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    require(viewport != nullptr, "viewport");
    test::render(*viewport);
    session.newDocument();
    require(session.createBoxFeature({20, 20, 10}).feature().has_value(), "Box");
    window.refreshAfterExternalChange();
    viewport->setCameraView(viewport::CameraView::isometric);
    test::render(*viewport);
    const auto revision = session.sceneStatus().document;
    const bool modified = session.isModified();

    QAction& measure = command(window, "solid.inspect.measure");
    require(measure.isEnabled() && measure.shortcut().toString() == QStringLiteral("I"),
            "Messen offered, shortcut I");
    measure.trigger();
    settle();
    require(panel(window) != nullptr && guidance(window).contains(QStringLiteral("Klicke")),
            "The panel asks for a pick");

    // On a silhouette edge a click right on it may land on its face; like the user (and the
    // fillet test), the click falls just outside the body there.
    const QPoint middle = viewport::ViewportTestAccess::project(*viewport, 10, 10, 5);
    const QPoint corner = viewport::ViewportTestAccess::project(*viewport, 20, 20, 5);
    const QPointF away(corner - middle);
    const double length = std::max(1.0, std::hypot(away.x(), away.y()));
    click(*viewport, 20, 20, 5, QPoint(qRound(2 * away.x() / length), qRound(2 * away.y() / length)));
    const QString edge = guidance(window);
    require(edge.contains(QStringLiteral("Länge ") + value(10, 3) + QStringLiteral(" mm")) &&
                edge.contains(QStringLiteral("Körpervolumen ") + value(4000, 3)),
            "A vertical edge: length 10 and the body volume");
    click(*viewport, 10, 10, 10);
    const QString pair = guidance(window);
    require(pair.contains(QStringLiteral("Fläche ") + value(400, 3)) &&
                pair.contains(QStringLiteral("Abstand ") + value(0, 3)) &&
                pair.contains(QStringLiteral("Winkel ") + value(90, 2)),
            "The top face adds area 400, distance 0 and 90 degrees");

    click(*viewport, 10, 10, 10);
    const QString again = guidance(window);
    require(again.startsWith(QStringLiteral("1. ")) && !again.contains(QStringLiteral("Abstand")),
            "A third pick starts over");
    viewport->setCameraView(viewport::CameraView::top);
    test::render(*viewport);
    for (auto* candidate : panel(window)->findChildren<QPushButton*>())
        if (candidate->toolTip() == QStringLiteral("Neu beginnen"))
            candidate->click();
    settle();
    click(*viewport, 10, 10, 10);
    require(guidance(window) == again, "The top view measures the same");

    test::activate(window); // Wayland hands the clipboard to the active window only
    QApplication::clipboard()->clear();
    bool copied = false;
    for (auto* candidate : panel(window)->findChildren<QPushButton*>())
        if (candidate->toolTip() == QStringLiteral("Werte kopieren")) {
            candidate->click();
            copied = true;
        }
    settle();
    require(copied && QApplication::clipboard()->text().contains(QLocale().toString(400.0, 'g', 15)),
            "Werte kopieren copies the full values");

    accept(window).click();
    settle();
    require(panel(window) == nullptr && session.sceneStatus().document == revision &&
                session.isModified() == modified,
            "Measuring leaves the document untouched");
    std::cout << "PASS M12 measure panel: edge length and volume, face area, distance, angle, "
                 "start over, same in the top view, copy, no document change" << std::endl;
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    try {
        run();
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
    return 0;
}
