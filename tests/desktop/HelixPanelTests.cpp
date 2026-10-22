// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <cmath>
#include <iostream>
#include <numbers>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>
#include <voelund/features/SketchRegionIdentity.hpp>

// M26 (user decision 2026-10-14): a circle of radius 1 on XZ, 10 from Z. Its region picked,
// "Wendel" previews a right-handed spring about Z (pitch 5, height 20, within 1 % of
// 2 pi^2 r^2 R n); the panel offers "Linksgängig"; height 10 halves it; OK keeps "Wendel 1";
// undo.
namespace {
using namespace voelund;
using test::require;
constexpr double pi = std::numbers::pi;

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
double shown(viewport::CadViewport& viewport) {
    test::render(viewport);
    return viewport::ViewportTestAccess::displayedVolume(viewport);
}
bool within(double value, double expected) { return std::abs(value - expected) < 0.01 * expected; }

void run() {
    application::CadSession session;
    desktop::MainWindow window(session);
    window.resize(1300, 860);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    require(viewport != nullptr, "viewport");
    test::render(*viewport);
    session.newDocument();
    document::SketchParameters sketch{document::datumSketchPlane(document::DatumPlane::xz), {}};
    sketch.datum = document::DatumPlane::xz;
    sketch.elements.values.push_back(
        {document::SketchElementId{1}, document::SketchCircle{{10, 0}, 1}});
    sketch.elements.nextId = 2;
    require(session.createDatumSketchFeature(sketch).status() ==
                application::CommandStatus::committed,
            "Sketch");
    const auto profile = session.features().back().id;
    const auto regions = features::identifySketchRegions(sketch.elements.values).ids;
    require(session.selectSketchRegions(profile, regions), "Region picked");
    window.refreshAfterExternalChange();
    test::render(*viewport);

    QAction& helix = command(window, "solid.create.helix");
    require(helix.isEnabled(), "Wendel offered for a picked region");
    helix.trigger();
    settle();
    const double spring = 2 * pi * pi * 10 * 4;
    require(panel(window) != nullptr && within(shown(*viewport), spring),
            "A spring of 4 turns is previewed");
    auto* hand = panel(window)->findChild<QComboBox*>(QStringLiteral("toolChoice_direction"));
    require(hand != nullptr && hand->itemText(1) == QStringLiteral("Linksgängig"),
            "The hand is offered");
    QDoubleSpinBox* height = nullptr;
    for (auto* candidate :
         panel(window)->findChildren<QDoubleSpinBox*>(QStringLiteral("toolLength_height")))
        if (candidate->isVisible())
            height = candidate;
    require(height != nullptr, "Height field");
    height->setValue(10);
    settle();
    require(within(shown(*viewport), spring / 2), "Height 10 halves the spring");
    panel(window)->findChild<QPushButton*>(QStringLiteral("toolAcceptButton"))->click();
    settle();
    require(session.features().back().name == "Wendel 1", "OK keeps Wendel 1");
    command(window, "history.undo").trigger();
    settle();
    require(session.validBodies().empty(), "Undo removes it");
    std::cout << "PASS M26 helix panel: preview, hand, height, OK, undo\n";
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
