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
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// M22 (user decision 2026-10-14): a box 40 x 30 x 20. Pick the top face, "Wandstärke": walls 2
// thick with the top open; 3 and back to 2 change the preview; a click on the front face of
// the preview opens it too; "Offene Flächen zurücksetzen" empties the list, a click on the top
// opens it again; OK is one undo step; the timeline reopens it with outward walls; Save/Open.
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
void setThickness(QWidget& window, double value) {
    field<QDoubleSpinBox>(window, QStringLiteral("toolLength_thickness"))->setValue(value);
    settle();
}
void resetFaces(QWidget& window) {
    for (auto* candidate : panel(window)->findChildren<QPushButton*>())
        if (candidate->toolTip() == QStringLiteral("Offene Flächen zurücksetzen")) {
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
    const double topOpen = 24000 - 36 * 26 * 18, bothOpen = 24000 - 36 * 28 * 18;

    click(*viewport, 20, 15, 20);
    QAction& shell = command(window, "solid.modify.shell");
    require(shell.isEnabled(), "Wandstärke offered for a picked face");
    shell.trigger();
    settle();
    require(panel(window) != nullptr && matches(shown(*viewport), topOpen),
            "The panel previews walls 2 thick with the top open");
    setThickness(window, 3);
    require(matches(shown(*viewport), 24000 - 34 * 24 * 17), "3 thick");
    setThickness(window, 2);
    require(matches(shown(*viewport), topOpen), "2 thick again");

    // The front face of the preview opens as well.
    viewport->setCameraView(viewport::CameraView::front);
    test::render(*viewport);
    click(*viewport, 30, 0, 6);
    require(matches(shown(*viewport), bothOpen), "A click on the front opens it too");
    resetFaces(window);
    require(!accept(window).isEnabled(), "Without open faces OK waits");
    viewport->setCameraView(viewport::CameraView::top);
    test::render(*viewport);
    click(*viewport, 20, 15, 20);
    require(accept(window).isEnabled() && matches(shown(*viewport), topOpen),
            "A click on the top opens it again");

    accept(window).click();
    settle();
    require(panel(window) == nullptr && session.features().back().name == "Wandstärke 1" &&
                matches(volume(session), topOpen),
            "OK hollows the box");
    require(session.undo().status() == application::CommandStatus::committed &&
                matches(volume(session), 24000) &&
                session.redo().status() == application::CommandStatus::committed,
            "One undo step");
    window.refreshAfterExternalChange();

    timelineStep(window, 1).click();
    settle();
    require(panel(window) != nullptr, "The timeline reopens the shell");
    field<QComboBox>(window, QStringLiteral("toolChoice_direction"))->setCurrentIndex(1);
    settle();
    accept(window).click();
    settle();
    require(matches(volume(session), 44 * 34 * 22 - 24000), "Outward walls");

    const auto directory = std::filesystem::temp_directory_path() / "voelund-m22-shell-panel";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const auto path = directory / "shell.volund";
    session.saveDocument(path);
    window.openFile(QString::fromStdString(path.string()));
    settle();
    require(session.features().size() == 2 && matches(volume(session), 44 * 34 * 22 - 24000),
            "Open restores the shell");
    std::filesystem::remove_all(directory);
    std::cout << "PASS M22 shell panel: top open, thickness, front over the preview, reset, OK, "
                 "undo, outward, open\n";
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
