// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QPushButton>
#include <QRectF>
#include <QVariant>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// W21 (as SolidCraft): a dimension's value dragged with the mouse moves its line; after OK the
// sketch keeps the place (schema 41).
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
void click(viewport::CadViewport& viewport, QPoint point) {
    test::mouse(viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
    test::mouse(viewport, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
    test::mouse(viewport, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
    test::render(viewport);
    settle();
}
void type(const QString& text) {
    for (const QChar character : text) {
        const int code = character == QLatin1Char('\t')   ? static_cast<int>(Qt::Key_Tab)
                         : character == QLatin1Char('\n') ? static_cast<int>(Qt::Key_Return)
                                                          : static_cast<int>(character.unicode());
        QKeyEvent press(QEvent::KeyPress, code, Qt::NoModifier,
                        character == QLatin1Char('\n') ? QString() : QString(character));
        QApplication::sendEvent(QApplication::focusWidget(), &press);
        settle();
    }
}
std::vector<QRectF> valueBoxes(viewport::CadViewport& viewport) {
    std::vector<QRectF> boxes;
    auto* overlay = viewport.findChild<QWidget*>(QStringLiteral("sketchGlyphOverlay"));
    require(overlay != nullptr, "Symbol overlay");
    for (const QVariant& value : overlay->property("valueBoxes").toList())
        boxes.push_back(value.toRectF());
    return boxes;
}
// Drags from a point straight down by 40 pixels in four steps.
void dragDown(viewport::CadViewport& viewport, QPoint from) {
    test::mouse(viewport, QEvent::MouseMove, from, Qt::NoButton, Qt::NoButton);
    test::mouse(viewport, QEvent::MouseButtonPress, from, Qt::LeftButton, Qt::LeftButton);
    for (int step = 1; step <= 4; ++step)
        test::mouse(viewport, QEvent::MouseMove, from + QPoint(0, 10 * step), Qt::NoButton,
                    Qt::LeftButton);
    test::mouse(viewport, QEvent::MouseButtonRelease, from + QPoint(0, 40), Qt::LeftButton,
                Qt::NoButton);
    test::render(viewport);
    settle();
    test::render(viewport);
}

void run() {
    application::CadSession session;
    session.newDocument();
    desktop::MainWindow window(session);
    window.resize(1500, 950);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    require(viewport != nullptr, "viewport");
    viewport->setCameraView(viewport::CameraView::top);
    test::render(*viewport);
    click(*viewport, viewport::ViewportTestAccess::project(*viewport, 30, 30, 0));
    command(window, "solid.create.sketch").trigger();
    settle();
    command(window, "sketch.tool.rectangle").trigger();
    click(*viewport, viewport::ViewportTestAccess::project(*viewport, -30, -15, 0));
    type(QStringLiteral("60\t30\n"));
    command(window, "sketch.tool.select").trigger();
    settle();
    test::render(*viewport);
    const auto boxes = valueBoxes(*viewport);
    require(boxes.size() >= 2, "Two dimension values are drawn: " + std::to_string(boxes.size()));
    // The lowest value is the width below the rectangle; it goes 40 px further down.
    const QRectF lower = *std::ranges::max_element(
        boxes, [](const QRectF& a, const QRectF& b) { return a.center().y() < b.center().y(); });
    dragDown(*viewport, lower.center().toPoint());
    const auto moved = valueBoxes(*viewport);
    require(
        std::ranges::any_of(
            moved, [&](const QRectF& box) { return box.center().y() > lower.center().y() + 25; }),
        "The value follows the drag downwards");
    window.findChild<QPushButton*>(QStringLiteral("toolAcceptButton"))->click();
    settle();
    const auto sketch = session.sketchParameters(session.features().back().id);
    require(std::ranges::any_of(
                sketch.constraints.values,
                [](const auto& constraint) { return constraint.dimensionPosition.has_value(); }),
            "After OK the sketch keeps the dragged place");
    std::cout << "PASS sketch dimension drag: the value moves its line, kept after OK\n";
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
