// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QPushButton>
#include <QToolButton>
#include <cmath>
#include <iostream>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// N12a: a box 20 x 20 x 10 with the construction plane x = 8. "Teilen" on the selected box
// cuts at that plane: both sides preview as 4000, the front only as 2400, the back only as
// 1600; OK with both sides makes two bodies; the timeline reopens the split, and so does its
// part "Teil 2".
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
QComboBox* choice(QWidget& window, const char* name) {
    QComboBox* found = nullptr;
    for (auto* candidate : panel(window)->findChildren<QComboBox*>(QLatin1String(name)))
        if (candidate->isVisible())
            found = candidate;
    require(found != nullptr, name);
    return found;
}
void choose(QWidget& window, const char* name, int index) {
    choice(window, name)->setCurrentIndex(index);
    settle();
}
QToolButton& timelineStep(QWidget& window, int index) {
    int seen = 0;
    for (auto* step : window.findChildren<QToolButton*>(QStringLiteral("timelineStep")))
        if (step->isVisible() && seen++ == index)
            return *step;
    throw std::runtime_error("Timeline step missing");
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
    const auto box = *session.createBoxFeature({20, 20, 10}).feature();
    require(session.createDatumPlane(document::DatumPlane::yz, std::nullopt, std::nullopt, 8,
                                     document::DatumRotationAxis::none, 0)
                    .feature()
                    .has_value() &&
                session.selectFeature(box),
            "Box and construction plane x = 8");
    window.refreshAfterExternalChange();
    QAction& split = command(window, "solid.modify.split");
    require(split.isEnabled(), "Teilen offered for the selected box");
    split.trigger();
    settle();
    require(panel(window) != nullptr && choice(window, "toolChoice_plane")->currentIndex() == 3 &&
                button(window, "toolAcceptButton").isEnabled() && matches(shown(*viewport), 4000),
            "The construction plane cuts; both sides preview");
    choose(window, "toolChoice_keep", 1);
    require(matches(shown(*viewport), 2400), "The front only: 2400");
    choose(window, "toolChoice_keep", 2);
    require(matches(shown(*viewport), 1600), "The back only: 1600");
    choose(window, "toolChoice_keep", 0);
    button(window, "toolAcceptButton").click();
    settle();
    require(panel(window) == nullptr && session.validBodies().size() == 2 &&
                matches(volume(session), 4000) && session.features().size() == 4,
            "OK: two bodies, the split and its part");
    window.refreshAfterExternalChange();
    timelineStep(window, 2).click();
    settle();
    require(panel(window) != nullptr && choice(window, "toolChoice_keep")->currentIndex() == 0,
            "The timeline reopens the split");
    button(window, "toolCancelButton").click();
    settle();
    require(session.selectFeature(session.features()[3].id), "Teil 2 chosen");
    window.refreshAfterExternalChange();
    command(window, "solid.modify.edit_sketch").trigger();
    settle();
    require(panel(window) != nullptr, "Its part reopens the split too");
    button(window, "toolCancelButton").click();
    settle();
    std::cout << "PASS N12a split panel: construction plane, both/front/back preview, OK, "
                 "timeline reopens split and part"
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
