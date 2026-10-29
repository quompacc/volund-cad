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
#include <array>
#include <cmath>
#include <iostream>
#include <string>
#include <voelund/application/Actions.hpp>
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

void click(viewport::CadViewport& viewport, double x, double y, double z) {
    test::render(viewport);
    const QPoint point = viewport::ViewportTestAccess::project(viewport, x, y, z);
    test::mouse(viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
    test::mouse(viewport, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
    test::mouse(viewport, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
    test::render(viewport);
    settle();
}
void setLength(QWidget& window, const char* name, double value) {
    QDoubleSpinBox* found = nullptr;
    for (auto* candidate : panel(window)->findChildren<QDoubleSpinBox*>(QLatin1String(name)))
        if (candidate->isVisible())
            found = candidate;
    require(found != nullptr, name);
    found->setValue(value);
    settle();
}

// N13a: a click on the top face of a box 20 x 20 x 10, then "Fläche versetzen": 5 previews
// 6000; "Löschen" of the top face cannot heal and OK waits; back to 5 and OK.
void faceEditFlow(desktop::MainWindow& window, application::CadSession& session,
                  viewport::CadViewport& viewport) {
    session.newDocument();
    static_cast<void>(session.createBoxFeature({20, 20, 10}));
    window.refreshAfterExternalChange();
    viewport.setCameraView(viewport::CameraView::isometric);
    click(viewport, 10, 10, 10);
    command(window, "solid.modify.offset_face").trigger();
    settle();
    require(panel(window) != nullptr, "Fläche versetzen opens on the clicked top face");
    setLength(window, "toolLength_distance", 5);
    require(button(window, "toolAcceptButton").isEnabled() && matches(shown(viewport), 6000),
            "5 up previews 6000");
    choose(window, "toolChoice_mode", 1);
    require(!button(window, "toolAcceptButton").isEnabled(),
            "The top face of a box cannot be deleted and healed; OK waits");
    choose(window, "toolChoice_mode", 0);
    button(window, "toolAcceptButton").click();
    settle();
    require(panel(window) == nullptr && matches(volume(session), 6000), "OK moves the face");
}

// N13b: "Bezugspunkt" without a picked edge offers X/Y/Z; X = 5 makes a point at (5; 0; 0).
// With two more points, "Ebene durch drei Punkte" is offered and OK makes the plane.
void constructionFlow(desktop::MainWindow& window, application::CadSession& session) {
    session.newDocument();
    window.refreshAfterExternalChange();
    command(window, "solid.create.datum_point").trigger();
    settle();
    require(panel(window) != nullptr, "Bezugspunkt opens its panel");
    setLength(window, "toolLength_x", 5);
    button(window, "toolAcceptButton").click();
    settle();
    require(panel(window) == nullptr && session.features().size() == 1 &&
                std::abs(session.datumFrame(session.features().back().id)->originX - 5) < 1e-9,
            "OK makes a construction point at x = 5");
    for (const auto& at : {std::array{0.0, 10.0, 0.0}, std::array{0.0, 0.0, 10.0}})
        require(session.createDatumPoint(application::CreateDatumPointAction{at, {}, {}})
                    .feature()
                    .has_value(),
                "Two more points");
    window.refreshAfterExternalChange();
    QAction& plane = command(window, "solid.create.plane_points");
    require(plane.isEnabled(), "Three points offer a plane through them");
    plane.trigger();
    settle();
    require(button(window, "toolAcceptButton").isEnabled(), "The plane previews");
    button(window, "toolAcceptButton").click();
    settle();
    require(session.features().back().kind == document::FeatureKind::datumPlane,
            "OK makes the plane through the points");
}

// N12b: an open shell 40 x 30 x 20 (wall 2) and a line across its top on y = 15. "Rippe" on
// the selected sketch: upwards it meets nothing and OK waits; "Umgekehrt" previews the rib down
// to the floor (7152 + 1296); OK joins it.
void ribFlow(desktop::MainWindow& window, application::CadSession& session,
             viewport::CadViewport& viewport) {
    session.newDocument();
    const auto box = *session.createBoxFeature({40, 30, 20}).feature();
    for (const auto& face : session.supportFaces(box))
        if (face.plane.normalZ > 0.5) {
            const std::array top{face.origin};
            require(session.createShell(box, top, 2, false).feature().has_value(), "Shell");
        }
    const auto plane = *session
                            .createDatumPlane(document::DatumPlane::xz, std::nullopt, std::nullopt,
                                              -15, document::DatumRotationAxis::none, 0)
                            .feature();
    document::SketchParameters sketch{*session.datumFrame(plane), {}};
    sketch.constructionPlane = plane;
    sketch.elements.values.push_back(
        {document::SketchElementId{1}, document::SketchLine{{0, 20}, {40, 20}}});
    sketch.elements.nextId = 2;
    require(session.createDatumSketchFeature(sketch).feature().has_value() &&
                session.selectFeature(session.features().back().id),
            "Line sketch chosen");
    window.refreshAfterExternalChange();
    command(window, "solid.create.rib").trigger();
    settle();
    require(panel(window) != nullptr && !button(window, "toolAcceptButton").isEnabled(),
            "Upwards the rib meets nothing, OK waits");
    choose(window, "toolChoice_direction", 1);
    require(button(window, "toolAcceptButton").isEnabled() && matches(shown(viewport), 7152 + 1296),
            "Umgekehrt: the rib down to the floor");
    button(window, "toolAcceptButton").click();
    settle();
    require(panel(window) == nullptr && matches(volume(session), 7152 + 1296), "OK joins the rib");
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
    ribFlow(window, session, *viewport);
    faceEditFlow(window, session, *viewport);
    constructionFlow(window, session);
    std::cout << "PASS N12a split panel: construction plane, both/front/back preview, OK, "
                 "timeline reopens split and part; N12b rib; N13a face edit; N13b points, plane"
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
