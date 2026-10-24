// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "IsolatedSettings.hpp"
#include "UiDriver.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QKeyEvent>
#include <QLabel>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <variant>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// W13 (user report 2026-10-24, as in FreeCAD): the rectangle tool shows "Abgerundete Ecken
// (U)" and "Rahmen (J)"; with both on, a rectangle drawn from the origin comes with rounded
// corners, an inner frame and every relation, so the panel reports it fully determined.
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
void move(viewport::CadViewport& viewport, double x, double y) {
    const QPoint point = viewport::ViewportTestAccess::project(viewport, x, y, 0);
    test::mouse(viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
    settle();
}
void click(viewport::CadViewport& viewport, double x, double y) {
    const QPoint point = viewport::ViewportTestAccess::project(viewport, x, y, 0);
    test::mouse(viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
    test::mouse(viewport, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
    test::mouse(viewport, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
    test::render(viewport);
    settle();
}
QComboBox* choice(QWidget& window, const char* id) {
    for (auto* box : window.findChildren<QComboBox*>(QStringLiteral("toolChoice_") + id))
        if (box->isVisibleTo(&window))
            return box;
    return nullptr;
}
// The panel's solve status ("Freiheitsgrade: 2", "✓ Vollständig bestimmt", ...).
QString freedom(QWidget& window) {
    for (auto* label : window.findChildren<QLabel*>())
        if (label->isVisible() && (label->text().contains(QStringLiteral("Freiheitsgrade")) ||
                                   label->text().contains(QStringLiteral("bestimmt")) ||
                                   label->text().contains(QStringLiteral("Überflüssig"))))
            return label->text();
    return {};
}

// The options show only with a rectangle tool; U and J switch them on.
void switchOptions(QWidget& window) {
    require(choice(window, "rounded_corners") == nullptr,
            "The rectangle options are hidden while the line tool is on");
    command(window, "sketch.tool.rectangle").trigger();
    settle();
    auto* rounded = choice(window, "rounded_corners");
    auto* frame = choice(window, "frame");
    require(rounded != nullptr && frame != nullptr && rounded->currentIndex() == 0,
            "The rectangle tool shows both options, off");
    command(window, "sketch.rounded_corners").trigger(); // U
    command(window, "sketch.frame").trigger();           // J
    settle();
    require(rounded->currentIndex() == 1 && frame->currentIndex() == 1, "U and J switch them on");
}

// W13b: the corners, then the radius (4 mm off the corner), then the frame (2 mm inside).
void drawInSteps(viewport::CadViewport* viewport) {
    click(*viewport, 0, 0);
    click(*viewport, 40, 20);
    auto* overlay = viewport->findChild<QWidget*>(QStringLiteral("sketchSnapOverlay"));
    move(*viewport, 36, 20);
    require(overlay != nullptr && overlay->property("dynamicFields").toStringList().size() == 1 &&
                overlay->property("dynamicFields")
                    .toStringList()
                    .front()
                    .startsWith(QStringLiteral("Radius")) &&
                overlay->property("stageOutlines").toInt() == 1,
            "After the corners the radius step shows its field and the rounded outline");
    click(*viewport, 36, 20);
    move(*viewport, 20, 2);
    require(overlay->property("dynamicFields")
                    .toStringList()
                    .front()
                    .startsWith(QStringLiteral("Dicke")) &&
                overlay->property("stageOutlines").toInt() == 2,
            "Then the frame step: its thickness and both outlines");
    click(*viewport, 20, 2);
}

void checkKept(const application::CadSession& session);

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
    switchOptions(window);
    drawInSteps(viewport);
    if (const auto directory = qEnvironmentVariable("VOELUND_DIMENSION_ARTIFACT_DIR");
        !directory.isEmpty())
        require(QDir().mkpath(directory) &&
                    viewport->grab().save(directory + "/rectangle-options.png"),
                "capture");
    require(freedom(window).contains(QStringLiteral("Vollständig bestimmt")),
            "Drawn from the origin the rectangle is fully determined: " +
                freedom(window).toStdString());

    QKeyEvent accept(QEvent::KeyPress, Qt::Key_Return, Qt::ControlModifier);
    QApplication::sendEvent(viewport, &accept);
    settle();
    checkKept(session);
}

// Kept: outer and inner rectangle, four sides and four rounded corners each.
void checkKept(const application::CadSession& session) {
    require(session.features().size() == 1, "The sketch is kept");
    const auto sketch = session.sketchParameters(session.features().front().id);
    int lines = 0, arcs = 0;
    for (const auto& element : sketch.elements.values) {
        arcs += std::holds_alternative<document::SketchArc>(element.geometry);
        lines +=
            std::holds_alternative<document::SketchLine>(element.geometry) && !element.construction;
    }
    std::vector<double> radii;
    for (const auto& element : sketch.elements.values)
        if (const auto* arc = std::get_if<document::SketchArc>(&element.geometry))
            radii.push_back(arc->radius);
    const auto count = [&](double wanted) {
        return std::ranges::count_if(radii, [&](double r) { return std::abs(r - wanted) < 1e-3; });
    };
    require(count(4.0) == 4 && count(2.0) == 4,
            "Corners R 4 from the radius step, the frame 2 mm inside with R 2");
    require(lines == 8 && arcs == 8,
            "Outer and inner rectangle, four sides and four rounded corners each: " +
                std::to_string(lines) + " lines, " + std::to_string(arcs) + " arcs");
    std::cout << "PASS W13 rectangle options through the panel: U, J, fully determined\n";
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
