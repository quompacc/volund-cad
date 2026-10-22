// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "../integration/CadSessionTestSupport.hpp"
#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QPushButton>
#include <TopExp_Explorer.hxx>
#include <cmath>
#include <iostream>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// T04c (user decision 2026-10-07: "Pfeil ziehen + Maßfeld"): a click on a planar face in the
// real window offers "Drücken/Ziehen"; the panel and the arrow in the viewport set the same
// distance; cancel leaves the model, OK creates one extrusion; pulling joins, pushing cuts.
// VOELUND_PUSH_PULL_ARTIFACT_DIR keeps captures for review.
namespace {
using namespace voelund;
using test::require;

bool matches(double a, double b) { return std::abs(a - b) < 1e-6 * std::max(1.0, std::abs(b)); }
double volume(const application::CadSession& session) {
    return test::volumeOf(session.currentShape());
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
QWidget& panel(QWidget& window) {
    auto* found = window.findChild<QWidget*>(QStringLiteral("activeToolPanel"));
    require(found != nullptr && found->isVisible(), "Tool panel shown");
    return *found;
}
QDoubleSpinBox& distanceField(QWidget& window) {
    QDoubleSpinBox* found = nullptr;
    for (auto* candidate :
         panel(window).findChildren<QDoubleSpinBox*>(QStringLiteral("toolLength_distance")))
        if (candidate->isVisible())
            found = candidate;
    require(found != nullptr, "Distance field");
    return *found;
}
QPushButton& panelButton(QWidget& window, const char* name) {
    auto* button = panel(window).findChild<QPushButton*>(QLatin1String(name));
    require(button != nullptr, "Panel button");
    return *button;
}
void capture(QWidget& window, const char* name) {
    const QString directory = qEnvironmentVariable("VOELUND_PUSH_PULL_ARTIFACT_DIR");
    if (directory.isEmpty())
        return;
    require(QDir().mkpath(directory), "capture directory");
    settle();
    require(window.grab().save(directory + QLatin1Char('/') + QLatin1String(name)), "capture");
}
// The middle of `support`'s face facing up, in world coordinates.
gp_Pnt topCenter(const application::CadSession& session, document::FeatureId support) {
    for (const auto& face : session.supportFaces(support))
        if (face.plane.normalZ > 0.99) {
            const auto& p = face.plane;
            const double yX = p.normalY * p.xDirectionZ - p.normalZ * p.xDirectionY;
            const double yY = p.normalZ * p.xDirectionX - p.normalX * p.xDirectionZ;
            const double yZ = p.normalX * p.xDirectionY - p.normalY * p.xDirectionX;
            const double u = (face.minX + face.maxX) / 2;
            const double v = (face.minY + face.maxY) / 2;
            return {p.originX + u * p.xDirectionX + v * yX, p.originY + u * p.xDirectionY + v * yY,
                    p.originZ + u * p.xDirectionZ + v * yZ};
        }
    throw std::runtime_error("No top face");
}
void click(viewport::CadViewport& viewport, QPoint point) {
    test::mouse(viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
    test::mouse(viewport, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
    test::mouse(viewport, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
    test::render(viewport);
    settle();
}
// Clicks the top face of `support` and opens push/pull on it.
void openOnTop(QWidget& window, viewport::CadViewport& viewport,
               const application::CadSession& session, document::FeatureId support) {
    const auto center = topCenter(session, support);
    click(viewport,
          viewport::ViewportTestAccess::project(viewport, center.X(), center.Y(), center.Z()));
    QAction& pushPull = command(window, "solid.modify.push_pull");
    require(pushPull.isEnabled(), "A picked planar face offers push/pull");
    pushPull.trigger();
    settle();
    test::render(viewport);
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
    const auto box = session.createBoxFeature({40, 40, 20});
    require(box.feature().has_value(), "box");
    window.refreshAfterExternalChange();
    viewport->setCameraView(viewport::CameraView::isometric);
    test::render(*viewport);
    const double boxVolume = volume(session);

    // Cancel leaves the model as it was.
    openOnTop(window, *viewport, session, *box.feature());
    require(matches(distanceField(window).value(), 10) &&
                panelButton(window, "toolAcceptButton").isEnabled() &&
                viewport::ViewportTestAccess::pushPullArrowZ(*viewport) > 0.99,
            "Push/pull opens with 10 mm, previewed and ready; the arrow points out");
    capture(window, "t04-push-pull-open.png");
    panelButton(window, "toolCancelButton").click();
    settle();
    require(session.features().size() == 1 && matches(volume(session), boxVolume),
            "Cancel leaves the model");

    // The arrow: find it above the face's middle and drag it upward.
    openOnTop(window, *viewport, session, *box.feature());
    const auto center = topCenter(session, *box.feature());
    const QPoint base =
        viewport::ViewportTestAccess::project(*viewport, center.X(), center.Y(), center.Z() + 10);
    const QPoint up =
        viewport::ViewportTestAccess::project(*viewport, center.X(), center.Y(), center.Z() + 30);
    const QPointF direction = QPointF(up - base) / std::hypot(up.x() - base.x(), up.y() - base.y());
    std::optional<QPoint> grip;
    for (int step = 0; step <= 90 && !grip; step += 3) {
        const QPoint candidate = base + (direction * step).toPoint();
        test::mouse(*viewport, QEvent::MouseMove, candidate, Qt::NoButton, Qt::NoButton);
        if (viewport::ViewportTestAccess::pushPullArrowHovered(*viewport))
            grip = candidate;
    }
    require(grip.has_value(), "The arrow points out of the face");
    const double previewed = viewport::ViewportTestAccess::displayedVolume(*viewport);
    test::mouse(*viewport, QEvent::MouseButtonPress, *grip, Qt::LeftButton, Qt::LeftButton);
    test::mouse(*viewport, QEvent::MouseMove, *grip + (direction * 40).toPoint(), Qt::NoButton,
                Qt::LeftButton);
    // S14 (user report): the body follows the arrow while the button is still down.
    QElapsedTimer waiting;
    waiting.start();
    while (waiting.elapsed() < 200)
        QApplication::processEvents(QEventLoop::AllEvents, 20);
    require(viewport::ViewportTestAccess::displayedVolume(*viewport) > previewed + 1.0,
            "The preview body grows during the drag, before the release");
    test::mouse(*viewport, QEvent::MouseButtonRelease, *grip + (direction * 40).toPoint(),
                Qt::LeftButton, Qt::NoButton);
    test::render(*viewport);
    settle();
    const double dragged = distanceField(window).value();
    require(dragged > 10 && std::abs(dragged - std::round(dragged)) < 1e-9,
            "Dragging the arrow outward lengthens the pull in snapped steps");
    capture(window, "t04-push-pull-dragged.png");

    // A typed distance wins; OK creates one extrusion that joins.
    distanceField(window).setValue(5);
    settle();
    panelButton(window, "toolAcceptButton").click();
    settle();
    require(session.features().size() == 2 && session.validBodies().size() == 1 &&
                matches(volume(session), boxVolume + 40 * 40 * 5),
            "OK pulls the top face 5 mm out");

    // T05 (user decision 2026-10-17): the new pull leaves no seams; "Naht entfernen" in its
    // properties brings them back and removes them again, the volume stays.
    const auto faces = [&session] {
        int count = 0;
        for (TopExp_Explorer explorer(session.currentShape(), TopAbs_FACE); explorer.More();
             explorer.Next())
            ++count;
        return count;
    };
    require(faces() == 6, "Pulling the top leaves six faces, no seams");
    const auto pull = session.features().back().id;
    require(session.selectFeature(pull), "The pull selected");
    window.refreshAfterExternalChange();
    settle();
    auto* seams = window.findChild<QCheckBox*>(QStringLiteral("extrusionMergeSeamsInput"));
    require(seams != nullptr && seams->isVisible() && seams->isChecked(),
            "The pull's properties offer \"Naht entfernen\", switched on");
    seams->click();
    settle();
    require(faces() == 10 && matches(volume(session), boxVolume + 40 * 40 * 5) &&
                !session.extrusionParameters(pull).mergeSeams,
            "Switched off, the seams are back");
    seams = window.findChild<QCheckBox*>(QStringLiteral("extrusionMergeSeamsInput"));
    seams->click();
    settle();
    require(faces() == 6 && session.extrusionParameters(pull).mergeSeams,
            "Switched on again, they are gone");

    // The new top (the pull's end face) is pushed 3 mm back in.
    openOnTop(window, *viewport, session, session.features().back().id);
    distanceField(window).setValue(-3);
    settle();
    require(viewport::ViewportTestAccess::pushPullArrowZ(*viewport) < -0.99,
            "For a push the arrow points into the body");
    capture(window, "t04-push-pull-push.png");
    panelButton(window, "toolAcceptButton").click();
    settle();
    require(session.features().size() == 3 && matches(volume(session), boxVolume + 40 * 40 * 2) &&
                session.features()[1].name == "Ziehen 1" &&
                session.features().back().name == "Drücken 1",
            "A negative distance pushes the face in; both are named after what they do");
    std::cout << "PASS T04c push/pull in the window: context command, arrow, field, cancel, "
                 "pull and push, seams removed and back\n";
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
