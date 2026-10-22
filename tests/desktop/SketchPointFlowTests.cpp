// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"
#include <QAction>
#include <QApplication>
#include <QPushButton>
#include <cmath>
#include <iostream>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// S15 (user decision 2026-10-14): in a sketch on XY the point tool (shortcut O) places a point
// with each click: on the origin (snapped), then two free ones; the select tool picks a point;
// OK stores three construction points, the first one on the origin.
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

void run() {
    application::CadSession session;
    session.newDocument();
    desktop::MainWindow window(session);
    window.resize(1300, 860);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    require(viewport != nullptr, "viewport");
    test::render(*viewport);
    viewport->setCameraView(viewport::CameraView::isometric);
    test::render(*viewport);
    click(*viewport, 30, 30);
    command(window, "solid.create.sketch").trigger();
    settle();
    test::render(*viewport);

    QAction& pointTool = command(window, "sketch.tool.point");
    require(pointTool.shortcut() == QKeySequence(QStringLiteral("O")), "Shortcut O");
    pointTool.trigger();
    settle();
    click(*viewport, 0.3, 0.2); // snaps to the origin
    click(*viewport, 20, 10);
    click(*viewport, 35, 5);
    require(viewport->displayedSketchElementCount() == 3, "Three points are drawn");

    command(window, "sketch.tool.select").trigger();
    settle();
    click(*viewport, 20, 10);
    auto* panel = window.findChild<QWidget*>(QStringLiteral("activeToolPanel"));
    panel->findChild<QPushButton*>(QStringLiteral("toolAcceptButton"))->click();
    settle();
    require(session.features().size() == 1, "The sketch is stored");
    const auto& elements = session.sketchParameters(session.features()[0].id).elements.values;
    require(elements.size() == 3, "Three elements");
    for (const auto& element : elements)
        require(std::holds_alternative<document::SketchPointGeometry>(element.geometry) &&
                    element.construction,
                "Each is a construction point");
    const auto& first = std::get<document::SketchPointGeometry>(elements.front().geometry);
    require(std::abs(first.position.x) < 1e-9 && std::abs(first.position.y) < 1e-9,
            "The first point lies on the origin");
    std::cout << "PASS S15 sketch point flow: three clicks, origin snap, select, OK\n";
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
