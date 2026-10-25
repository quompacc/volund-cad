// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "IsolatedSettings.hpp"
#include "UiDriver.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <QAction>
#include <QApplication>
#include <QPushButton>
#include <QStatusBar>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// User report 2026-10-25: on a tray (a ring extruded 10 mm, its floor joined 3 mm without a
// seam, the outer corners rounded) a chamfer or a fillet at the bottom edge was refused: "Diese
// Kante lässt sich nicht wählen". Through the real view: from below, the tool opened with
// nothing selected, the bottom edge clicked.
namespace {
using namespace voelund;
void require(bool condition, const std::string& message) {
    test::require(condition, message.c_str());
}
QAction& command(QWidget& window, const char* id) {
    for (auto* action : window.findChildren<QAction*>())
        if (action->property("commandId").toString() == QLatin1String(id))
            return *action;
    throw std::runtime_error(std::string("Command is missing: ") + id);
}
void settle() {
    for (int i = 0; i < 5; ++i)
        QApplication::processEvents();
}

void tray(application::CadSession& session) {
    document::SketchParameters sketch{document::datumSketchPlane(document::DatumPlane::xy), {}};
    sketch.datum = document::DatumPlane::xy;
    sketch.elements.values = {
        {document::SketchElementId{1}, document::SketchRectangle{-40, -20, 40, 20}},
        {document::SketchElementId{2}, document::SketchRectangle{-36, -16, 36, 16}}};
    sketch.elements.nextId = 3;
    const auto sketchId = *session.createDatumSketchFeature(sketch).feature();
    require(session.selectSketchProfileAt(sketchId, {0, -18, 0}, false), "The ring");
    const std::vector<document::SketchRegionId> ring(session.selectedSketchProfiles().begin(),
                                                     session.selectedSketchProfiles().end());
    require(session.createSketchProfileExtrusion(sketchId, ring, 10).status() ==
                application::CommandStatus::committed,
            "Ring");
    require(session.selectSketchProfileAt(sketchId, {0, 0, 0}, false), "The floor");
    const std::vector<document::SketchRegionId> floor(session.selectedSketchProfiles().begin(),
                                                      session.selectedSketchProfiles().end());
    application::ExtrusionMode join;
    join.operation = document::ExtrusionOperation::join;
    join.target = document::BodyId{sketchId.value()};
    require(session.createSketchProfileExtrusion(sketchId, floor, 3, join).status() ==
                application::CommandStatus::committed,
            "Floor joined");
}

void run(bool chamfer) {
    application::CadSession session;
    session.newDocument();
    desktop::MainWindow window(session);
    window.resize(1300, 860);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    require(viewport != nullptr, "viewport");
    test::render(*viewport);
    tray(session);
    window.refreshAfterExternalChange();
    viewport->setCameraView(viewport::CameraView::bottom);
    test::render(*viewport);
    test::activate(window);
    command(window, chamfer ? "solid.modify.chamfer" : "solid.modify.fillet").trigger();
    settle();
    // The bottom front edge (y = -20, z = 0), a pixel off the silhouette.
    const QPoint edge = viewport::ViewportTestAccess::project(*viewport, 0, -20, 0);
    const QPoint inside = viewport::ViewportTestAccess::project(*viewport, 0, 0, 0);
    const QPoint away = edge + (edge.y() > inside.y() ? QPoint(0, 2) : QPoint(0, -2));
    test::mouse(*viewport, QEvent::MouseMove, away, Qt::NoButton, Qt::NoButton);
    test::mouse(*viewport, QEvent::MouseButtonPress, away, Qt::LeftButton, Qt::LeftButton);
    test::mouse(*viewport, QEvent::MouseButtonRelease, away, Qt::LeftButton, Qt::NoButton);
    test::render(*viewport);
    settle();
    const auto status = window.findChild<QStatusBar*>()->currentMessage();
    auto* accept = window.findChild<QPushButton*>(QStringLiteral("toolAcceptButton"));
    require(!status.contains(QStringLiteral("nicht wählen")) && accept != nullptr &&
                accept->isEnabled(),
            std::string(chamfer ? "Fase" : "Verrunden") +
                ": the tray's bottom edge is taken; status: " + status.toStdString());
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    try {
        run(false);
        run(true);
        std::cout << "PASS tray bottom edge picked for fillet and chamfer in the view\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
