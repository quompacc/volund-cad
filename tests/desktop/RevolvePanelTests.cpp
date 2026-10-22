// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QToolButton>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <numbers>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>
#include <voelund/features/SketchRegionIdentity.hpp>

// M20 (user decision 2026-10-14): a rectangle 10 x 20 on XZ, 5 from Z, with a construction
// line on Z. Pick its region, "Drehen": the panel turns about the construction line by 360
// degrees (4000 pi); 90 / 180 change the preview; "Z-Achse" and back to "Linie der Skizze"
// waits for a click on a line, the click on the construction line sets it again; OK is one
// undo step; the timeline reopens it with 270 degrees; Save/Open.
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
QPushButton& accept(QWidget& window) {
    auto* found = panel(window)->findChild<QPushButton*>(QStringLiteral("toolAcceptButton"));
    require(found != nullptr, "OK button");
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
void setAngle(QWidget& window, double degrees) {
    field<QDoubleSpinBox>(window, QStringLiteral("toolLength_angle"))->setValue(degrees);
    settle();
}
void chooseAxis(QWidget& window, int index) {
    field<QComboBox>(window, QStringLiteral("toolChoice_axis"))->setCurrentIndex(index);
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

void run() {
    application::CadSession session;
    desktop::MainWindow window(session);
    window.resize(1300, 860);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    require(viewport != nullptr, "viewport");
    test::render(*viewport);
    session.newDocument();

    // A rectangle 10 x 20 on XZ, 5 from Z, and a construction line on Z.
    document::SketchParameters sketch{document::datumSketchPlane(document::DatumPlane::xz), {}};
    sketch.datum = document::DatumPlane::xz;
    const document::SketchPoint corners[]{{5, 0}, {15, 0}, {15, 20}, {5, 20}};
    for (std::uint64_t index = 0; index < 4; ++index)
        sketch.elements.values.push_back(
            {document::SketchElementId{index + 1},
             document::SketchLine{corners[index], corners[(index + 1) % 4]}});
    sketch.elements.values.push_back(
        {document::SketchElementId{5}, document::SketchLine{{0, -5}, {0, 25}}, true});
    sketch.elements.nextId = 6;
    require(session.createDatumSketchFeature(sketch).status() ==
                application::CommandStatus::committed,
            "Sketch");
    const auto sketchId = session.features().back().id;
    const auto regions = features::identifySketchRegions(sketch.elements.values).ids;
    require(session.selectSketchRegions(sketchId, regions), "Region picked");
    window.refreshAfterExternalChange();
    viewport->setCameraView(viewport::CameraView::front);
    test::render(*viewport);

    QAction& revolve = command(window, "solid.create.revolve");
    require(revolve.isEnabled(), "Drehen offered for a picked region");
    revolve.trigger();
    settle();
    require(panel(window) != nullptr && accept(window).isEnabled() &&
                matches(shown(*viewport), 4000 * pi),
            "A full turn about the construction line previews 4000 pi");
    setAngle(window, 90);
    require(matches(shown(*viewport), 1000 * pi), "90 degrees preview a quarter");
    setAngle(window, 180);
    require(matches(shown(*viewport), 2000 * pi), "180 degrees preview a half");

    // Z, then back to a sketch line: OK waits for the clicked line.
    chooseAxis(window, 3);
    require(accept(window).isEnabled() && matches(shown(*viewport), 2000 * pi),
            "Z gives the same half");
    chooseAxis(window, 0);
    require(!accept(window).isEnabled(), "A sketch line is awaited");
    click(*viewport, 0, 0, 10);
    require(accept(window).isEnabled() && matches(shown(*viewport), 2000 * pi),
            "A click on the construction line makes it the axis");

    accept(window).click();
    settle();
    require(panel(window) == nullptr && session.features().back().name == "Drehung 1" &&
                matches(volume(session), 2000 * pi),
            "OK turns the region by 180 degrees");
    require(session.undo().status() == application::CommandStatus::committed &&
                session.validBodies().empty() &&
                session.redo().status() == application::CommandStatus::committed,
            "One undo step");
    window.refreshAfterExternalChange();

    timelineStep(window, 1).click();
    settle();
    require(panel(window) != nullptr, "The timeline reopens the revolve");
    setAngle(window, 270);
    accept(window).click();
    settle();
    require(matches(volume(session), 3000 * pi), "270 degrees after editing");

    const auto directory = std::filesystem::temp_directory_path() / "voelund-m20-revolve-panel";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const auto path = directory / "revolve.volund";
    session.saveDocument(path);
    window.openFile(QString::fromStdString(path.string()));
    settle();
    require(session.features().size() == 2 && matches(volume(session), 3000 * pi),
            "Open restores the revolve");
    std::filesystem::remove_all(directory);
    std::cout << "PASS M20 revolve panel: construction line axis, angles, Z, picked line, OK, "
                 "undo, edit, open\n";
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
