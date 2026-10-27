// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <QApplication>
#include <QStatusBar>
#include <TopAbs_ShapeEnum.hxx>
#include <iostream>
#include <stdexcept>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// W07 (as the pick list in Fusion): a click again at the same place takes the next candidate
// under the pointer - on a box's top front edge first the edge, then a face beside it, then
// another face; a click elsewhere in between starts again with the edge.
namespace {
using namespace voelund;
void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
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
std::string message(desktop::MainWindow& window) {
    return window.statusBar()->currentMessage().toStdString();
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
    require(session.createBoxFeature({20, 20, 10}).feature().has_value(), "Box");
    window.refreshAfterExternalChange();
    viewport->setCameraView(viewport::CameraView::isometric);
    test::render(*viewport);
    using Access = viewport::ViewportTestAccess;
    const QPoint edge = Access::project(*viewport, 10, 0, 10);
    click(*viewport, edge);
    require(message(window).starts_with("1 Kante"), "First the edge: " + message(window));
    click(*viewport, edge);
    const auto first = Access::selectedShape(*viewport);
    require(!first.IsNull() && first.ShapeType() == TopAbs_FACE,
            "Again: a face behind the edge (" + message(window) + ")");
    click(*viewport, edge);
    const auto second = Access::selectedShape(*viewport);
    require(!second.IsNull() && !second.IsSame(first), "Again: the next candidate");
    click(*viewport, QPoint(20, 20)); // empty
    click(*viewport, edge);
    require(message(window).starts_with("1 Kante"),
            "Elsewhere in between: the edge again: " + message(window));
    std::cout << "PASS click cycle flow: a click again at the same place takes the next\n";
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
