// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "../../src/desktop/ModelTreePanel.hpp"
#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QPushButton>
#include <QStatusBar>
#include <QTreeWidget>
#include <iostream>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// V01b/U06: base planes in the viewport of the real main window. Prominent in an empty
// document, pickable, start a sketch on the picked plane, hidden while sketching. Bound to
// the origin in the model tree: hidden automatically once a body exists, shown again by its
// eye, and a body behind a shown plane wins the click. Captures are written for review.
namespace {
using namespace voelund;
using test::require;
void capture(viewport::CadViewport& viewport, QWidget& window, const char* name) {
    const QString directory = qEnvironmentVariable("VOELUND_SMOKE_ARTIFACT_DIR");
    if (directory.isEmpty())
        return;
    QDir().mkpath(directory);
    viewport.grabFramebuffer().save(directory + "/base-planes-" + name + "-viewport.png");
    window.grab().save(directory + "/base-planes-" + name + "-window.png");
}
void toggleOrigin(desktop::MainWindow& window) {
    auto* tree = window.findChild<QTreeWidget*>();
    require(tree != nullptr && tree->topLevelItemCount() > 0, "Model tree");
    QTreeWidgetItem* origin = tree->topLevelItem(0);
    require(origin->data(0, desktop::ModelTreePanel::kindRole).toInt() ==
                static_cast<int>(desktop::ModelTreePanel::Kind::origin),
            "The tree starts with the document origin");
    emit tree->itemClicked(origin, 1);
}
// A shown plane in front of a body must not steal the click from the body (U06).
void bodyWinsOverPlane() {
    application::CadSession session;
    session.newDocument();
    static_cast<void>(session.createBoxFeature());
    auto box = session.selectedBoxParameters();
    box.placement.translationY = 10; // XZ (y = 0) now lies in front of the box from the front.
    require(session.editSelectedBoxParameters(box).status() ==
                application::CommandStatus::committed,
            "Move the box behind XZ");
    desktop::MainWindow window(session);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    test::render(*viewport);
    toggleOrigin(window);
    viewport->setCameraView(viewport::CameraView::front);
    viewport->fitView();
    test::render(*viewport);
    require(viewport->displayedBasePlaneCount() == 3, "Planes shown in front of the box");
    const QPoint point = viewport::ViewportTestAccess::project(*viewport, 10, 10, 20);
    test::mouse(*viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
    test::mouse(*viewport, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
    test::mouse(*viewport, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
    test::render(*viewport);
    capture(*viewport, window, "body-behind-plane");
    require(!window.statusBar()->currentMessage().startsWith(QStringLiteral("XZ")) &&
                session.selectedFeature().has_value(),
            "The body behind the plane wins the click");
}
void basePlanes() {
    application::CadSession session;
    session.newDocument();
    desktop::MainWindow window(session);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    require(viewport != nullptr, "viewport");
    test::render(*viewport);
    viewport->setCameraView(viewport::CameraView::isometric);
    test::render(*viewport);
    require(viewport->displayedBasePlaneCount() == 3, "Empty document shows three base planes");
    capture(*viewport, window, "empty");

    auto* sketch = window.findChild<QAction*>(QStringLiteral("sketchEditorAction"));
    require(sketch != nullptr && !sketch->isEnabled(), "Sketch needs a selected plane or face");
    const QPoint xy =
        viewport::ViewportTestAccess::project(*viewport, 20, -20, 0); // In front of XZ/YZ.
    test::mouse(*viewport, QEvent::MouseMove, xy, Qt::NoButton, Qt::NoButton);
    test::render(*viewport);
    capture(*viewport, window, "hover-xy");
    test::mouse(*viewport, QEvent::MouseButtonPress, xy, Qt::LeftButton, Qt::LeftButton);
    test::mouse(*viewport, QEvent::MouseButtonRelease, xy, Qt::LeftButton, Qt::NoButton);
    test::render(*viewport);
    require(window.statusBar()->currentMessage().startsWith(QStringLiteral("XY")),
            "Click picks the XY plane");
    require(sketch->isEnabled(), "Picking the XY plane enables Sketch");
    capture(*viewport, window, "selected-xy");

    // A click into empty space deselects the plane again (user report 2026-10-05).
    const QPoint empty(viewport->width() - 20, viewport->height() - 20);
    test::mouse(*viewport, QEvent::MouseMove, empty, Qt::NoButton, Qt::NoButton);
    test::mouse(*viewport, QEvent::MouseButtonPress, empty, Qt::LeftButton, Qt::LeftButton);
    test::mouse(*viewport, QEvent::MouseButtonRelease, empty, Qt::LeftButton, Qt::NoButton);
    test::render(*viewport);
    require(!sketch->isEnabled(), "A click into empty space deselects the plane");
    test::mouse(*viewport, QEvent::MouseMove, xy, Qt::NoButton, Qt::NoButton);
    test::mouse(*viewport, QEvent::MouseButtonPress, xy, Qt::LeftButton, Qt::LeftButton);
    test::mouse(*viewport, QEvent::MouseButtonRelease, xy, Qt::LeftButton, Qt::NoButton);
    test::render(*viewport);
    require(sketch->isEnabled(), "The plane can be picked again");

    sketch->trigger();
    test::render(*viewport);
    require(viewport->displayedBasePlaneCount() == 0, "Base planes are hidden while sketching");
    capture(*viewport, window, "sketch-on-xy");
    // S13: Escape steps back but never leaves the editor; Cancel does (nothing drawn).
    window.findChild<QWidget*>(QStringLiteral("activeToolPanel"))
        ->findChild<QPushButton*>(QStringLiteral("toolCancelButton"))
        ->click();
    QApplication::processEvents();
    test::render(*viewport);

    auto* box = window.findChild<QAction*>(QStringLiteral("addBoxAction"));
    require(box != nullptr && box->isEnabled(), "Add box");
    box->trigger();
    test::render(*viewport);
    require(viewport->displayedBasePlaneCount() == 0,
            "The origin hides automatically once a body exists");
    toggleOrigin(window);
    test::render(*viewport);
    require(viewport->displayedBasePlaneCount() == 3, "The origin eye shows the planes again");
    viewport->setCameraView(viewport::CameraView::isometric);
    viewport->fitView();
    test::render(*viewport);
    capture(*viewport, window, "with-body");
    std::cout << "PASS base planes: empty, hover, pick, sketch start, hidden, faint beside body\n";
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    try {
        basePlanes();
        bodyWinsOverPlane();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
