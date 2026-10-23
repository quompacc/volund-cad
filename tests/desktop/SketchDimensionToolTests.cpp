// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "IsolatedSettings.hpp"
#include "UiDriver.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <cmath>
#include <iostream>
#include <numbers>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// W02 (as in Fusion and SolidCraft): the dimension tool D. A click on a line opens its length in
// the field at the pointer, typed over and set with Enter; a click on a circle gives its
// diameter; two lines picked one after the other give their angle. Escape keeps the measured
// value. Ctrl+Enter keeps the sketch with all of them.
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

    void click(double x, double y) {
        const QPoint point = viewport::ViewportTestAccess::project(viewport, x, y, 0);
        test::mouse(viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
        test::mouse(viewport, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
        test::mouse(viewport, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
        test::render(viewport);
        settle();
    }
    QLineEdit* field() const {
        auto* found = viewport.findChild<QLineEdit*>(QStringLiteral("dimensionValueEdit"));
        return found != nullptr && found->isVisible() ? found : nullptr;
    }
    // Types into the open field as a user does: the selected value is replaced.
    void type(const QString& text, int finalKey) {
        auto* edit = field();
        require(edit != nullptr, "The value field is open");
        for (const QChar character : text) {
            QKeyEvent press(QEvent::KeyPress, 0, Qt::NoModifier, QString(character));
            QApplication::sendEvent(edit, &press);
        }
        QKeyEvent finish(QEvent::KeyPress, finalKey, Qt::NoModifier);
        QApplication::sendEvent(edit, &finish);
        settle();
        test::render(viewport);
    }
};

bool hasDimension(const document::SketchParameters& sketch, SketchConstraintKind kind,
                  double value) {
    for (const auto& constraint : sketch.constraints.values) {
        const double actual = kind == SketchConstraintKind::angle
                                  ? constraint.angleValue.value() * 180.0 / std::numbers::pi
                                  : constraint.value;
        if (constraint.kind == kind && std::abs(actual - value) < 1e-6)
            return true;
    }
    return false;
}

void draw(Sketcher& s) {
    command(s.window, "sketch.tool.line").trigger();
    s.click(0, 0);
    s.click(30, 0);
    command(s.window, "sketch.tool.line").trigger();
    s.click(-10, 10);
    s.click(-10, 40);
    command(s.window, "sketch.tool.circle").trigger();
    s.click(60, 20);
    s.click(70, 20);
}

void dimension(Sketcher& s) {
    QAction& tool = command(s.window, "sketch.tool.dimension");
    require(tool.shortcut().toString() == QStringLiteral("D"), "The dimension tool is D");
    tool.trigger();
    s.click(15, 0);
    auto* edit = s.field();
    require(edit != nullptr && edit->selectedText() == edit->text() &&
                std::abs(edit->text().toDouble() - 30) < 1e-6,
            "A click on the line opens its length, selected");
    s.type(QStringLiteral("45"), Qt::Key_Return);
    require(s.field() == nullptr, "Enter sets the length and closes the field");

    s.click(65, 30); // on the circle (centre 60/20, radius 10)
    edit = s.field();
    require(edit != nullptr && std::abs(edit->text().toDouble() - 20) < 1e-6,
            "A circle opens its diameter");
    s.type(QString{}, Qt::Key_Escape);
    require(s.field() == nullptr, "Escape keeps the measured diameter");

    // Two lines one after the other: their angle.
    s.click(-10, 25);
    require(s.field() != nullptr, "The vertical line opens its length first");
    s.click(20, 0);
    edit = s.field();
    require(edit != nullptr && std::abs(edit->text().toDouble() - 90) < 1e-6,
            "The second line turns it into their angle: " + edit->text().toStdString());
    s.type(QStringLiteral("60"), Qt::Key_Return);
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
    draw(s);
    dimension(s);
    test::activate(window);
    QKeyEvent accept(QEvent::KeyPress, Qt::Key_Return, Qt::ControlModifier);
    QApplication::sendEvent(viewport, &accept);
    settle();

    require(session.features().size() == 1, "One sketch");
    const auto sketch = session.sketchParameters(session.features().front().id);
    require(hasDimension(sketch, SketchConstraintKind::distance, 45), "Length 45");
    require(hasDimension(sketch, SketchConstraintKind::diameter, 20), "Diameter 20 kept");
    require(hasDimension(sketch, SketchConstraintKind::angle, 60), "Angle 60");
    std::cout << "PASS W02 dimension tool: line length typed, circle diameter kept, angle of two "
                 "lines\n";
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
