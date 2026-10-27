// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <QAction>
#include <QApplication>
#include <QLabel>
#include <QStatusBar>
#include <TopAbs_ShapeEnum.hxx>
#include <iostream>
#include <stdexcept>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// W07 (as Fusion's selection filter): without edges a click on an edge takes the face there,
// without faces and edges it takes the whole body, without planes a base plane takes no click;
// an open tool takes what it needs (the fillet panel still edges). The filter is a setting.
namespace {
using namespace voelund;
using Access = viewport::ViewportTestAccess;
void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
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
void click(viewport::CadViewport& viewport, QPoint at) {
    test::mouse(viewport, QEvent::MouseMove, at, Qt::NoButton, Qt::NoButton);
    test::mouse(viewport, QEvent::MouseButtonPress, at, Qt::LeftButton, Qt::LeftButton);
    test::mouse(viewport, QEvent::MouseButtonRelease, at, Qt::LeftButton, Qt::NoButton);
    settle();
}
QString guidance(QWidget& window) {
    auto* panel = window.findChild<QWidget*>(QStringLiteral("activeToolPanel"));
    auto* label =
        panel != nullptr ? panel->findChild<QLabel*>(QStringLiteral("toolGuidance")) : nullptr;
    return label != nullptr ? label->text() : QString();
}
void toggle(QWidget& window, const char* id, bool on) {
    auto& action = command(window, id);
    if (action.isChecked() != on)
        action.trigger();
    settle();
}

void planes(desktop::MainWindow& window, viewport::CadViewport& viewport) {
    // An empty document: only the base planes; the top view looks at the XY plane.
    viewport.setCameraView(viewport::CameraView::top);
    test::render(viewport);
    const QPoint inside = Access::project(viewport, 10, 10, 0);
    toggle(window, "select.filter.planes", false);
    click(viewport, inside);
    require(!Access::selectedShapeType(viewport).has_value(), "Without planes no plane is picked");
    toggle(window, "select.filter.planes", true);
    click(viewport, inside);
    require(Access::selectedShapeType(viewport) == TopAbs_FACE, "With planes the plane is picked");
}

void run() {
    application::CadSession session;
    desktop::MainWindow window(session);
    window.resize(1300, 860);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    require(viewport != nullptr, "viewport");
    test::render(*viewport);
    session.newDocument();
    window.refreshAfterExternalChange();
    planes(window, *viewport);
    const auto box = *session.createBoxFeature({20, 20, 10}).feature();
    window.refreshAfterExternalChange();
    viewport->setCameraView(viewport::CameraView::isometric);
    test::render(*viewport);
    const QPoint edge = Access::project(*viewport, 10, 0, 10);
    toggle(window, "select.filter.edges", false);
    click(*viewport, edge);
    require(Access::selectedShapeType(*viewport) == TopAbs_FACE, "Without edges: the face");
    toggle(window, "select.filter.faces", false);
    click(*viewport, QPoint(0, 0));
    click(*viewport, edge);
    const auto whole = Access::selectedShapeType(*viewport);
    require(whole.has_value() && *whole != TopAbs_FACE && *whole != TopAbs_EDGE,
            "Without faces and edges: the whole body");
    // The fillet panel takes edges whatever the filter says.
    require(session.selectFeature(box), "Box selected");
    window.refreshAfterExternalChange();
    command(window, "solid.modify.fillet").trigger();
    settle();
    test::render(*viewport);
    click(*viewport, edge + QPoint(0, -2));
    require(guidance(window).startsWith(QStringLiteral("1 Kante")),
            "The fillet panel takes the edge: " + guidance(window).toStdString());
    std::cout << "PASS selection filter flow: planes, edges, faces, bodies, tools unaffected\n";
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
