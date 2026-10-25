// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "IsolatedSettings.hpp"
#include "UiDriver.hpp"
#include "ViewportLifecycleSupport.hpp"

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

// User report 2026-10-24 (as in Fusion): the base planes take only part of the empty view and
// go once the first sketch exists; a sketch shows until a later step (an extrusion) uses it,
// then it hides itself, and its eye in the model tree brings it back.
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
void click(viewport::CadViewport& viewport, QPoint point) {
    test::mouse(viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
    test::mouse(viewport, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
    test::mouse(viewport, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
    test::render(viewport);
    settle();
}
void click(viewport::CadViewport& viewport, double x, double y) {
    click(viewport, viewport::ViewportTestAccess::project(viewport, x, y, 0));
}
void key(QWidget& target, int code, const QString& text, Qt::KeyboardModifiers modifiers = {}) {
    QKeyEvent press(QEvent::KeyPress, code, modifiers, text);
    QApplication::sendEvent(&target, &press);
    settle();
}
QTreeWidgetItem* row(QTreeWidget& tree, const char* text) {
    for (QTreeWidgetItemIterator it(&tree); *it != nullptr; ++it)
        if ((*it)->text(0) == QString::fromUtf8(text))
            return *it;
    return nullptr;
}
QTreeWidgetItem* sketchRow(QTreeWidget& tree) {
    for (QTreeWidgetItemIterator it(&tree); *it != nullptr; ++it)
        if ((*it)->text(0).startsWith(QStringLiteral("Skizze")))
            return *it;
    return nullptr;
}

// The planes are 100 mm wide; in the empty view they take well under the window's width, at the
// start and after every fit (user report 2026-10-25: Home, the view cube, F6).
void smallPlanes(viewport::CadViewport& viewport) {
    const auto small = [&](const std::string& when) {
        test::render(viewport);
        const QPoint left = viewport::ViewportTestAccess::project(viewport, -50, 0, 0);
        const QPoint right = viewport::ViewportTestAccess::project(viewport, 50, 0, 0);
        const double across = std::hypot(right.x() - left.x(), right.y() - left.y());
        require(viewport.displayedBasePlaneCount() == 3 && across < 0.6 * viewport.width(),
                when + ": the base planes take part of the empty view: " + std::to_string(across) +
                    " of " + std::to_string(viewport.width()) + " px");
    };
    small("At the start");
    viewport.fitView();
    small("After Home");
    viewport.setCameraView(viewport::CameraView::front);
    small("After a view cube side");
    viewport.setCameraView(viewport::CameraView::isometric);
    small("Back to isometric");
}

void firstSketch(QWidget& window, viewport::CadViewport& viewport) {
    viewport.setCameraView(viewport::CameraView::top);
    test::render(viewport);
    click(viewport, 30, 30);
    command(window, "solid.create.sketch").trigger();
    settle();
    command(window, "sketch.tool.rectangle").trigger();
    click(viewport, -20, -10);
    click(viewport, 20, 10);
    key(viewport, Qt::Key_Return, {}, Qt::ControlModifier);
    test::render(viewport);
    require(viewport.displayedBasePlaneCount() == 0, "The planes go with the first sketch");
}

// User report 2026-10-25: the tree says the planes are hidden; a plane's eye brings it alone.
void planeEyes(QTreeWidget& tree, viewport::CadViewport& viewport) {
    require(row(tree, "XY-Ebene") != nullptr &&
                !row(tree, "XY-Ebene")->data(1, Qt::UserRole).toBool(),
            "The tree shows the planes hidden");
    emit tree.itemClicked(row(tree, "XY-Ebene"), 1);
    settle();
    require(viewport.displayedBasePlaneCount() == 1 &&
                row(tree, "XY-Ebene")->data(1, Qt::UserRole).toBool() &&
                !row(tree, "XZ-Ebene")->data(1, Qt::UserRole).toBool(),
            "The XY eye shows that plane alone");
    emit tree.itemClicked(row(tree, "XY-Ebene"), 1);
    settle();
    require(sketchRow(tree) != nullptr && sketchRow(tree)->data(1, Qt::UserRole).toBool(),
            "The new sketch has an open eye in the tree");
}

void extrude(QWidget& window, viewport::CadViewport& viewport) {
    click(viewport, 5, 3); // the region
    command(window, "solid.create.extrude_profiles").trigger();
    settle();
    for (const char* digit : {"1", "0"})
        key(*QApplication::focusWidget(), Qt::Key_0 + (digit[0] - '0'), QString::fromLatin1(digit));
    key(*QApplication::focusWidget(), Qt::Key_Return, {});
}

// From below the hidden sketch takes no click; shown again it does; hidden again while selected
// it goes at once (user report 2026-10-25).
void hiddenSketch(application::CadSession& session, QTreeWidget& tree,
                  viewport::CadViewport& viewport) {
    viewport.setCameraView(viewport::CameraView::bottom);
    test::render(viewport);
    click(viewport, 5, 3);
    require(session.selectedFeatureDescriptor().kind != document::FeatureKind::sketch,
            "A hidden sketch takes no click");
    emit tree.itemClicked(sketchRow(tree), 1);
    settle();
    require(viewport.shownSketchCount() == 1 && sketchRow(tree)->data(1, Qt::UserRole).toBool(),
            "Its eye brings the used sketch back");
    click(viewport, 5, 3);
    require(session.selectedFeatureDescriptor().kind == document::FeatureKind::sketch &&
                !viewport::ViewportTestAccess::sketchPoints(viewport).empty(),
            "Shown again, its area is clicked from below and drawn");
    emit tree.itemClicked(sketchRow(tree), 1);
    settle();
    require(viewport::ViewportTestAccess::sketchPoints(viewport).empty() &&
                !sketchRow(tree)->data(1, Qt::UserRole).toBool(),
            "Hidden while selected, the sketch goes at once");
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
    smallPlanes(*viewport);
    firstSketch(window, *viewport);
    auto* tree = window.findChild<QTreeWidget*>(QStringLiteral("modelTree"));
    require(tree != nullptr, "tree");
    planeEyes(*tree, *viewport);
    extrude(window, *viewport);
    require(session.features().size() == 2, "Extruded");
    require(viewport->shownSketchCount() == 0 && !sketchRow(*tree)->data(1, Qt::UserRole).toBool(),
            "The extrusion uses the sketch: it hides itself");
    hiddenSketch(session, *tree, *viewport);
    std::cout << "PASS sketch visibility: small planes, gone after the first sketch, used "
                 "sketch hidden, eye in the tree\n";
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
