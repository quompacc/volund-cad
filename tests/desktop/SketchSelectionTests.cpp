// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QStatusBar>
#include <iostream>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// S13 (user decision 2026-10-09): a box dragged left to right selects elements fully inside
// (window), right to left elements it touches (crossing); whole elements, curves count by
// their curve; Ctrl adds; an empty box clears. An element wins over the label on it; a double
// click on a relation selects it and Delete removes it.
namespace {
using namespace voelund;
void require(bool condition, const std::string& message) {
    test::require(condition, message.c_str());
}

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
QPoint at(viewport::CadViewport& viewport, double x, double y) {
    return viewport::ViewportTestAccess::project(viewport, x, y, 0);
}
void send(viewport::CadViewport& viewport, QEvent::Type type, QPoint point,
          Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QMouseEvent event(type, QPointF(point), QPointF(viewport.mapToGlobal(point)), Qt::LeftButton,
                      buttons, modifiers);
    QApplication::sendEvent(&viewport, &event);
}
void click(viewport::CadViewport& viewport, double x, double y) {
    send(viewport, QEvent::MouseMove, at(viewport, x, y), Qt::NoButton);
    send(viewport, QEvent::MouseButtonPress, at(viewport, x, y), Qt::LeftButton);
    send(viewport, QEvent::MouseButtonRelease, at(viewport, x, y), Qt::NoButton);
    test::render(viewport);
    settle();
}
void box(viewport::CadViewport& viewport, double x0, double y0, double x1, double y1,
         Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    send(viewport, QEvent::MouseMove, at(viewport, x0, y0), Qt::NoButton);
    send(viewport, QEvent::MouseButtonPress, at(viewport, x0, y0), Qt::LeftButton, modifiers);
    send(viewport, QEvent::MouseMove, at(viewport, x1, y1), Qt::LeftButton, modifiers);
    send(viewport, QEvent::MouseButtonRelease, at(viewport, x1, y1), Qt::NoButton, modifiers);
    test::render(viewport);
    settle();
}
// Elements drawn in the selection color.
int selectedCount(viewport::CadViewport& viewport) {
    int count = 0;
    for (std::uint64_t id = 1; id <= 12; ++id)
        if (const auto color = viewport::ViewportTestAccess::sketchElementColor(viewport, id))
            if (color->Red() > 0.9 && color->Green() > 0.5 && color->Green() < 0.8 &&
                color->Blue() < 0.3)
                ++count;
    return count;
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
    command(window, "sketch.tool.rectangle").trigger();
    click(*viewport, -20, -10);
    click(*viewport, 20, 10);
    command(window, "sketch.tool.circle").trigger();
    // Every box starts far from any curve, so the press never picks an element.
    click(*viewport, 60, 0);
    click(*viewport, 75, 0);
    command(window, "sketch.tool.select").trigger();
    settle();

    box(*viewport, -26, -16, 26, 16);
    require(selectedCount(*viewport) == 4, "Window: the four sides, not the circle (" +
                                               std::to_string(selectedCount(*viewport)) + ")");
    box(*viewport, 56, -4, 64, 4);
    require(selectedCount(*viewport) == 0,
            "A window around the circle's center alone takes nothing and clears");
    box(*viewport, 80, 6, 70, -6);
    require(selectedCount(*viewport) == 1, "Crossing: a box touching the circle's curve");
    box(*viewport, -26, -16, 26, 16, Qt::ControlModifier);
    require(selectedCount(*viewport) == 5, "Ctrl adds the sides");
    box(*viewport, 90, 30, 100, 40);
    require(selectedCount(*viewport) == 0, "An empty box clears the selection");

    // The bottom side's [H] label sits 12 px right of its middle (U11), over the side; a click
    // where both are selects the side.
    const QPoint middle = at(*viewport, 0, -10) + QPoint(20, 0);
    send(*viewport, QEvent::MouseMove, middle, Qt::NoButton);
    send(*viewport, QEvent::MouseButtonPress, middle, Qt::LeftButton);
    send(*viewport, QEvent::MouseButtonRelease, middle, Qt::NoButton);
    test::render(*viewport);
    settle();
    require(selectedCount(*viewport) == 1 && status(window).startsWith(QStringLiteral("Skizze")),
            "An element wins over its label, got " + status(window).toStdString());
    send(*viewport, QEvent::MouseButtonPress, middle, Qt::LeftButton);
    send(*viewport, QEvent::MouseButtonRelease, middle, Qt::NoButton);
    send(*viewport, QEvent::MouseButtonDblClick, middle, Qt::LeftButton);
    send(*viewport, QEvent::MouseButtonRelease, middle, Qt::NoButton);
    settle();
    require(status(window).startsWith(QStringLiteral("Bedingung gewählt")),
            "A double click on a relation selects it, got " + status(window).toStdString());
    const auto before = session.features().size();
    QKeyEvent remove(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
    QApplication::sendEvent(viewport, &remove);
    settle();
    require(status(window) == QStringLiteral("Skizzenbedingung entfernt") &&
                session.features().size() == before,
            "Delete removes the selected relation, got " + status(window).toStdString());
    std::cout << "PASS sketch selection: window, crossing, Ctrl, labels, Delete\n";
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
