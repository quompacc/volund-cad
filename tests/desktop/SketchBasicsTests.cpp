// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QSettings>
#include <QStatusBar>
#include <QToolButton>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// VP01-VP03 (user review 2026-10-30): the first minutes with a sketch, as a user does them: draw
// a rectangle, dimension it, drag it, measure between two corners, and find out what is still
// missing. With VOELUND_SKETCH_SHOTS set it saves what it sees, to be looked at.
namespace {
using namespace voelund;
void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
QAction& command(QWidget& window, const char* id) {
    for (auto* action : window.findChildren<QAction*>())
        if (action->property("commandId").toString() == QLatin1String(id))
            return *action;
    throw std::runtime_error(std::string("Command is missing: ") + id);
}
void settle() {
    for (int i = 0; i < 6; ++i)
        QApplication::processEvents();
}
void shot(QWidget& window, viewport::CadViewport& viewport, const char* name) {
    const QString directory = qEnvironmentVariable("VOELUND_SKETCH_SHOTS");
    if (directory.isEmpty())
        return;
    test::render(viewport);
    settle();
    QDir().mkpath(directory);
    window.grab().save(directory + QLatin1Char('/') + QLatin1String(name) + ".png");
}
QPoint at(viewport::CadViewport& viewport, double x, double y) {
    return viewport::ViewportTestAccess::project(viewport, x, y, 0);
}
void move(viewport::CadViewport& viewport, double x, double y) {
    test::mouse(viewport, QEvent::MouseMove, at(viewport, x, y), Qt::NoButton, Qt::NoButton);
    test::render(viewport);
    settle();
}
void click(viewport::CadViewport& viewport, double x, double y) {
    move(viewport, x, y);
    test::mouse(viewport, QEvent::MouseButtonPress, at(viewport, x, y), Qt::LeftButton,
                Qt::LeftButton);
    test::mouse(viewport, QEvent::MouseButtonRelease, at(viewport, x, y), Qt::LeftButton,
                Qt::NoButton);
    test::render(viewport);
    settle();
}
void drag(viewport::CadViewport& viewport, double fromX, double fromY, double toX, double toY) {
    move(viewport, fromX, fromY);
    test::mouse(viewport, QEvent::MouseButtonPress, at(viewport, fromX, fromY), Qt::LeftButton,
                Qt::LeftButton);
    for (int step = 1; step <= 4; ++step) {
        const double part = step / 4.0;
        test::mouse(viewport, QEvent::MouseMove,
                    at(viewport, fromX + (toX - fromX) * part, fromY + (toY - fromY) * part),
                    Qt::LeftButton, Qt::LeftButton);
        test::render(viewport);
        settle();
    }
    test::mouse(viewport, QEvent::MouseButtonRelease, at(viewport, toX, toY), Qt::LeftButton,
                Qt::NoButton);
    test::render(viewport);
    settle();
}
QString status(QWidget& window) { return window.findChild<QStatusBar*>()->currentMessage(); }
void typeValue(QWidget& window, const char* value) {
    auto* field = window.findChild<QLineEdit*>(QStringLiteral("dimensionValueEdit"));
    require(field != nullptr && field->isVisible(),
            std::string("The value field opens for ") + value);
    field->setText(QLatin1String(value));
    QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QApplication::sendEvent(field, &enter);
    settle();
}
double lowestX(const document::SketchParameters& sketch) {
    double lowest = 1.0e9;
    for (const auto& element : sketch.elements.values)
        if (const auto* line = std::get_if<document::SketchLine>(&element.geometry))
            lowest = std::min({lowest, line->start.x, line->end.x});
    return lowest;
}
bool hasKind(const document::SketchParameters& sketch, document::SketchConstraintKind kind,
             double value) {
    for (const auto& constraint : sketch.constraints.values)
        if (constraint.kind == kind && std::abs(std::abs(constraint.value) - value) < 1.0e-6)
            return true;
    return false;
}
bool guidanceSays(QWidget& window, const QString& text) {
    for (auto* label : window.findChildren<QLabel*>())
        if (label->isVisible() && label->text().contains(text))
            return true;
    return false;
}

void pickAndDimension(QWidget& window, viewport::CadViewport& viewport) {
    // Hovering a corner lights it up before it is clicked.
    move(viewport, -30, 15);
    bool lit = false;
    for (auto* child : viewport.findChildren<QWidget*>())
        lit = lit || child->property("hoveredPoint").isValid();
    require(lit, "The corner under the pointer lights up");
    shot(window, viewport, "3-corner-hover");

    // The panel says what is missing, in words.
    require(guidanceSays(window, QStringLiteral("nicht festgelegt")),
            "The panel explains what blue means");

    // A click in the middle of a line takes the line, not one of its ends.
    click(viewport, 0, 15);
    require(status(window).startsWith(QStringLiteral("Element gewählt")),
            "A click on a line selects the line, got " + status(window).toStdString());
    // A click on a corner takes the corner.
    click(viewport, -30, 15);
    require(status(window).startsWith(QStringLiteral("Punkt gewählt")),
            "A click on a corner selects the point, got " + status(window).toStdString());
    shot(window, viewport, "4-corner-selected");

    // Dimension between two corners on one edge: direct, no question.
    command(window, "sketch.tool.dimension").trigger();
    settle();
    click(viewport, -30, 15);
    require(status(window).contains(QStringLiteral("zweiten Punkt")),
            "The first corner waits for the second, got " + status(window).toStdString());
    click(viewport, 30, 15);
    shot(window, viewport, "5-dimension-two-corners");
    typeValue(window, "60");

    // Dimension between two corners across: the tool asks which distance, never an angle.
    click(viewport, -30, 15);
    click(viewport, 30, -15);
    auto* menu = window.findChild<QMenu*>(QStringLiteral("dimensionAxisMenu"));
    require(menu != nullptr && menu->actions().size() == 3,
            "Two corners across ask which distance is meant");
    shot(window, viewport, "6-which-distance");
    menu->actions().at(1)->trigger(); // senkrecht
    settle();
    typeValue(window, "30");

    // Fully dimensioned in size: only the position is open now, and the card says so.
    command(window, "sketch.tool.select").trigger();
    settle();
    require(guidanceSays(window, QStringLiteral("nur noch die Lage")),
            "The card names the missing position");
    shot(window, viewport, "7-only-position-missing");
}

void dragIt(QWidget& window, viewport::CadViewport& viewport) {
    // Dragging a line of the dimensioned rectangle moves all of it, its size stays.
    drag(viewport, 0, 15, 25, 35);
    require(status(window).startsWith(QStringLiteral("Skizzenelement verschoben")),
            "The drag moved it, got " + status(window).toStdString());
    shot(window, viewport, "8-dragged-by-line");

    // So does dragging a corner.
    drag(viewport, 55, 35, 35, 25);
    require(status(window).startsWith(QStringLiteral("Skizzenelement verschoben")),
            "The corner drag moved it, got " + status(window).toStdString());
    shot(window, viewport, "9-dragged-by-corner");

    // The card opens its details on request.
    window.findChild<QToolButton*>(QStringLiteral("sketchDetailsToggle"))->setChecked(true);
    settle();
    shot(window, viewport, "10-details-open");
}

// The user's case: a sketch on the top of a body, a smaller rectangle in it, and a dimension
// between a corner of it and a corner of the body's outline.
void sketchOnBody(desktop::MainWindow& window, application::CadSession& session,
                  viewport::CadViewport& viewport, document::FeatureId sketch) {
    require(session.selectFeature(sketch) &&
                session.selectSketchProfileByElement(document::SketchElementId{1}, false),
            "The rectangle's region is selected");
    window.refreshAfterExternalChange();
    command(window, "solid.create.extrude_profiles").trigger();
    settle();
    for (auto* button : window.findChildren<QPushButton*>())
        if (button->isVisible() && button->text() == QStringLiteral("OK")) {
            button->click();
            break;
        }
    settle();
    test::render(viewport);
    const auto before = session.features().size();
    shot(window, viewport, "12-body");
    // The rectangle lies from (-25, -5) to (35, 25); the extrusion is 10 high.
    const QPoint top = viewport::ViewportTestAccess::project(viewport, 5, 10, 10);
    test::mouse(viewport, QEvent::MouseMove, top, Qt::NoButton, Qt::NoButton);
    test::mouse(viewport, QEvent::MouseButtonPress, top, Qt::LeftButton, Qt::LeftButton);
    test::mouse(viewport, QEvent::MouseButtonRelease, top, Qt::LeftButton, Qt::NoButton);
    test::render(viewport);
    settle();
    command(window, "solid.create.sketch").trigger();
    settle();
    test::render(viewport);
    settle();
    shot(window, viewport, "13-sketch-on-body");
    command(window, "sketch.tool.rectangle").trigger();
    settle();
    click(viewport, -10, 2);
    click(viewport, 20, 18);
    command(window, "sketch.tool.select").trigger();
    settle();
    shot(window, viewport, "14-rectangle-on-body");
    // Two edges of the body become sketch geometry, as the user had them.
    command(window, "sketch.project").trigger();
    settle();
    click(viewport, 35, 10);
    click(viewport, 5, -5);
    command(window, "sketch.tool.select").trigger();
    settle();
    shot(window, viewport, "14b-edges-projected");
    command(window, "sketch.tool.dimension").trigger();
    settle();
    click(viewport, -10, 18); // a corner of the new rectangle
    require(status(window).contains(QStringLiteral("zweiten Punkt")),
            "A corner on the body is a point, got " + status(window).toStdString());
    shot(window, viewport, "15-first-corner-on-body");
    click(viewport, 35, -5); // the corner of the body's outline (the others lie under cards)
    shot(window, viewport, "16-second-corner-on-body");
    require(status(window).startsWith(QStringLiteral("Welchen Abstand")),
            "Corners across on a body ask which distance, got " + status(window).toStdString());
    window.findChild<QMenu*>(QStringLiteral("dimensionAxisMenu"))->actions().at(0)->trigger();
    settle();
    typeValue(window, "20");
    shot(window, viewport, "17-dimension-on-body");
    require(session.features().size() == before, "The sketch is still open");
}

void run() {
    if (!qEnvironmentVariable("VOELUND_SKETCH_SHOTS").isEmpty())
        QSettings().setValue(QStringLiteral("ui/theme"), QStringLiteral("dark")); // as the user's
    application::CadSession session;
    session.newDocument();
    desktop::MainWindow window(session);
    window.resize(1700, 1000);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    require(viewport != nullptr, "viewport");
    test::render(*viewport);
    settle();
    shot(window, *viewport, "1-start");
    viewport->setCameraView(viewport::CameraView::top);
    test::render(*viewport);
    click(*viewport, 30, 30);
    command(window, "solid.create.sketch").trigger();
    settle();
    command(window, "sketch.tool.rectangle").trigger();
    settle();
    click(*viewport, -30, -15);
    click(*viewport, 30, 15);
    command(window, "sketch.tool.select").trigger();
    settle();
    shot(window, *viewport, "2-rectangle");

    pickAndDimension(window, *viewport);
    dragIt(window, *viewport);

    command(window, "sketch.tool.select").trigger();
    settle();
    for (auto* button : window.findChildren<QPushButton*>())
        if (button->isVisible() && button->text() == QStringLiteral("OK")) {
            button->click();
            break;
        }
    settle();
    shot(window, *viewport, "11-accepted");
    require(!session.features().empty(),
            "OK adds the sketch to the document, status: " + status(window).toStdString());
    const auto sketch = session.sketchParameters(session.features().front().id);
    require(hasKind(sketch, document::SketchConstraintKind::pointDistance, 60.0),
            "The distance of the two corners is in the sketch");
    require(hasKind(sketch, document::SketchConstraintKind::pointDistanceY, 30.0),
            "The chosen vertical distance is in the sketch");
    // Moved by (25, 20) along the line, then (-20, -10) at the corner; the size stays.
    require(std::abs(lowestX(sketch) - -25.0) < 1.0e-6,
            "The rectangle followed both drags: its left side is at " +
                std::to_string(lowestX(sketch)));
    sketchOnBody(window, session, *viewport, session.features().front().id);
    std::cout << "PASS sketch basics: pick, dimension, drag, what is missing\n";
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
