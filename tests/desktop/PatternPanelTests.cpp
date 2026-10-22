// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"
#include <BRepClass3d_SolidClassifier.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QStatusBar>
#include <QToolButton>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// M10 (user decision 2026-10-15): a box 20 x 20 x 10 at the origin. "Lineares Muster" on the
// selected box: three copies along X 20 apart, then four; 25 apart; joined 25 apart is refused,
// 10 apart overlaps into one body; a click on a copy's edge in the preview is refused, a click
// on the original's vertical corner edge stacks the copies along it; circular about that edge
// four quarters, joined into one square; OK is one undo step; the timeline reopens it as new
// bodies; Bearbeiten on a copy reopens the pattern; Save/Open.
namespace {
using namespace voelund;
using test::require;

double volume(const application::CadSession& session) {
    double total = 0;
    for (const auto& body : session.validBodies()) {
        GProp_GProps properties;
        BRepGProp::VolumeProperties(body.shape, properties);
        total += properties.Mass();
    }
    return total;
}
bool matches(double a, double b) { return std::abs(a - b) < 1e-6 * std::max(1.0, std::abs(b)); }
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
QWidget* panel(QWidget& window) {
    auto* found = window.findChild<QWidget*>(QStringLiteral("activeToolPanel"));
    return found != nullptr && found->isVisible() ? found : nullptr;
}
QPushButton& button(QWidget& window, const char* name) {
    auto* found = panel(window)->findChild<QPushButton*>(QLatin1String(name));
    require(found != nullptr, name);
    return *found;
}
QPushButton& accept(QWidget& window) { return button(window, "toolAcceptButton"); }
template <class Field> Field* field(QWidget& window, const QString& name) {
    Field* found = nullptr;
    for (auto* candidate : panel(window)->findChildren<Field*>(name))
        if (candidate->isVisible())
            found = candidate;
    require(found != nullptr, "Panel field");
    return found;
}
QToolButton& timelineStep(QWidget& window, int index) {
    int seen = 0;
    for (auto* step : window.findChildren<QToolButton*>(QStringLiteral("timelineStep")))
        if (step->isVisible() && seen++ == index)
            return *step;
    throw std::runtime_error("Timeline step missing");
}
void click(viewport::CadViewport& viewport, double x, double y, double z) {
    test::render(viewport);
    const QPoint point = viewport::ViewportTestAccess::project(viewport, x, y, z);
    test::mouse(viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
    test::mouse(viewport, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
    test::mouse(viewport, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
    test::render(viewport);
    settle();
}
double shown(viewport::CadViewport& viewport) {
    test::render(viewport);
    return viewport::ViewportTestAccess::displayedVolume(viewport);
}

void choose(QWidget& window, const char* name, int index) {
    field<QComboBox>(window, QString::fromLatin1(name))->setCurrentIndex(index);
    settle();
}
void setValue(QWidget& window, const char* name, double value) {
    field<QDoubleSpinBox>(window, QString::fromLatin1(name))->setValue(value);
    settle();
}
bool solidAt(const application::CadSession& session, double x, double y, double z) {
    for (const auto& body : session.validBodies())
        if (BRepClass3d_SolidClassifier(body.shape, gp_Pnt(x, y, z), 1e-7).State() == TopAbs_IN)
            return true;
    return false;
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
    const auto box = *session.createBoxFeature({20, 20, 10}).feature();
    window.refreshAfterExternalChange();
    viewport->setCameraView(viewport::CameraView::isometric);
    test::render(*viewport);
    require(session.selectFeature(box), "Box selected");
    window.refreshAfterExternalChange();

    QAction& linear = command(window, "solid.modify.pattern_linear");
    require(linear.isEnabled(), "Lineares Muster offered for the selected box");
    linear.trigger();
    settle();
    require(panel(window) != nullptr && accept(window).isEnabled() &&
                matches(shown(*viewport), 3 * 4000),
            "Three boxes along X preview as bodies");
    setValue(window, "toolLength_count", 4);
    require(matches(shown(*viewport), 4 * 4000), "Four boxes");
    setValue(window, "toolLength_spacing", 25);
    require(matches(shown(*viewport), 4 * 4000), "Four boxes 25 apart");
    choose(window, "toolChoice_mode", 1);
    require(!accept(window).isEnabled(), "Joined boxes 25 apart are refused, OK waits");
    setValue(window, "toolLength_spacing", 10);
    require(accept(window).isEnabled() && matches(shown(*viewport), 50 * 20 * 10),
            "Joined boxes 10 apart overlap into one body");

    // Picks over the preview: an edge of a copy cannot carry the axis; the original's
    // vertical corner edge can.
    choose(window, "toolChoice_mode", 0);
    setValue(window, "toolLength_spacing", 25);
    click(*viewport, 45, 20, 5);
    require(field<QComboBox>(window, QStringLiteral("toolChoice_axis"))->currentIndex() == 0,
            "A copy's edge is refused; the X axis stays");
    // The copies far off, so the original's corner edge stands free of their edges.
    setValue(window, "toolLength_spacing", 60);
    click(*viewport, 20, 20, 5);
    require(field<QComboBox>(window, QStringLiteral("toolChoice_axis"))->currentIndex() == 3 &&
                accept(window).isEnabled() && matches(shown(*viewport), 4 * 4000),
            "The corner edge stacks the copies along it");

    choose(window, "toolChoice_kind", 1);
    require(accept(window).isEnabled() && matches(shown(*viewport), 4 * 4000),
            "Circular about the corner edge: four quarters");
    choose(window, "toolChoice_mode", 1);
    require(accept(window).isEnabled() && matches(shown(*viewport), 4 * 4000),
            "Joined into one square");
    accept(window).click();
    settle();
    require(panel(window) == nullptr && session.features().back().name == "Muster 1" &&
                session.validBodies().size() == 1 && matches(volume(session), 4 * 4000) &&
                solidAt(session, 30, 30, 5) && solidAt(session, 10, 30, 5),
            "OK joins the four quarters");
    require(session.undo().status() == application::CommandStatus::committed &&
                matches(volume(session), 4000) &&
                session.redo().status() == application::CommandStatus::committed,
            "One undo step");
    window.refreshAfterExternalChange();

    timelineStep(window, 1).click();
    settle();
    require(panel(window) != nullptr, "The timeline reopens the pattern");
    choose(window, "toolChoice_mode", 0);
    accept(window).click();
    settle();
    require(session.validBodies().size() == 4 && matches(volume(session), 4 * 4000),
            "Four bodies again");

    const auto copy = session.features().back().id;
    require(session.features().back().kind == document::FeatureKind::patternCopy &&
                session.selectFeature(copy),
            "A copy selected");
    window.refreshAfterExternalChange();
    command(window, "solid.modify.edit_sketch").trigger();
    settle();
    require(panel(window) != nullptr, "Bearbeiten on a copy reopens its pattern");
    button(window, "toolCancelButton").click();
    settle();

    const auto directory = std::filesystem::temp_directory_path() / "voelund-m10-pattern-panel";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const auto path = directory / "pattern.volund";
    session.saveDocument(path);
    window.openFile(QString::fromStdString(path.string()));
    settle();
    require(session.features().size() == 5 && session.validBodies().size() == 4 &&
                matches(volume(session), 4 * 4000),
            "Open restores the pattern and its copies");
    std::filesystem::remove_all(directory);
    std::cout << "PASS M10 pattern panel: linear count and spacing, joined refused and "
                 "overlapping, copy edge refused, corner edge, circular, joined, OK, undo, "
                 "timeline, copy reopens, open"
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
