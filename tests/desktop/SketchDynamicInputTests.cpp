// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "IsolatedSettings.hpp"
#include "UiDriver.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QStringList>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <numbers>
#include <string>
#include <variant>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// D01 (user report 2026-10-23, as in Fusion and SolidCraft): after the first point the fields
// follow the pointer. Typed values go in without a click, Tab moves on, Enter places the
// element; each typed length becomes a dimension in the same undo step as the element. Line
// 40 at 30°, rectangle 50 x 20 (undone and redrawn), circle Ø16, a polyline of two typed
// segments; Ctrl+Enter keeps them all.
namespace {
using namespace voelund;
using document::SketchConstraintKind;
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
    for (int i = 0; i < 3; ++i)
        QApplication::processEvents();
}

struct Sketcher final {
    QWidget& window;
    viewport::CadViewport& viewport;

    void moveTo(double x, double y) {
        const QPoint point = viewport::ViewportTestAccess::project(viewport, x, y, 0);
        test::mouse(viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
        settle();
    }
    void click(double x, double y) {
        moveTo(x, y);
        const QPoint point = viewport::ViewportTestAccess::project(viewport, x, y, 0);
        test::mouse(viewport, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
        test::mouse(viewport, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
        test::render(viewport);
        settle();
    }
    void key(int code, const QString& text = {}, Qt::KeyboardModifiers modifiers = {}) {
        test::activate(window);
        QKeyEvent press(QEvent::KeyPress, code, modifiers, text);
        QApplication::sendEvent(&viewport, &press);
        settle();
        test::render(viewport);
    }
    void type(const QString& text) {
        for (const QChar character : text)
            key(character.isDigit() ? Qt::Key_0 + character.digitValue() : Qt::Key_Period,
                QString(character));
    }
    QStringList fields() const {
        auto* overlay = viewport.findChild<QWidget*>(QStringLiteral("sketchSnapOverlay"));
        return overlay != nullptr ? overlay->property("dynamicFields").toStringList()
                                  : QStringList{};
    }
};

// The dimension of `kind` on the element with the given geometry.
bool dimensioned(const document::SketchParameters& sketch, SketchConstraintKind kind,
                 double value) {
    for (const auto& constraint : sketch.constraints.values)
        if (constraint.kind == kind && !constraint.second &&
            std::abs(constraint.value - value) < 1e-9)
            return true;
    return false;
}

void drawLineAndRectangle(Sketcher& s) {
    command(s.window, "sketch.tool.line").trigger();
    s.click(0, 0);
    s.moveTo(10, 3);
    const auto open = s.fields();
    require(open.size() == 2 && open[0].startsWith(QStringLiteral("Länge")) &&
                open[1].startsWith(QStringLiteral("Winkel")),
            "After the first point the line shows length and angle at the pointer");
    s.type(QStringLiteral("40"));
    require(s.fields()[0] == QStringLiteral("Länge: 40|"),
            "Digits go into the length without a click: " + s.fields()[0].toStdString());
    s.key(Qt::Key_Tab);
    s.type(QStringLiteral("30"));
    s.key(Qt::Key_Return);
    require(s.fields().isEmpty(), "Enter places the line and closes the fields");

    command(s.window, "sketch.tool.rectangle").trigger();
    s.click(60, 0);
    s.moveTo(70, 5);
    s.type(QStringLiteral("50"));
    s.key(Qt::Key_Tab);
    s.type(QStringLiteral("20"));
    s.key(Qt::Key_Return);
    // The rectangle and its two dimensions are one step: Undo removes all of them.
    command(s.window, "sketch.undo_sketch").trigger();
    settle();
    s.click(60, 0);
    s.moveTo(70, 5);
    s.type(QStringLiteral("50"));
    s.key(Qt::Key_Tab);
    s.type(QStringLiteral("20"));
    s.key(Qt::Key_Return);
}

void drawCircleAndPolyline(Sketcher& s) {
    command(s.window, "sketch.tool.circle").trigger();
    s.click(0, 60);
    s.moveTo(5, 62);
    require(s.fields().size() == 1 && s.fields()[0].startsWith(QStringLiteral("Durchmesser")),
            "A circle shows its diameter");
    s.type(QStringLiteral("16"));
    s.key(Qt::Key_Return);

    command(s.window, "sketch.tool.polyline").trigger();
    s.click(100, 0);
    s.moveTo(105, 1);
    s.type(QStringLiteral("10"));
    s.key(Qt::Key_Tab);
    s.type(QStringLiteral("0"));
    s.key(Qt::Key_Return);
    s.moveTo(111, 4);
    require(s.fields().size() == 2, "The next polyline segment shows its fields again");
    s.type(QStringLiteral("10"));
    s.key(Qt::Key_Tab);
    s.type(QStringLiteral("90"));
    s.key(Qt::Key_Return);
    s.key(Qt::Key_Return); // nothing typed: Enter ends the polyline as before

    // W11: an arc of radius 10 and sweep 90, each value followed by Enter.
    command(s.window, "sketch.tool.center_arc").trigger();
    s.click(0, -60);
    s.moveTo(4, -58);
    s.type(QStringLiteral("10"));
    s.key(Qt::Key_Return);
    s.moveTo(-3, -50);
    require(s.fields().size() == 1 && s.fields()[0].startsWith(QStringLiteral("Öffnungswinkel")),
            "After the radius the arc asks for its sweep");
    s.type(QStringLiteral("90"));
    s.key(Qt::Key_Return);
}

// The line typed as 40 mm at 30 degrees from the origin.
bool hasTypedLine(const document::SketchParameters& sketch) {
    return std::ranges::any_of(sketch.elements.values, [](const auto& element) {
        const auto* segment = std::get_if<document::SketchLine>(&element.geometry);
        return segment != nullptr && std::abs(segment->start.x) < 1e-6 &&
               std::abs(segment->start.y) < 1e-6 &&
               std::abs(segment->end.x - 40 * std::cos(std::numbers::pi / 6)) < 1e-6 &&
               std::abs(segment->end.y - 20) < 1e-6;
    });
}

// The arc typed with radius 10.
bool hasTypedArc(const document::SketchParameters& sketch) {
    return std::ranges::any_of(sketch.elements.values, [](const auto& element) {
        const auto* curve = std::get_if<document::SketchArc>(&element.geometry);
        return curve != nullptr && std::abs(curve->radius - 10) < 1e-6;
    });
}

void run() {
    application::CadSession session;
    session.newDocument();
    desktop::MainWindow window(session);
    window.resize(1300, 860);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    require(viewport != nullptr, "viewport");
    test::render(*viewport);
    viewport->setCameraView(viewport::CameraView::top);
    test::render(*viewport);
    Sketcher s{window, *viewport};
    s.click(30, 30);
    command(window, "solid.create.sketch").trigger();
    settle();
    test::render(*viewport);
    drawLineAndRectangle(s);
    drawCircleAndPolyline(s);
    s.key(Qt::Key_Return, {}, Qt::ControlModifier);

    require(session.features().size() == 1, "One sketch");
    const auto sketch = session.sketchParameters(session.features().front().id);
    require(hasTypedLine(sketch), "The line runs 40 mm at 30 degrees from the first point");
    require(hasTypedArc(sketch), "The arc has the typed radius 10");
    require(sketch.elements.values.size() == 9,
            "Line, rectangle (once, after Undo), circle, two segments and the arc: " +
                std::to_string(sketch.elements.values.size()));
    require(dimensioned(sketch, SketchConstraintKind::distance, 40) &&
                dimensioned(sketch, SketchConstraintKind::distance, 50) &&
                dimensioned(sketch, SketchConstraintKind::distance, 20) &&
                dimensioned(sketch, SketchConstraintKind::radius, 8) &&
                dimensioned(sketch, SketchConstraintKind::distance, 10),
            "Typed values became dimensions: 40, 50, 20, R8, 10");
    std::cout << "PASS D01 dynamic input: fields at the pointer, typing without a click, Tab, "
                 "Enter, dimensions in one undo step, polyline chain\n";
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
