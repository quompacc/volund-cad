// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "IsolatedSettings.hpp"
#include "UiDriver.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <cmath>
#include <iostream>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// W15 (user report 2026-10-25, as in FreeCAD and Fusion): a selected sketch shows its closed
// areas lightly filled, the one under the pointer lights up, and a click picks the area to
// extrude - here the wall between two nested rectangles, then the inside as well.
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
    for (int i = 0; i < 5; ++i)
        QApplication::processEvents();
}
QPoint at(viewport::CadViewport& viewport, double x, double y) {
    return viewport::ViewportTestAccess::project(viewport, x, y, 0);
}
void hover(viewport::CadViewport& viewport, double x, double y) {
    test::mouse(viewport, QEvent::MouseMove, at(viewport, x, y), Qt::NoButton, Qt::NoButton);
    test::render(viewport);
    settle();
}
void click(viewport::CadViewport& viewport, double x, double y,
           Qt::KeyboardModifiers modifiers = {}) {
    const QPoint point = at(viewport, x, y);
    test::mouse(viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
    test::mouse(viewport, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton,
                modifiers);
    test::mouse(viewport, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton,
                modifiers);
    test::render(viewport);
    settle();
}
void key(QWidget& target, int code, const QString& text, Qt::KeyboardModifiers modifiers = {}) {
    QKeyEvent press(QEvent::KeyPress, code, modifiers, text);
    QApplication::sendEvent(&target, &press);
    settle();
}
QTreeWidgetItem* sketchRow(QTreeWidget& tree) {
    for (QTreeWidgetItemIterator it(&tree); *it != nullptr; ++it)
        if ((*it)->text(0).startsWith(QStringLiteral("Skizze")))
            return *it;
    return nullptr;
}
double volume(const application::CadSession& session) {
    GProp_GProps properties;
    BRepGProp::VolumeProperties(session.currentShape(), properties);
    return properties.Mass();
}

void nestedRectangles(QWidget& window, viewport::CadViewport& viewport) {
    viewport.setCameraView(viewport::CameraView::top);
    test::render(viewport);
    click(viewport, 30, 30);
    command(window, "solid.create.sketch").trigger();
    settle();
    command(window, "sketch.tool.rectangle").trigger();
    click(viewport, -20, -10);
    click(viewport, 20, 10);
    command(window, "sketch.tool.rectangle").trigger();
    click(viewport, -15, -5);
    click(viewport, 15, 5);
    key(viewport, Qt::Key_Return, {}, Qt::ControlModifier);
    test::render(viewport);
}

// Hovering lights the area under the pointer; the wall and the inside are different areas.
void hovering(viewport::CadViewport& viewport) {
    require(viewport.displayedSketchRegionCount() == 2,
            "The selected sketch shows its two areas: " +
                std::to_string(viewport.displayedSketchRegionCount()));
    hover(viewport, 0, 0);
    const auto inside = viewport.hoveredSketchRegion();
    hover(viewport, -17.5, 0);
    const auto wall = viewport.hoveredSketchRegion();
    require(inside && wall && *inside != *wall, "Inside and wall light up apart");
    hover(viewport, 40, 40);
    require(!viewport.hoveredSketchRegion(), "Outside the sketch nothing lights up");
}

void run() {
    application::CadSession session;
    session.newDocument();
    desktop::MainWindow window(session);
    window.resize(1300, 860);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    require(viewport != nullptr, "viewport");
    test::render(*viewport);
    nestedRectangles(window, *viewport);
    auto* tree = window.findChild<QTreeWidget*>(QStringLiteral("modelTree"));
    require(tree != nullptr && sketchRow(*tree) != nullptr, "tree");
    emit tree->itemClicked(sketchRow(*tree), 0);
    settle();
    test::render(*viewport);
    hovering(*viewport);
    click(*viewport, -17.5, 0); // the wall
    require(session.selectedSketchProfiles().size() == 1, "A click picks the wall");
    hover(*viewport, -17.5, 0); // again over the chosen area: it stays chosen
    click(*viewport, -17.5, 0);
    require(session.selectedSketchProfiles().size() == 1, "A second click keeps the wall");
    command(window, "solid.create.extrude_profiles").trigger();
    settle();
    key(*QApplication::focusWidget(), Qt::Key_1, QStringLiteral("1"));
    key(*QApplication::focusWidget(), Qt::Key_0, QStringLiteral("0"));
    key(*QApplication::focusWidget(), Qt::Key_Return, {});
    require(std::abs(volume(session) - 5000.0) < 1.0,
            "The wall alone is extruded: " + std::to_string(volume(session)));
    std::cout << "PASS sketch regions: filled areas, hover, the wall picked and extruded\n";
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
