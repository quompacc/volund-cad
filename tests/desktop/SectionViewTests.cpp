// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "IsolatedSettings.hpp"
#include "UiDriver.hpp"
#include "ViewportLifecycleSupport.hpp"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QToolButton>
#include <cmath>
#include <iostream>
#include <optional>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// N05 (user decision 2026-10-23): a plate 40 x 20 x 10 with a through hole and a blind hole
// (the pocket). "Schnitt" cuts it at XZ, the field moves the cut through both holes, the arrow
// moves it too, a click on the removed half picks nothing there, closing shows the whole model;
// the document stays untouched throughout.
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
template <typename Widget> Widget& child(QWidget& window, const char* name) {
    auto* found = window.findChild<Widget*>(QLatin1String(name));
    if (found == nullptr)
        throw std::runtime_error(std::string("Widget is missing: ") + name);
    return *found;
}
void capture(QWidget& window, const char* name) {
    const QString directory = qEnvironmentVariable("VOELUND_SECTION_ARTIFACT_DIR");
    if (directory.isEmpty())
        return;
    require(QDir().mkpath(directory), "capture directory");
    settle();
    require(window.grab().save(directory + QLatin1Char('/') + QLatin1String(name)), "capture");
}

struct Hole final {
    double x;
    double y;
    double diameter;
    std::optional<double> depth; // none: through all
};

// A hole into the top face (z = 10) of `support`'s result at model (x, y).
document::FeatureId drill(application::CadSession& session, document::FeatureId support,
                          const Hole& hole) {
    for (const auto& face : session.supportFaces(support)) {
        const auto& p = face.plane;
        if (p.normalZ < 0.5 || std::abs(p.originZ - 10) > 1e-6)
            continue;
        const double dx = hole.x - p.originX, dy = hole.y - p.originY;
        const double yX = p.normalY * p.xDirectionZ - p.normalZ * p.xDirectionY;
        const double yY = p.normalZ * p.xDirectionX - p.normalX * p.xDirectionZ;
        const auto result =
            session.createHole(support, face.origin, dx * p.xDirectionX + dy * p.xDirectionY,
                               dx * yX + dy * yY, hole.diameter, hole.depth);
        require(result.feature().has_value(), "Hole");
        return *result.feature();
    }
    throw std::runtime_error("Top face missing");
}

double sectionY(viewport::CadViewport& viewport) {
    const auto plane = viewport::ViewportTestAccess::section(viewport);
    require(plane.has_value(), "A section is shown");
    return plane->origin[1];
}

// 3. The arrow points to the removed side; dragging it moves the cut and the field.
void dragArrow(viewport::CadViewport& viewport, QDoubleSpinBox& offset) {
    const QPoint base = viewport::ViewportTestAccess::project(viewport, 20, 10, 5);
    const QPoint front = viewport::ViewportTestAccess::project(viewport, 20, -20, 5);
    const QPointF direction =
        QPointF(front - base) / std::hypot(front.x() - base.x(), front.y() - base.y());
    std::optional<QPoint> grip;
    for (int step = 0; step <= 150 && !grip; step += 3) {
        const QPoint candidate = base + (direction * step).toPoint();
        test::mouse(viewport, QEvent::MouseMove, candidate, Qt::NoButton, Qt::NoButton);
        if (viewport::ViewportTestAccess::sectionArrowHovered(viewport))
            grip = candidate;
    }
    require(grip.has_value(), "The section arrow can be grabbed");
    test::mouse(viewport, QEvent::MouseButtonPress, *grip, Qt::LeftButton, Qt::LeftButton);
    test::mouse(viewport, QEvent::MouseMove, *grip + (direction * 40).toPoint(), Qt::NoButton,
                Qt::LeftButton);
    test::mouse(viewport, QEvent::MouseButtonRelease, *grip + (direction * 40).toPoint(),
                Qt::LeftButton, Qt::NoButton);
    test::render(viewport);
    settle();
    const double dragged = offset.value();
    require(dragged > -10 && std::abs(dragged - std::round(dragged)) < 1e-9 &&
                std::abs(sectionY(viewport) + dragged) < 1e-9,
            "Dragging the arrow forward moves cut and field in snapped steps");
}

