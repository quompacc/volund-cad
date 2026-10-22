// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "../../src/desktop/ViewCube.hpp"
#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <QApplication>
#include <QMouseEvent>
#include <cmath>
#include <iostream>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// View cube (user report 2026-10-05): it turns with the camera; faces, edges and corners pick
// all 26 directions; dragging orbits like grabbing the cube; the arrows roll by 90°; the house
// returns to the isometric view.
namespace {
using namespace voelund;
using test::require;

std::array<double, 3> normalized(double x, double y, double z) {
    const double length = std::sqrt(x * x + y * y + z * z);
    return {x / length, y / length, z / length};
}
bool along(const double* actual, const std::array<double, 3>& expected) {
    return std::abs(actual[0] - expected[0]) < 1e-6 && std::abs(actual[1] - expected[1]) < 1e-6 &&
           std::abs(actual[2] - expected[2]) < 1e-6;
}
double dot(const double* a, const double* b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

void send(QWidget& widget, QEvent::Type type, const QPointF& point, Qt::MouseButtons buttons) {
    QMouseEvent event(type, point, widget.mapToGlobal(point), Qt::LeftButton, buttons,
                      Qt::NoModifier);
    QApplication::sendEvent(&widget, &event);
    QApplication::processEvents();
}
void click(QWidget& widget, const QPointF& point) {
    send(widget, QEvent::MouseMove, point, Qt::NoButton);
    send(widget, QEvent::MouseButtonPress, point, Qt::LeftButton);
    send(widget, QEvent::MouseButtonRelease, point, Qt::NoButton);
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
    auto* widget = window.findChild<QWidget*>(QStringLiteral("viewCube"));
    require(widget != nullptr, "view cube");
    auto& cube = *static_cast<desktop::ViewCube*>(widget);
    const auto home = [&] {
        click(cube, cube.homeCenter());
        test::render(*viewport);
    };
    home();
    require(along(viewport->cameraFrame().toEye, normalized(1, -1, 1)),
            "The house shows the isometric view from front, right and top");

    // Isometric: top, front and right are visible, with their edges and the shared corner.
    const auto pick = [&](const desktop::ViewCube::Direction& direction, const char* what) {
        const auto center = cube.cellCenter(direction);
        require(center.has_value(), (std::string("Visible cell: ") + what).c_str());
        click(cube, *center);
        test::render(*viewport);
        require(along(viewport->cameraFrame().toEye,
                      normalized(direction[0], direction[1], direction[2])),
                (std::string("Looks along ") + what).c_str());
    };
    pick({0, 0, 1}, "top face");
    home();
    pick({0, -1, 0}, "front face");
    home();
    pick({1, -1, 1}, "front-right-top corner");
    home();
    pick({0, -1, 1}, "front-top edge");
    home();
    pick({1, 0, 0}, "right face");
    // From the right, the back is now visible; it was hidden in the isometric view.
    pick({1, 1, 0}, "right-back edge");

    // Rolling turns the up direction by 90° and keeps the view direction.
    home();
    auto before = viewport->cameraFrame();
    click(cube, cube.rollRightCenter());
    auto after = viewport->cameraFrame();
    require(std::abs(dot(before.up, after.up)) < 1e-6 && along(after.toEye, normalized(1, -1, 1)),
            "Roll right turns the view by 90°");
    click(cube, cube.rollLeftCenter());
    require(dot(viewport->cameraFrame().up, before.up) > 0.999999, "Roll left turns it back");
    // Counter-clockwise: what pointed right on screen points up afterwards.
    const double screenRight[3]{before.up[1] * before.toEye[2] - before.up[2] * before.toEye[1],
                                before.up[2] * before.toEye[0] - before.up[0] * before.toEye[2],
                                before.up[0] * before.toEye[1] - before.up[1] * before.toEye[0]};
    click(cube, cube.rollLeftCenter());
    require(dot(viewport->cameraFrame().up, screenRight) > 0.999999,
            "Roll left turns the picture counter-clockwise");
    click(cube, cube.rollRightCenter());

    // Dragging the cube to the right turns its front to the right: the eye moves left.
    home();
    before = viewport->cameraFrame();
    const double right[3]{before.up[1] * before.toEye[2] - before.up[2] * before.toEye[1],
                          before.up[2] * before.toEye[0] - before.up[0] * before.toEye[2],
                          before.up[0] * before.toEye[1] - before.up[1] * before.toEye[0]};
    const auto start = *cube.cellCenter({0, -1, 0});
    send(cube, QEvent::MouseButtonPress, start, Qt::LeftButton);
    for (int step = 1; step <= 6; ++step)
        send(cube, QEvent::MouseMove, start + QPointF(step * 5, 0), Qt::LeftButton);
    send(cube, QEvent::MouseButtonRelease, start + QPointF(30, 0), Qt::NoButton);
    after = viewport->cameraFrame();
    require(!along(after.toEye, normalized(1, -1, 1)) && dot(after.toEye, right) < 0.0,
            "Dragging orbits the view like grabbing the cube");

    click(cube, cube.fitCenter());
    require(along(viewport->cameraFrame().toEye, {after.toEye[0], after.toEye[1], after.toEye[2]}),
            "Fit keeps the direction");
    std::cout << "PASS view cube\n";
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
