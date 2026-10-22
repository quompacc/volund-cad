// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"
#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <QAction>
#include <QApplication>
#include <QComboBox>
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

// M21 (user decision 2026-10-14): a box 40 x 30 x 10 and two separate cylinders through it
// (r 5 and r 3, height 20). Pick the box, "Kombinieren", "Abziehen"; clicks on the cylinders
// add them one after the other over the preview, a click on the previewed box changes
// nothing, "Werkzeugkörper zurücksetzen" empties the list, the clicks repeat; OK is one undo
// step; the timeline reopens the combination and "Behalten" keeps the cylinders; Save/Open.
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
QComboBox& choice(QWidget& window, const char* id) {
    QComboBox* found = nullptr;
    for (auto* candidate :
         panel(window)->findChildren<QComboBox*>(QStringLiteral("toolChoice_") + QLatin1String(id)))
        if (candidate->isVisible())
            found = candidate;
    require(found != nullptr, "Panel choice");
    return *found;
}
void clearTools(QWidget& window) {
    for (auto* candidate : panel(window)->findChildren<QPushButton*>())
        if (candidate->toolTip() == QStringLiteral("Werkzeugkörper zurücksetzen")) {
            candidate->click();
            settle();
            return;
        }
    throw std::runtime_error("Reset button missing");
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
document::FeatureId cylinder(application::CadSession& session, double radius, double x, double y) {
    const auto created = session.createNewBodyCylinder();
    require(created.feature().has_value(), "Cylinder");
    auto parameters = session.cylinderParameters(*created.feature());
    parameters.radius = radius;
    parameters.height = 20;
    parameters.placement.translationX = x;
    parameters.placement.translationY = y;
    parameters.placement.translationZ = -5;
    require(session.editCylinderParameters(*created.feature(), parameters).status() ==
                application::CommandStatus::committed,
            "Cylinder placed");
    return *created.feature();
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
    cylinder(session, 5, 25, 15);
    cylinder(session, 3, 8, 8);
    window.refreshAfterExternalChange();
    viewport->setCameraView(viewport::CameraView::top);
    test::render(*viewport);
    const double box = 12000, first = 250 * pi, second = 90 * pi;
    const double firstBody = 500 * pi, secondBody = 180 * pi;
    require(matches(shown(*viewport), box + firstBody + secondBody), "Three bodies are shown");

    // Pick the box away from the cylinders, then "Kombinieren" and "Abziehen".
    click(*viewport, 36, 26, 10);
    QAction& combine = command(window, "solid.modify.combine");
    require(combine.isEnabled(), "Kombinieren offered for a picked body");
    combine.trigger();
    settle();
    require(panel(window) != nullptr && !accept(window).isEnabled(),
            "The panel opens without tools");
    choice(window, "operation").setCurrentIndex(1);
    settle();

    // Clicks on the cylinder tops add them one after the other, over the preview.
    const auto toolsOnce = [&] {
        click(*viewport, 25, 15, 15);
        require(accept(window).isEnabled() && matches(shown(*viewport), box - first + secondBody),
                "The first cylinder is cut out");
        click(*viewport, 8, 8, 15);
        if (!matches(shown(*viewport), box - first - second))
            std::cerr << "shown " << shown(*viewport) << " expected " << box - first - second
                      << " guidance "
                      << panel(window)
                             ->findChild<QLabel*>(QStringLiteral("toolGuidance"))
                             ->text()
                             .toStdString()
                      << std::endl;
        require(matches(shown(*viewport), box - first - second),
                "The second cylinder is cut out as well");
        click(*viewport, 36, 26, 10);
        require(matches(shown(*viewport), box - first - second),
                "A click on the previewed box changes nothing");
    };
    toolsOnce();
    clearTools(window);
    require(!accept(window).isEnabled() && matches(shown(*viewport), box + firstBody + secondBody),
            "Reset shows the model again");
    toolsOnce();
    accept(window).click();
    settle();
    require(panel(window) == nullptr && session.features().size() == 4 &&
                session.features().back().kind == document::FeatureKind::combine &&
                session.validBodies().size() == 1 && matches(volume(session), box - first - second),
            "OK cuts both cylinders out of the box");
    require(session.undo().status() == application::CommandStatus::committed &&
                session.validBodies().size() == 3 &&
                session.redo().status() == application::CommandStatus::committed,
            "One undo step");
    window.refreshAfterExternalChange();

    // The timeline reopens the combination; "Behalten" keeps the cylinders.
    timelineStep(window, 3).click();
    settle();
    require(panel(window) != nullptr, "The timeline reopens the combination");
    choice(window, "tools").setCurrentIndex(1);
    settle();
    accept(window).click();
    settle();
    require(session.validBodies().size() == 3 &&
                matches(volume(session), box - first - second + firstBody + secondBody),
            "Kept cylinders stay separate bodies");

    const auto directory = std::filesystem::temp_directory_path() / "voelund-m21-combine-panel";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const auto path = directory / "combine.volund";
    session.saveDocument(path);
    window.openFile(QString::fromStdString(path.string()));
    settle();
    require(session.features().size() == 4 && session.validBodies().size() == 3 &&
                matches(volume(session), box - first - second + firstBody + secondBody),
            "Open restores the combination");
    std::filesystem::remove_all(directory);
    std::cout << "PASS M21 combine panel: pick target, add tools by clicks over the preview, "
                 "reset, OK, undo, edit, keep, open\n";
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
