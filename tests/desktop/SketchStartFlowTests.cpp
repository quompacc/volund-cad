// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "DiscardPrompt.hpp"
#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QDoubleSpinBox>
#include <QKeyEvent>
#include <QPushButton>
#include <QStatusBar>
#include <QTreeWidget>
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <numbers>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// S03: sketch start through the real main window with real events. Empty document -> XY ->
// rectangle -> dimensions 20 x 10 -> OK without a box; XZ, YZ and a rotated support face show
// the sketch axes to the right and upwards; Cancel creates no feature; the camera returns to
// the view before the sketch after OK and Cancel; Edit Sketch opens through the same path.
namespace {
using namespace voelund;
using test::require;
void require(bool condition, const std::string& message) {
    test::require(condition, message.c_str());
}

// Captures for review, only when VOELUND_SMOKE_ARTIFACT_DIR is set.
void capture(QWidget& window, const QString& name) {
    const QString directory = qEnvironmentVariable("VOELUND_SMOKE_ARTIFACT_DIR");
    if (directory.isEmpty())
        return;
    QDir().mkpath(directory);
    window.grab().save(directory + "/sketch-start-" + name + ".png");
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

void click(viewport::CadViewport& viewport, double x, double y, double z) {
    const QPoint point = viewport::ViewportTestAccess::project(viewport, x, y, z);
    test::mouse(viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
    test::mouse(viewport, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
    test::mouse(viewport, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
    test::render(viewport);
    settle();
}

QWidget& toolPanel(QWidget& window) {
    auto* panel = window.findChild<QWidget*>(QStringLiteral("activeToolPanel"));
    require(panel != nullptr, "Tool panel missing");
    return *panel;
}

QPushButton& panelButton(QWidget& window, const char* name) {
    auto* button = toolPanel(window).findChild<QPushButton*>(QLatin1String(name));
    require(button != nullptr, "Tool panel button missing");
    return *button;
}

void enter(QWidget& window, const char* id, double value) {
    QDoubleSpinBox* field = nullptr; // earlier panels' fields may await deletion
    for (auto* candidate :
         toolPanel(window).findChildren<QDoubleSpinBox*>(QLatin1String("toolLength_") + id))
        if (candidate->isVisible())
            field = candidate;
    require(field != nullptr, "Panel field missing");
    field->setValue(value);
    emit field->editingFinished();
    settle();
}

void requireEditor(QWidget& window, const char* what) {
    auto* panel = window.findChild<QWidget*>(QStringLiteral("activeToolPanel"));
    require(panel != nullptr && panel->isVisible() &&
                window.findChild<QPushButton*>(QStringLiteral("toolAcceptButton")) != nullptr,
            std::string(what) + ": the sketch editor is open");
}

// Screen offset of a world direction seen from the point (x, y, z).
QPoint screenStep(viewport::CadViewport& viewport, const std::array<double, 3>& at,
                  const std::array<double, 3>& direction) {
    const QPoint from = viewport::ViewportTestAccess::project(viewport, at[0], at[1], at[2]);
    const QPoint to = viewport::ViewportTestAccess::project(
        viewport, at[0] + 10 * direction[0], at[1] + 10 * direction[1], at[2] + 10 * direction[2]);
    return to - from;
}

// The sketch x axis points right and its y axis up on screen.
void requireAxes(viewport::CadViewport& viewport, const std::array<double, 3>& at,
                 const std::array<double, 3>& x, const std::array<double, 3>& y, const char* what) {
    const QPoint right = screenStep(viewport, at, x);
    const QPoint up = screenStep(viewport, at, y);
    std::cout << what << ": x -> (" << right.x() << ", " << right.y() << "), y -> (" << up.x()
              << ", " << up.y() << ")\n";
    require(right.x() > 5 && std::abs(right.y()) <= 1, std::string(what) + ": x points right");
    require(up.y() < -5 && std::abs(up.x()) <= 1, std::string(what) + ": y points up");
}

using Camera = occ::handle<Graphic3d_Camera>;

// The camera is back where it was: same eye, centre, up and scale. The viewport may have
// changed its size meanwhile (tool panel, properties), so pixels are no measure.
void requireSameView(viewport::CadViewport& viewport, const Camera& before, const char* what) {
    const Camera after = viewport::ViewportTestAccess::camera(viewport);
    const double moved = before->Eye().Distance(after->Eye()) +
                         before->Center().Distance(after->Center()) +
                         (before->Up().IsEqual(after->Up(), 1e-9) ? 0.0 : 1.0) +
                         std::abs(before->Scale() - after->Scale());
    std::cout << what << ": camera difference " << moved << '\n';
    require(moved < 1e-6, std::string(what) + ": camera returned");
}

Camera probe(viewport::CadViewport& viewport) {
    return viewport::ViewportTestAccess::camera(viewport);
}

struct Window final {
    application::CadSession session;
    std::unique_ptr<desktop::MainWindow> owner;
    viewport::CadViewport* viewport{nullptr};

    // A box (with the sketch on its face) is part of the document before the window opens.
    explicit Window(bool withBox = false) {
        session.newDocument();
        if (withBox)
            setUpTurnedBox();
        owner = std::make_unique<desktop::MainWindow>(session);
        auto& window = *owner;
        window.resize(1300, 860);
        window.show();
        viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
        require(viewport != nullptr, "viewport");
        test::render(*viewport);
        viewport->setCameraView(viewport::CameraView::isometric);
        test::render(*viewport);
    }
    desktop::MainWindow& main() { return *owner; }

    void setUpTurnedBox() {
        require(session.createBoxFeature({40, 20, 30}).status() ==
                    application::CommandStatus::committed,
                "Box");
        auto box = session.selectedBoxParameters();
        box.placement.rotationZ = kernel::toRadians(kernel::Degrees(30));
        require(session.editSelectedBoxParameters(box).status() ==
                    application::CommandStatus::committed,
                "Turn the box");
    }
};

// A-UI-01 sketch basis: empty document -> XY -> rectangle -> 20 x 10 -> OK, no box needed.
void xyRectangle() {
    Window w;
    const Camera before = probe(*w.viewport);
    click(*w.viewport, 30, 30, 0);
    require(w.main().statusBar()->currentMessage().startsWith(QStringLiteral("XY")), "Pick XY");
    command(w.main(), "solid.create.sketch").trigger();
    settle();
    test::render(*w.viewport);
    requireEditor(w.main(), "XY");
    requireAxes(*w.viewport, {0, 0, 0}, {1, 0, 0}, {0, 1, 0}, "XY");

    command(w.main(), "sketch.tool.rectangle").trigger();
    click(*w.viewport, 0, 0, 0);
    click(*w.viewport, 18, 9, 0);
    command(w.main(), "sketch.tool.select").trigger();
    click(*w.viewport, 9, 0, 0); // the bottom edge
    enter(w.main(), "line_length", 20);
    click(*w.viewport, 0, 4.5, 0); // the left edge
    enter(w.main(), "line_length", 10);
    QPushButton& accept = panelButton(w.main(), "toolAcceptButton");
    require(accept.isEnabled(), "The dimensioned rectangle can be confirmed");
    accept.click();
    settle();
    test::render(*w.viewport);
    const auto features = w.session.features();
    require(features.size() == 1 && features.front().kind == document::FeatureKind::sketch,
            "Exactly one sketch, no box");
    const auto sketch = w.session.sketchParameters(features.front().id);
    require(sketch.datum == document::DatumPlane::xy, "The sketch lies on XY");
    require(!sketch.constraints.values.empty(), "The dimensions are stored as constraints");
    double minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
    for (const auto& element : sketch.elements.values) {
        const auto extend = [&](const document::SketchPoint& p) {
            minX = std::min(minX, p.x), maxX = std::max(maxX, p.x);
            minY = std::min(minY, p.y), maxY = std::max(maxY, p.y);
        };
        if (const auto* line = std::get_if<document::SketchLine>(&element.geometry)) {
            extend(line->start);
            extend(line->end);
        } else if (const auto* rectangle =
                       std::get_if<document::SketchRectangle>(&element.geometry)) {
            extend(rectangle->first);
            extend(rectangle->second);
        }
    }
    std::cout << "rectangle " << maxX - minX << " x " << maxY - minY << '\n';
    require(std::abs(maxX - minX - 20) < 1e-6 && std::abs(maxY - minY - 10) < 1e-6,
            "The rectangle is 20 x 10");
    requireSameView(*w.viewport, before, "After OK");

    // Edit Sketch opens through the same path, oriented again; Cancel keeps it unchanged.
    require(w.session.selectFeature(features.front().id), "Select the sketch");
    w.main().statusBar()->clearMessage();
    command(w.main(), "solid.modify.edit_sketch").trigger();
    settle();
    test::render(*w.viewport);
    requireEditor(w.main(), "Edit XY");
    requireAxes(*w.viewport, {0, 0, 0}, {1, 0, 0}, {0, 1, 0}, "Edit XY");
    // S13: Escape no longer leaves the editor; Cancel without changes asks nothing.
    panelButton(w.main(), "toolCancelButton").click();
    settle();
    test::render(*w.viewport);
    require(w.session.features().size() == 1 &&
                w.session.sketchParameters(features.front().id) == sketch,
            "Cancel of Edit Sketch changes nothing");
    requireSameView(*w.viewport, before, "After editing");
    std::cout << "PASS S03 XY: rectangle 20 x 10 without a box, axes, return camera, edit\n";
}

void selectTreePlane(desktop::MainWindow& window, int index) {
    auto* tree = window.findChild<QTreeWidget*>();
    require(tree != nullptr && tree->topLevelItemCount() > 0, "Model tree");
    QTreeWidgetItem* plane = tree->topLevelItem(0)->child(index);
    require(plane != nullptr, "Plane in the tree");
    emit tree->itemClicked(plane, 0);
    settle();
}

// XZ and YZ: right axes, Cancel creates no feature, camera returns.
void sidePlanes() {
    Window w;
    struct Case {
        int index;
        const char* name;
        std::array<double, 3> x;
    };
    for (const Case& item : {Case{1, "XZ", {1, 0, 0}}, Case{2, "YZ", {0, 1, 0}}}) {
        const Camera before = probe(*w.viewport);
        selectTreePlane(w.main(), item.index);
        require(w.main().statusBar()->currentMessage().startsWith(QLatin1String(item.name)),
                std::string("Pick ") + item.name);
        command(w.main(), "solid.create.sketch").trigger();
        settle();
        test::render(*w.viewport);
        requireEditor(w.main(), item.name);
        requireAxes(*w.viewport, {0, 0, 0}, item.x, {0, 0, 1}, item.name);
        capture(w.main(), QString::fromLatin1(item.name).toLower());
        command(w.main(), "sketch.tool.line").trigger();
        const auto at = [&](double u, double v) {
            click(*w.viewport, u * item.x[0], u * item.x[1], v);
        };
        at(0, 0);
        at(15, 5);
        bool asked = false;
        test::answerDiscardQuestion(QStringLiteral("Verwerfen"), &asked);
        panelButton(w.main(), "toolCancelButton").click();
        settle();
        test::render(*w.viewport);
        require(asked && w.session.features().empty(),
                std::string(item.name) + ": Cancel asks, then adds nothing");
        requireSameView(*w.viewport, before, item.name);
    }
    std::cout << "PASS S03 XZ/YZ: axes, Cancel without a feature, return camera\n";
}

// A face of a box turned by 30 degrees about Z: the new sketch stands upright, its x runs
// along the face horizontally and its y along +Z, on screen and in the stored frame.
void rotatedSupport() {
    Window w(true);
    const double c = std::cos(std::numbers::pi / 6), s = std::sin(std::numbers::pi / 6);
    const std::array<double, 3> along{c, s, 0}; // the front face runs along the turned X
    // The front face (y = -depth/2 before the turn) faces (s, -c, 0); provenance keeps it
    // reversed against the solid, the case that failed before S03.
    const auto faces = w.session.supportFaces(w.session.features().front().id);
    const auto front = std::ranges::find_if(faces, [&](const auto& face) {
        return std::abs(face.plane.normalX - s) < 1e-9 && std::abs(face.plane.normalY + c) < 1e-9;
    });
    require(front != faces.end(), "The turned box offers its front face");
    const auto& f = front->plane;
    const double fy[3]{f.normalY * f.xDirectionZ - f.normalZ * f.xDirectionY,
                       f.normalZ * f.xDirectionX - f.normalX * f.xDirectionZ,
                       f.normalX * f.xDirectionY - f.normalY * f.xDirectionX};
    const double cx = (front->minX + front->maxX) / 2, cy = (front->minY + front->maxY) / 2;
    // A point of the face, u along the face and v upwards from its centre.
    const auto onFace = [&](double u, double v) {
        return std::array<double, 3>{f.originX + cx * f.xDirectionX + cy * fy[0] + u * c,
                                     f.originY + cx * f.xDirectionY + cy * fy[1] + u * s,
                                     f.originZ + cx * f.xDirectionZ + cy * fy[2] + v};
    };
    w.viewport->setCameraView(viewport::CameraView::front); // the face looks towards -Y
    test::render(*w.viewport);
    const Camera before = probe(*w.viewport);
    capture(w.main(), QStringLiteral("rotated-face-before"));
    const auto pick = onFace(0, 0);
    click(*w.viewport, pick[0], pick[1], pick[2]);
    command(w.main(), "solid.create.sketch").trigger();
    settle();
    test::render(*w.viewport);
    requireEditor(w.main(), "Sketch on the rotated face");
    requireAxes(*w.viewport, pick, along, {0, 0, 1}, "Rotated face");
    capture(w.main(), QStringLiteral("rotated-face"));

    command(w.main(), "sketch.tool.rectangle").trigger();
    const auto first = onFace(-8, -5), second = onFace(8, 5);
    click(*w.viewport, first[0], first[1], first[2]);
    click(*w.viewport, second[0], second[1], second[2]);
    panelButton(w.main(), "toolAcceptButton").click();
    settle();
    test::render(*w.viewport);
    const auto features = w.session.features();
    require(features.size() == 2 && features.back().kind == document::FeatureKind::sketch,
            "Sketch on the rotated face");
    const auto plane = w.session.sketchParameters(features.back().id).plane;
    std::cout << "stored x (" << plane.xDirectionX << ", " << plane.xDirectionY << ", "
              << plane.xDirectionZ << ")\n";
    require(std::abs(plane.xDirectionX - c) < 1e-6 && std::abs(plane.xDirectionY - s) < 1e-6 &&
                std::abs(plane.xDirectionZ) < 1e-6,
            "The stored sketch x runs horizontally along the face");
    requireSameView(*w.viewport, before, "Rotated face");
    capture(w.main(), QStringLiteral("rotated-face-after-ok"));
    std::cout << "PASS S03 rotated support face: upright frame, axes, return camera\n";
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    try {
        xyRectangle();
        sidePlanes();
        rotatedSupport();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
