// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "IsolatedSettings.hpp"
#include "UiDriver.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QPushButton>
#include <QStatusBar>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>
#include <voelund/features/SketchRegionIdentity.hpp>

// User report 2026-10-23: after one fillet every "Bearbeiten" command stayed greyed out: a click
// into empty space left nothing selected and Verrunden needed a selected body. As in Fusion it
// now stays offered and the edge is clicked in the open panel.
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
QWidget* panel(QWidget& window) {
    auto* found = window.findChild<QWidget*>(QStringLiteral("activeToolPanel"));
    return found != nullptr && found->isVisible() ? found : nullptr;
}

struct Edge final {
    double x, y, z;
};

// Just outside the body, away from its middle, as a user clicks a silhouette edge.
void pick(viewport::CadViewport& viewport, const Edge& edge) {
    test::render(viewport);
    const QPoint middle = viewport::ViewportTestAccess::project(viewport, 10, 10, 5);
    const QPoint at = viewport::ViewportTestAccess::project(viewport, edge.x, edge.y, edge.z);
    const QPointF away(at - middle);
    const double length = std::max(1.0, std::hypot(away.x(), away.y()));
    const QPoint point = at + QPoint(qRound(2 * away.x() / length), qRound(2 * away.y() / length));
    test::mouse(viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
    test::mouse(viewport, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
    test::mouse(viewport, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
    test::render(viewport);
    settle();
}

QString states(QWidget& window) {
    QString text;
    for (const char* id : {"solid.modify.fillet", "solid.modify.chamfer", "solid.modify.push_pull",
                           "solid.modify.shell", "solid.modify.hole", "solid.modify.mirror"})
        text += QString::fromLatin1(id) + "=" + (command(window, id).isEnabled() ? "1 " : "0 ");
    return text;
}

void filletOnce(QWidget& window, viewport::CadViewport& viewport, const Edge& edge, int round) {
    pick(viewport, edge);
    QAction& fillet = command(window, "solid.modify.fillet");
    require(fillet.isEnabled(),
            "Round " + std::to_string(round) +
                ": Verrunden offered for a picked edge: " + states(window).toStdString() +
                " status " + window.findChild<QStatusBar*>()->currentMessage().toStdString());
    fillet.trigger();
    settle();
    require(panel(window) != nullptr, "Round " + std::to_string(round) + ": the panel opens");
    auto* accept = panel(window)->findChild<QPushButton*>(QStringLiteral("toolAcceptButton"));
    require(accept != nullptr && accept->isEnabled(),
            "Round " + std::to_string(round) + ": OK is offered");
    accept->click();
    settle();
    require(panel(window) == nullptr, "Round " + std::to_string(round) + ": the panel closes");
    std::cout << "after round " << round << ": " << states(window).toStdString() << '\n';
}

void clickEmpty(viewport::CadViewport& viewport) {
    const QPoint corner(30, viewport.height() - 30);
    test::mouse(viewport, QEvent::MouseMove, corner, Qt::NoButton, Qt::NoButton);
    test::mouse(viewport, QEvent::MouseButtonPress, corner, Qt::LeftButton, Qt::LeftButton);
    test::mouse(viewport, QEvent::MouseButtonRelease, corner, Qt::LeftButton, Qt::NoButton);
    test::render(viewport);
    settle();
}

void filletFromNothing(QWidget& window, viewport::CadViewport& viewport, const Edge& edge) {
    command(window, "solid.modify.fillet").trigger();
    settle();
    require(panel(window) != nullptr, "Verrunden opens with nothing selected");
    pick(viewport, edge);
    auto* accept = panel(window)->findChild<QPushButton*>(QStringLiteral("toolAcceptButton"));
    require(accept != nullptr && accept->isEnabled(),
            "The edge clicked in the open panel is taken: " +
                window.findChild<QStatusBar*>()->currentMessage().toStdString());
    accept->click();
    settle();
    require(panel(window) == nullptr, "The panel closes");
}

enum class Start { box, plate, panel };

// The user's flow: a rectangle sketch on a base plane, extruded with the panel ("10", Enter).
void extrudeWithPanel(desktop::MainWindow& window, application::CadSession& session) {
    document::SketchParameters square{document::datumSketchPlane(document::DatumPlane::xy), {}};
    square.datum = document::DatumPlane::xy;
    square.elements.values.push_back(
        {document::SketchElementId{1}, document::SketchRectangle{0, 0, 20, 20}});
    square.elements.nextId = 2;
    const auto sketch = session.createDatumSketchFeature(square).feature();
    require(sketch.has_value(), "Sketch");
    window.refreshAfterExternalChange();
    test::activate(window);
    require(session.selectFeature(*sketch) &&
                session.selectSketchProfileByElement(document::SketchElementId{1}, false),
            "Region selected");
    window.refreshAfterExternalChange();
    command(window, "solid.create.extrude_profiles").trigger();
    settle();
    for (const char* key : {"1", "0"}) {
        QKeyEvent press(QEvent::KeyPress, Qt::Key_0 + (key[0] - '0'), Qt::NoModifier,
                        QString::fromLatin1(key));
        QApplication::sendEvent(QApplication::focusWidget(), &press);
    }
    QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QApplication::sendEvent(QApplication::focusWidget(), &enter);
    settle();
    require(panel(window) == nullptr && !session.validBodies().empty(), "Extruded with the panel");
    std::cout << "after extrusion: " << states(window).toStdString() << '\n';
}

void run(Start start) {
    std::cout << "== start " << static_cast<int>(start) << '\n';
    application::CadSession session;
    session.newDocument();
    desktop::MainWindow window(session);
    window.resize(1300, 860);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    require(viewport != nullptr, "viewport");
    test::render(*viewport);
    if (start == Start::panel) {
        extrudeWithPanel(window, session);
    } else if (start == Start::plate) {
        // The user's flow: a rectangle sketch on a base plane, extruded.
        document::SketchParameters square{document::datumSketchPlane(document::DatumPlane::xy), {}};
        square.datum = document::DatumPlane::xy;
        square.elements.values.push_back(
            {document::SketchElementId{1}, document::SketchRectangle{0, 0, 20, 20}});
        square.elements.nextId = 2;
        require(session.createDatumSketchExtrusion(
                           square, features::identifySketchRegions(square.elements.values).ids, 10)
                        .status() == application::CommandStatus::committed,
                "Plate 20 x 20 x 10");
    } else {
        require(session.createBoxFeature({20, 20, 10}).feature().has_value(), "Box");
    }
    const std::size_t base = session.features().size();
    window.refreshAfterExternalChange();
    viewport->setCameraView(viewport::CameraView::isometric);
    test::render(*viewport);
    Bnd_Box box;
    BRepBndLib::Add(session.validBodies().front().shape, box);
    double x0 = 0, y0 = 0, z0 = 0, x1 = 0, y1 = 0, z1 = 0;
    box.Get(x0, y0, z0, x1, y1, z1);
    const double middle = (z0 + z1) / 2;
    filletOnce(window, *viewport, {x1, y1, middle}, 1);
    require(session.features().size() == base + 1, "The first fillet is made");
    // The user's case: a click into empty space clears the selection; Verrunden stays offered,
    // opens, and the edge is clicked in the open panel.
    clickEmpty(*viewport);
    require(command(window, "solid.modify.fillet").isEnabled() &&
                command(window, "solid.modify.chamfer").isEnabled(),
            "With nothing selected Verrunden and Fase stay offered: " +
                states(window).toStdString());
    filletFromNothing(window, *viewport, {x1, y0, middle});
    require(session.features().size() == base + 2, "The second fillet is made");
    // The top edge between both rounded corners: it runs into the earlier fillets.
    filletOnce(window, *viewport, {x1, (y0 + y1) / 2, z1}, 3);
    require(session.features().size() == base + 3, "The third fillet is made");
    clickEmpty(*viewport);
    require(command(window, "solid.modify.fillet").isEnabled() &&
                command(window, "solid.modify.chamfer").isEnabled(),
            "After three fillets and a click into empty space the commands are offered: " +
                states(window).toStdString() + " status " +
                window.findChild<QStatusBar*>()->currentMessage().toStdString());
    std::cout << "PASS fillet twice: the modify commands stay offered\n";
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    try {
        run(Start::box);
        run(Start::plate);
        run(Start::panel);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
