// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <BRepAdaptor_Curve.hxx>
#include <QAction>
#include <QApplication>
#include <QLabel>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// W07 (as Fusion): in the fillet panel a double click on an edge also takes the edges joined to
// it without a kink - on a box with one rounded upright edge the top edge, the round and the
// next top edge. A box in the panel takes edges too: a window the edges wholly inside, a
// crossing box those it touches.
namespace {
using namespace voelund;
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
QString guidance(QWidget& window) {
    auto* panel = window.findChild<QWidget*>(QStringLiteral("activeToolPanel"));
    auto* label =
        panel != nullptr ? panel->findChild<QLabel*>(QStringLiteral("toolGuidance")) : nullptr;
    return label != nullptr ? label->text() : QString();
}
bool near(double a, double b) { return std::abs(a - b) < 1e-6; }

// Rounds the upright edge at x = 20, y = 0 of a 20 x 20 x 10 box; returns the fillet.
document::FeatureId roundedBox(application::CadSession& session) {
    session.newDocument();
    const auto box = *session.createBoxFeature({20, 20, 10}).feature();
    for (TopExp_Explorer explorer(session.validBodies().front().shape, TopAbs_EDGE);
         explorer.More(); explorer.Next()) {
        const BRepAdaptor_Curve curve(TopoDS::Edge(explorer.Current()));
        const gp_Pnt middle = curve.Value((curve.FirstParameter() + curve.LastParameter()) / 2);
        if (!near(middle.X(), 20) || !near(middle.Y(), 0))
            continue;
        const auto named = session.filletEdge(box, explorer.Current());
        require(named.has_value(), "Upright edge named");
        const auto created = session.createFillet(box, std::span(&*named, 1), 3);
        require(created.feature().has_value(), "Rounded: " + created.diagnostic());
        return *created.feature();
    }
    throw std::runtime_error("Upright edge missing");
}

void run() {
    application::CadSession session;
    desktop::MainWindow window(session);
    window.resize(1300, 860);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    require(viewport != nullptr, "viewport");
    test::render(*viewport);
    const auto fillet = roundedBox(session);
    window.refreshAfterExternalChange();
    viewport->setCameraView(viewport::CameraView::isometric);
    require(session.selectFeature(fillet), "Fillet selected");
    window.refreshAfterExternalChange();
    command(window, "solid.modify.fillet").trigger();
    settle();
    test::render(*viewport);
    // The front top edge, a little outside the body as the user clicks a silhouette edge.
    const QPoint at = viewport::ViewportTestAccess::project(*viewport, 6, 0, 10) + QPoint(0, -2);
    test::mouse(*viewport, QEvent::MouseMove, at, Qt::NoButton, Qt::NoButton);
    test::mouse(*viewport, QEvent::MouseButtonPress, at, Qt::LeftButton, Qt::LeftButton);
    test::mouse(*viewport, QEvent::MouseButtonRelease, at, Qt::LeftButton, Qt::NoButton);
    settle();
    require(guidance(window).startsWith(QStringLiteral("1 Kante")),
            "A click takes the edge: " + guidance(window).toStdString());
    test::mouse(*viewport, QEvent::MouseButtonDblClick, at, Qt::LeftButton, Qt::LeftButton);
    test::mouse(*viewport, QEvent::MouseButtonRelease, at, Qt::LeftButton, Qt::NoButton);
    settle();
    require(guidance(window).startsWith(QStringLiteral("3 Kante")),
            "A double click takes the tangent chain: " + guidance(window).toStdString());
    std::cout << "PASS tangent chain flow: a double click takes the edges joined smoothly\n";
}

void drag(viewport::CadViewport& viewport, QPoint from, QPoint to) {
    test::mouse(viewport, QEvent::MouseMove, from, Qt::NoButton, Qt::NoButton);
    test::mouse(viewport, QEvent::MouseButtonPress, from, Qt::LeftButton, Qt::LeftButton);
    test::mouse(viewport, QEvent::MouseMove, (from + to) / 2, Qt::NoButton, Qt::LeftButton);
    test::mouse(viewport, QEvent::MouseMove, to, Qt::NoButton, Qt::LeftButton);
    test::mouse(viewport, QEvent::MouseButtonRelease, to, Qt::LeftButton, Qt::NoButton);
    settle();
}

void boxFlow() {
    application::CadSession session;
    desktop::MainWindow window(session);
    window.resize(1300, 860);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    require(viewport != nullptr, "viewport");
    test::render(*viewport);
    session.newDocument();
    // Tall: no upright edge fits in the window around a top edge. Not as deep as wide: in the
    // isometric view a back edge would lie right behind the front corner.
    const auto box = *session.createBoxFeature({20, 30, 40}).feature();
    window.refreshAfterExternalChange();
    viewport->setCameraView(viewport::CameraView::isometric);
    require(session.selectFeature(box), "Box selected");
    window.refreshAfterExternalChange();
    command(window, "solid.modify.fillet").trigger();
    settle();
    test::render(*viewport);
    using Access = viewport::ViewportTestAccess;
    const QRect edge =
        QRect(Access::project(*viewport, 0, 0, 40), Access::project(*viewport, 20, 0, 40))
            .normalized()
            .adjusted(-8, -8, 8, 8);
    drag(*viewport, edge.topLeft(), edge.bottomRight());
    require(guidance(window).startsWith(QStringLiteral("1 Kante")),
            "A window takes the edge inside: " + guidance(window).toStdString());
    const QPoint corner = Access::project(*viewport, 20, 0, 40);
    // Wide enough to reach the edges that the previewed round now ends short of the corner.
    drag(*viewport, corner + QPoint(30, 30), corner - QPoint(30, 30));
    require(guidance(window).startsWith(QStringLiteral("3 Kante")),
            "A crossing box takes the edges it touches: " + guidance(window).toStdString());
    std::cout << "PASS edge box flow: a box in the fillet panel takes edges\n";
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    try {
        run();
        boxFlow();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
