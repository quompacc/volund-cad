// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QStatusBar>
#include <QToolButton>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// S04a: every drawing tool shows snap markers; the point the cursor snaps to lights up and a
// label names the relation a click creates; Ctrl suppresses snapping. A rectangle drawn from
// its center on the origin stays centered when its size changes. S04: with "Beziehungen:
// Keine" a line still lands on the origin but adds no constraint; "Automatisch" adds them.
namespace {
using namespace voelund;
using test::require;

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

void move(viewport::CadViewport& viewport, double x, double y,
          Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    const QPoint point = viewport::ViewportTestAccess::project(viewport, x, y, 0);
    QMouseEvent event(QEvent::MouseMove, QPointF(point), QPointF(viewport.mapToGlobal(point)),
                      Qt::NoButton, Qt::NoButton, modifiers);
    QApplication::sendEvent(&viewport, &event);
    settle();
}

void click(viewport::CadViewport& viewport, double x, double y) {
    move(viewport, x, y);
    const QPoint point = viewport::ViewportTestAccess::project(viewport, x, y, 0);
    test::mouse(viewport, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
    test::mouse(viewport, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
    test::render(viewport);
    settle();
}

void capture(QWidget& window, const char* name) {
    const QString directory = qEnvironmentVariable("VOELUND_SMOKE_ARTIFACT_DIR");
    if (!directory.isEmpty())
        window.grab().save(directory + "/s04a-" + name + ".png");
}

QDoubleSpinBox* field(QWidget& panel, const QString& label) {
    for (auto* text : panel.findChildren<QLabel*>())
        if (text->text() == label && text->isVisible())
            return qobject_cast<QDoubleSpinBox*>(text->buddy());
    return nullptr;
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
    viewport->setCameraView(viewport::CameraView::isometric);
    test::render(*viewport);
    click(*viewport, 30, 30);
    command(window, "solid.create.sketch").trigger();
    settle();
    test::render(*viewport);
    auto* overlay = viewport->findChild<QWidget*>(QStringLiteral("sketchSnapOverlay"));
    require(overlay != nullptr, "Snap overlay exists");

    // Every drawing tool snaps to the origin and shows it.
    for (const char* tool : {"sketch.tool.line", "sketch.tool.circle", "sketch.tool.center_arc",
                             "sketch.tool.polygon", "sketch.tool.rectangle"}) {
        command(window, tool).trigger();
        settle();
        move(*viewport, 0.4, 0.3);
        if (std::string(tool) == "sketch.tool.circle")
            capture(window, "circle-origin");
        require(overlay->isVisible() && overlay->property("activeSnap").toBool() &&
                    overlay->property("snapLabel").toString() == QStringLiteral("Ursprung"),
                (std::string("Origin marker for ") + tool).c_str());
        move(*viewport, 0.4, 0.3, Qt::ControlModifier);
        require(!overlay->property("activeSnap").toBool(),
                (std::string("Ctrl suppresses snapping for ") + tool).c_str());
    }
    command(window, "sketch.tool.select").trigger();
    settle();
    move(*viewport, 0.4, 0.3);
    require(!overlay->property("activeSnap").toBool(), "Selecting does not snap");

    // The relation a click creates is named in advance.
    command(window, "sketch.tool.line").trigger();
    settle();
    click(*viewport, 0, 0);
    move(*viewport, 30, 0.6);
    capture(window, "line-horizontal");
    require(overlay->property("snapLabel").toString() == QStringLiteral("Waagrecht"),
            "A nearly horizontal line announces Waagrecht");
    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(viewport, &escape);
    settle();

    // Rectangle from its center on the origin, then change the width.
    QAction& centered = command(window, "sketch.tool.center_rectangle");
    require(centered.shortcut() == QKeySequence(QStringLiteral("Shift+R")), "Shortcut Shift+R");
    centered.trigger();
    settle();
    click(*viewport, 0, 0);
    move(*viewport, 20, 10);
    capture(window, "center-rectangle");
    click(*viewport, 20, 10);
    auto* panel = window.findChild<QWidget*>(QStringLiteral("activeToolPanel"));
    QDoubleSpinBox* length = field(*panel, QStringLiteral("Linienlänge"));
    require(length != nullptr, "The new rectangle offers its edge length");
    length->setValue(60.0);
    settle();
    panel->findChild<QPushButton*>(QStringLiteral("toolAcceptButton"))->click();
    settle();
    require(session.features().size() == 1, "Sketch created");
    double minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
    int construction = 0;
    for (const auto& element : session.sketchParameters(session.features()[0].id).elements.values) {
        const auto* line = std::get_if<document::SketchLine>(&element.geometry);
        require(line != nullptr, "Only lines");
        if (element.construction) {
            ++construction;
            continue;
        }
        for (const auto& point : {line->start, line->end}) {
            minX = std::min(minX, point.x);
            maxX = std::max(maxX, point.x);
            minY = std::min(minY, point.y);
            maxY = std::max(maxY, point.y);
        }
    }
    require(construction == 2, "Two construction diagonals");
    require(std::abs(minX + maxX) < 1e-6 && std::abs(minY + maxY) < 1e-6 &&
                std::abs((maxX - minX) - 60) < 1e-6 && std::abs((maxY - minY) - 20) < 1e-6,
            "The rectangle stays centered on the origin after the width change");
    std::cout << "PASS snap markers for every drawing tool and centered rectangle\n";
}
void relationsSwitch() {
    application::CadSession session;
    session.newDocument();
    desktop::MainWindow window(session);
    window.resize(1300, 860);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    require(viewport != nullptr, "viewport");
    test::render(*viewport);
    viewport->setCameraView(viewport::CameraView::isometric);
    test::render(*viewport);
    click(*viewport, 30, 30);
    command(window, "solid.create.sketch").trigger();
    settle();
    test::render(*viewport);
    auto* overlay = viewport->findChild<QWidget*>(QStringLiteral("sketchSnapOverlay"));
    auto* panel = window.findChild<QWidget*>(QStringLiteral("activeToolPanel"));
    // VP03: the view switches wait behind the details.
    window.findChild<QToolButton*>(QStringLiteral("sketchDetailsToggle"))->setChecked(true);
    settle();
    QComboBox* relations = nullptr;
    for (auto* box : panel->findChildren<QComboBox*>(QStringLiteral("toolChoice_relations")))
        if (box->isVisible())
            relations = box;
    require(relations != nullptr && relations->currentIndex() == 0,
            "The panel offers Beziehungen, automatic by default");

    relations->setCurrentIndex(1); // Keine
    settle();
    command(window, "sketch.tool.line").trigger();
    settle();
    click(*viewport, 0, 0);
    move(*viewport, 30, 0.6);
    require(overlay->property("snapLabel").toString() != QStringLiteral("Waagrecht"),
            "Without automatic relations no relation is announced");
    click(*viewport, 30, 0.6);
    relations->setCurrentIndex(0); // Automatisch
    settle();
    click(*viewport, 0, 20);
    click(*viewport, 30, 20.6);
    panel->findChild<QPushButton*>(QStringLiteral("toolAcceptButton"))->click();
    settle();
    require(session.features().size() == 1, "Sketch created");
    const auto sketch = session.sketchParameters(session.features()[0].id);
    require(sketch.elements.values.size() == 2, "Two lines");
    const auto first = sketch.elements.values[0].id;
    const auto& line = std::get<document::SketchLine>(sketch.elements.values[0].geometry);
    require(std::hypot(line.start.x, line.start.y) < 1e-9,
            "The first line still lands on the origin");
    require(!sketch.constraints.values.empty(), "The second line has its relations");
    for (const auto& constraint : sketch.constraints.values)
        require(constraint.first.element != first &&
                    (!constraint.second || constraint.second->element != first),
                "The line drawn with Keine has no constraint");
    std::cout << "PASS Beziehungen: Keine binds nothing, Automatisch binds\n";
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    try {
        run();
        relationsSwitch();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