// 4. Selection follows the cut: a click on the removed half of the top face hits nothing
// there. (The test takes the face picks from here on.)
void pickRemovedHalf(viewport::CadViewport& viewport, QDoubleSpinBox& offset) {
    offset.setValue(-10);
    settle();
    std::optional<std::array<double, 3>> picked;
    viewport.setPlanarFaceSelectionHandler([&](const viewport::PlanarFaceSelection& face) {
        if (face.pickedPoint)
            picked = face.pickedPoint;
    });
    test::render(viewport);
    const QPoint removed = viewport::ViewportTestAccess::project(viewport, 20, 4, 10);
    test::mouse(viewport, QEvent::MouseMove, removed, Qt::NoButton, Qt::NoButton);
    test::mouse(viewport, QEvent::MouseButtonPress, removed, Qt::LeftButton, Qt::LeftButton);
    test::mouse(viewport, QEvent::MouseButtonRelease, removed, Qt::LeftButton, Qt::NoButton);
    settle();
    require(!picked || (*picked)[1] > 10 - 1e-6, "The removed half can't be picked");
}

void closeSection(QWidget& window, viewport::CadViewport& viewport, const QAction& section) {
    child<QToolButton>(window, "sectionClose").click();
    settle();
    test::render(viewport);
    require(!section.isChecked() && !child<QWidget>(window, "sectionCard").isVisible() &&
                !viewport::ViewportTestAccess::section(viewport) &&
                !viewport::ViewportTestAccess::bodiesClipped(viewport),
            "Closing removes the cut");
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
    const auto plate = session.createBoxFeature({40, 20, 10}).feature();
    require(plate.has_value(), "Plate");
    const auto hole = drill(session, *plate, {10, 10, 6, std::nullopt});
    static_cast<void>(drill(session, hole, {30, 10, 8, 4.0}));
    window.refreshAfterExternalChange();
    viewport->setCameraView(viewport::CameraView::isometric);
    test::render(*viewport);
    const auto revision = session.sceneStatus().document;
    const bool modified = session.isModified();
    const bool canUndo = session.canUndo();

    // 1. The switch opens the card and cuts at XZ, the side away from the eye stays.
    QAction& section = command(window, "view.section");
    require(section.isCheckable() && !section.isChecked(), "Schnitt is a view switch");
    section.trigger();
    settle();
    test::render(*viewport);
    auto& card = child<QWidget>(window, "sectionCard");
    auto& planes = child<QComboBox>(window, "sectionPlane");
    auto& offset = child<QDoubleSpinBox>(window, "sectionOffset");
    const auto opened = viewport::ViewportTestAccess::section(*viewport);
    require(card.isVisible() && planes.currentText() == QStringLiteral("XZ-Ebene") && opened &&
                std::abs(opened->normal[1]) > 0.99 &&
                viewport::ViewportTestAccess::bodiesClipped(*viewport),
            "The card opens and the bodies are cut at XZ");
    require(opened->normal[1] > 0.99, "The back half stays (the eye looks from -Y)");

    // 2. The field moves the cut through both holes (the base normal of XZ is -Y).
    offset.setValue(-10);
    settle();
    test::render(*viewport);
    require(std::abs(sectionY(*viewport) - 10) < 1e-9, "Offset -10 puts the cut at y = 10");
    capture(window, "n05-section-holes.png");

    dragArrow(*viewport, offset);
    pickRemovedHalf(*viewport, offset);

    // 5. Closing shows the whole model; the document is unchanged and has no undo step.
    closeSection(window, *viewport, section);
    require(session.sceneStatus().document == revision && session.isModified() == modified &&
                session.canUndo() == canUndo,
            "The section leaves the document untouched");
    std::cout << "PASS N05 section view: XZ cut, field, arrow, selection on the cut model, "
                 "close, no document change"
              << std::endl;
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    try {
        run();
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
    return 0;
}
