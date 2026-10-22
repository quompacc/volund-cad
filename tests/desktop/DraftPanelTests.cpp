// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <QAction>
#include <QApplication>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <cmath>
#include <iostream>
#include <numbers>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// M25 (user decision 2026-10-14): a box 40 x 30 x 20. Pick the top face, "Formschräge": the top
// is the neutral face. A click on the front face tilts it by 5 degrees (a wedge of the box
// goes); a second click on the same face (on the preview) takes it back, a third tilts it
// again; 10 degrees follow; OK keeps "Formschräge 1"; undo.
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
bool matches(double a, double b) { return std::abs(a - b) < 1e-4 * std::max(1.0, std::abs(b)); }
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
// The wedge of the 40 wide front face tilted about its top edge over the height of 20.
double tilted(double degrees) { return 24000 - 0.5 * 20 * 20 * std::tan(degrees * pi / 180) * 40; }

void run() {
    application::CadSession session;
    desktop::MainWindow window(session);
    window.resize(1300, 860);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    require(viewport != nullptr, "viewport");
    test::render(*viewport);
    session.newDocument();
    require(session.createBoxFeature({40, 30, 20}).feature().has_value(), "Box");
    window.refreshAfterExternalChange();
    viewport->setCameraView(viewport::CameraView::top);
    test::render(*viewport);

    click(*viewport, 20, 15, 20);
    QAction& draft = command(window, "solid.modify.draft");
    require(draft.isEnabled(), "Formschräge offered for a picked face");
    draft.trigger();
    settle();
    require(panel(window) != nullptr, "The panel opens with the top as neutral face");

    viewport->setCameraView(viewport::CameraView::front);
    test::render(*viewport);
    click(*viewport, 30, 0, 10);
    require(matches(shown(*viewport), tilted(5)), "The front face tilts by 5 degrees");
    click(*viewport, 20, 0, 12);
    require(matches(shown(*viewport), 24000), "A second click on it takes it back");
    click(*viewport, 20, 0, 12);
    require(matches(shown(*viewport), tilted(5)), "A third click tilts it again");
    QDoubleSpinBox* angle = nullptr;
    for (auto* candidate :
         panel(window)->findChildren<QDoubleSpinBox*>(QStringLiteral("toolLength_angle")))
        if (candidate->isVisible())
            angle = candidate;
    require(angle != nullptr, "Angle field");
    angle->setValue(10);
    settle();
    require(matches(shown(*viewport), tilted(10)), "10 degrees follow");

    panel(window)->findChild<QPushButton*>(QStringLiteral("toolAcceptButton"))->click();
    settle();
    require(panel(window) == nullptr && session.features().back().name == "Formschräge 1" &&
                matches(volume(session), tilted(10)),
            "OK keeps the draft");
    command(window, "history.undo").trigger();
    settle();
    require(matches(volume(session), 24000), "Undo restores the box");
    std::cout << "PASS M25 draft panel: neutral top, tilt, untilt, tilt, 10 degrees, OK, undo\n";
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
