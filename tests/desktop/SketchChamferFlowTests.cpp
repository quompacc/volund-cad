// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <QAction>
#include <QApplication>
#include <QDoubleSpinBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QStatusBar>
#include <iostream>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// S19 (user decision 2026-10-14): "Fasen" in the sketch panel. Two sides of a rectangle picked
// with Ctrl-click; a distance too large is refused with its reason; 3 cuts the corner; a second
// corner right after with 2; OK stores six lines.
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
    for (int i = 0; i < 3; ++i)
        QApplication::processEvents();
}
void click(viewport::CadViewport& viewport, double x, double y,
           Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    const QPoint point = viewport::ViewportTestAccess::project(viewport, x, y, 0);
    test::mouse(viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
    for (const auto type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease}) {
        QMouseEvent event(
            type, QPointF(point), QPointF(viewport.mapToGlobal(point)), Qt::LeftButton,
            type == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton, modifiers);
        QApplication::sendEvent(&viewport, &event);
    }
    test::render(viewport);
    settle();
}
QString run(QWidget& window, viewport::CadViewport& viewport, const char* id) {
    command(window, id).trigger();
    settle();
    test::render(viewport);
    return window.findChild<QStatusBar*>()->currentMessage();
}
void pickSides(QWidget& window, viewport::CadViewport& viewport, double x1, double y1, double x2,
               double y2) {
    command(window, "sketch.tool.select").trigger();
    settle();
    click(viewport, x1, y1);
    click(viewport, x2, y2, Qt::ControlModifier);
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

    click(*viewport, 30, 30);
    command(window, "solid.create.sketch").trigger();
    settle();
    test::render(*viewport);
    command(window, "sketch.tool.rectangle").trigger();
    click(*viewport, 0, 0);
    click(*viewport, 20, 10);

    pickSides(window, *viewport, 10, 0, 20, 5);
    auto* distance =
        window.findChild<QDoubleSpinBox*>(QStringLiteral("toolLength_chamfer_distance"));
    require(distance != nullptr && command(window, "sketch.chamfer").isEnabled(),
            "Fasen ready for two sides");
    distance->setValue(30);
    const QString tooLarge = run(window, *viewport, "sketch.chamfer");
    require(tooLarge.startsWith(QStringLiteral("Fasen: ")) &&
                tooLarge.contains(QStringLiteral("zu groß")),
            "Too large: refused with the reason, got " + tooLarge.toStdString());
    distance->setValue(3);
    const QString cut = run(window, *viewport, "sketch.chamfer");
    require(cut.startsWith(QStringLiteral("Skizzenoperation abgeschlossen")),
            "3 cuts the corner, got " + cut.toStdString());

    pickSides(window, *viewport, 10, 10, 0, 5);
    distance->setValue(2);
    const QString second = run(window, *viewport, "sketch.chamfer");
    require(second.startsWith(QStringLiteral("Skizzenoperation abgeschlossen")),
            "A second corner right after, got " + second.toStdString());

    auto* panel = window.findChild<QWidget*>(QStringLiteral("activeToolPanel"));
    panel->findChild<QPushButton*>(QStringLiteral("toolAcceptButton"))->click();
    settle();
    int lines = 0;
    for (const auto& element :
         session.sketchParameters(session.features().back().id).elements.values)
        lines +=
            std::holds_alternative<document::SketchLine>(element.geometry) && !element.construction;
    require(lines == 6, "OK stores six lines, got " + std::to_string(lines));
    std::cout << "PASS S19 sketch chamfer flow: refused, two corners, OK\n";
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
