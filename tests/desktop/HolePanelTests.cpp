// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "../BoundSketchFixture.hpp"
#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"
#include <BRepBndLib.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <numbers>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// M08 (user decision 2026-10-13): a click on the top face of a 40 x 30 x 10 box, then
// "Bohrung", opens the panel with the hole at the click point. Further clicks on the face,
// also on the previewed body, move it; depth 4 and "Durchgehend" change the preview; OK is
// one undo step; the timeline reopens the hole, a new diameter is one more step; Save/Open.
namespace {
using namespace voelund;
using test::require;
constexpr double pi = std::numbers::pi;

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
bool solidAt(const TopoDS_Shape& shape, double x, double y, double z) {
    BRepClass3d_SolidClassifier classifier(shape, gp_Pnt(x, y, z), 1e-6);
    return classifier.State() == TopAbs_IN;
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
QWidget* panel(QWidget& window) {
    auto* found = window.findChild<QWidget*>(QStringLiteral("activeToolPanel"));
    return found != nullptr && found->isVisible() ? found : nullptr;
}
QPushButton& button(QWidget& window, const char* name) {
    auto* found = panel(window)->findChild<QPushButton*>(QLatin1String(name));
    require(found != nullptr, name);
    return *found;
}
template <class Field> Field* field(QWidget& window, const QString& name) {
    Field* found = nullptr;
    for (auto* candidate : panel(window)->findChildren<Field*>(name))
        if (candidate->isVisible())
            found = candidate;
    require(found != nullptr, "Panel field");
    return found;
}
double length(QWidget& window, const char* id) {
    return field<QDoubleSpinBox>(window, QStringLiteral("toolLength_") + QLatin1String(id))
        ->value();
}
void setLength(QWidget& window, const char* id, double value) {
    field<QDoubleSpinBox>(window, QStringLiteral("toolLength_") + QLatin1String(id))
        ->setValue(value);
    settle();
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

// N09: the reopened hole (Ø6 through the 10 mm plate) becomes an M6 tapped hole - its core
// drill 5.0, the diameter locked - with a 90° countersink, Ø10 by default (2 d).
void variant(desktop::MainWindow& window, application::CadSession& session) {
    timelineStep(window, 1).click();
    settle();
    require(panel(window) != nullptr, "The timeline reopens the hole");
    field<QComboBox>(window, QStringLiteral("toolChoice_thread"))->setCurrentIndex(7); // M6
    settle();
    auto* diameter = field<QDoubleSpinBox>(window, QStringLiteral("toolLength_diameter"));
    require(std::abs(diameter->value() - 5.0) < 1e-9 && !diameter->isEnabled(),
            "M6 takes the 5.0 core drill");
    field<QComboBox>(window, QStringLiteral("toolChoice_kind"))->setCurrentIndex(2); // Senkung
    settle();
    require(std::abs(length(window, "headDiameter") - 10.0) < 1e-9, "Countersink Ø10 by default");
    button(window, "toolAcceptButton").click();
    settle();
    const auto hole = session.features().back().id;
    const auto stored = session.holeParameters(hole).variant;
    // Through 10 mm: the cone from Ø10 to Ø5 is 2.5 high, then the core drill.
    const double removed = pi * 2.5 * (25 + 12.5 + 6.25) / 3 + pi * 6.25 * 7.5;
    require(stored.thread == "M6" && stored.kind == document::HoleKind::countersink &&
                matches(volume(session), 12000 - removed),
            "M6 with a countersink: thread stated, volume exact");
}

// N09b: a sketch with three points on the top face, selected: "Bohrung" opens without X/Y,
// and OK drills three Ø6 holes 4 deep in one step.
void sketchHoles(desktop::MainWindow& window, application::CadSession& session) {
    session.newDocument();
    const auto box = *session.createBoxFeature({40, 30, 10}).feature();
    const auto point = [](std::uint64_t id, double x, double y) {
        return document::SketchElement{document::SketchElementId{id},
                                       document::SketchPointGeometry{{x, y}}};
    };
    const document::SketchParameters points{
        {0, 0, 10, 0, 0, 1, 1, 0, 0}, {{point(1, 10, 10), point(2, 20, 15), point(3, 30, 20)}, 4}};
    const auto sketch = *session.createSketchFeature(box, points).feature();
    test::bindTop(session, sketch);
    require(session.selectFeature(sketch), "Sketch selected");
    window.refreshAfterExternalChange();
    settle();
    QAction& hole = command(window, "solid.modify.hole");
    require(hole.isEnabled(), "Bohrung offered for a face sketch with points");
    hole.trigger();
    settle();
    require(panel(window) != nullptr &&
                panel(window)
                        ->findChild<QDoubleSpinBox*>(QStringLiteral("toolLength_x"))
                        ->isVisible() == false,
            "The panel opens without X/Y");
    setLength(window, "depth", 4);
    button(window, "toolAcceptButton").click();
    settle();
    require(session.features().back().kind == document::FeatureKind::hole &&
                session.holeParameters(session.features().back().id).points.size() == 3 &&
                matches(volume(session), 12000 - 3 * 36 * pi),
            "OK drills three holes in one step");
}

void variantAndSketchHoles(desktop::MainWindow& window, application::CadSession& session) {
    variant(window, session);
    sketchHoles(window, session);
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
    require(session.createBoxFeature({40, 30, 10}).feature().has_value(), "Box");
    window.refreshAfterExternalChange();
    viewport->setCameraView(viewport::CameraView::top);
    test::render(*viewport);
    Bnd_Box bounds;
    BRepBndLib::Add(session.validBodies().front().shape, bounds);
    double x0 = 0, y0 = 0, z0 = 0, x1 = 0, y1 = 0, z1 = 0;
    bounds.Get(x0, y0, z0, x1, y1, z1);

    // Pick the top face, then "Bohrung": the hole sits at the click point.
    click(*viewport, x0 + 10, y0 + 10, z1);
    QAction& hole = command(window, "solid.modify.hole");
    require(hole.isEnabled(), "Bohrung offered for a picked face");
    hole.trigger();
    settle();
    require(panel(window) != nullptr, "The hole panel opens");
    setLength(window, "depth", 4);
    require(matches(shown(*viewport), 12000 - 36 * pi), "Depth 4 previews 36 pi");
    const double firstX = length(window, "x");

    // Further clicks move it, also on the previewed body.
    click(*viewport, x0 + 30, y0 + 20, z1);
    require(std::abs(length(window, "x") - firstX) > 15 &&
                matches(shown(*viewport), 12000 - 36 * pi),
            "A second click moves the hole across the face");
    click(*viewport, x0 + 25, y0 + 15, z1);
    require(matches(shown(*viewport), 12000 - 36 * pi), "A third click moves it again");
    field<QComboBox>(window, QStringLiteral("toolChoice_end"))->setCurrentIndex(1);
    settle();
    require(matches(shown(*viewport), 12000 - 90 * pi) &&
                !field<QDoubleSpinBox>(window, QStringLiteral("toolLength_depth"))->isEnabled(),
            "Durchgehend previews 90 pi without a depth");
    button(window, "toolAcceptButton").click();
    settle();
    const auto& shape = session.validBodies().front().shape;
    require(panel(window) == nullptr && session.features().size() == 2 &&
                session.features().back().kind == document::FeatureKind::hole &&
                matches(volume(session), 12000 - 90 * pi) &&
                !solidAt(shape, x0 + 25, y0 + 15, z0 + 1) &&
                solidAt(shape, x0 + 10, y0 + 10, z0 + 1),
            "OK drills through at the last click point");
    require(session.undo().status() == application::CommandStatus::committed &&
                matches(volume(session), 12000) &&
                session.redo().status() == application::CommandStatus::committed,
            "One undo step");
    window.refreshAfterExternalChange();
    // The timeline reopens the hole; a new diameter is one more step.
    timelineStep(window, 1).click();
    settle();
    require(panel(window) != nullptr, "The timeline reopens the hole");
    setLength(window, "diameter", 8);
    button(window, "toolAcceptButton").click();
    settle();
    require(session.features().size() == 2 && matches(volume(session), 12000 - 160 * pi),
            "Diameter 8 through all removes 160 pi");
    require(session.undo().status() == application::CommandStatus::committed &&
                matches(volume(session), 12000 - 90 * pi),
            "Undo returns to diameter 6");
    const auto directory = std::filesystem::temp_directory_path() / "voelund-m08-hole-panel";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const auto path = directory / "hole.volund";
    session.saveDocument(path);
    window.openFile(QString::fromStdString(path.string()));
    settle();
    require(session.features().size() == 2 && matches(volume(session), 12000 - 90 * pi),
            "Open restores the hole");
    std::filesystem::remove_all(directory);
    variantAndSketchHoles(window, session);
    std::cout << "PASS M08 hole panel: place by click, move by clicks over the preview, depth, "
                 "through, OK, undo, edit, open; N09 tapped M6 with countersink, holes at sketch "
                 "points\n";
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
