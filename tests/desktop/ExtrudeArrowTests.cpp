// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "IsolatedSettings.hpp"
#include "UiDriver.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <QAction>
#include <QApplication>
#include <QDoubleSpinBox>
#include <QKeyEvent>
#include <QPushButton>
#include <cmath>
#include <iostream>
#include <optional>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// W03 (as in Fusion and SolidCraft): Extrude opens with the distance ready for typing, so
// "25" and Enter make a 25 mm solid without a click; an arrow stands on the profile and
// dragging it lengthens the extrusion in snapped steps.
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
double volume(const application::CadSession& session) {
    GProp_GProps properties;
    BRepGProp::VolumeProperties(session.currentShape(), properties);
    return properties.Mass();
}
QDoubleSpinBox* distanceField(QWidget& window) {
    QDoubleSpinBox* found = nullptr;
    for (auto* field : window.findChildren<QDoubleSpinBox*>(QStringLiteral("toolLength_distance")))
        if (field->isVisible())
            found = field;
    return found;
}

void open(QWidget& window, application::CadSession& session, document::FeatureId sketch) {
    require(session.selectFeature(sketch) &&
                session.selectSketchProfileByElement(document::SketchElementId{1}, false),
            "Region selected");
    static_cast<desktop::MainWindow&>(window).refreshAfterExternalChange();
    command(window, "solid.create.extrude_profiles").trigger();
    settle();
}

void typeRightAway(QWidget& window, application::CadSession& session, document::FeatureId sketch) {
    open(window, session, sketch);
    auto* field = distanceField(window);
    require(field != nullptr && QApplication::focusWidget() != nullptr &&
                QApplication::focusWidget()->parentWidget() == field,
            "The distance takes the keyboard when Extrude opens");
    for (const char* digit : {"2", "5"}) {
        QKeyEvent press(QEvent::KeyPress, Qt::Key_0 + (digit[0] - '0'), Qt::NoModifier,
                        QString::fromLatin1(digit));
        QApplication::sendEvent(QApplication::focusWidget(), &press);
    }
    QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QApplication::sendEvent(QApplication::focusWidget(), &enter);
    settle();
    require(std::abs(volume(session) - 40 * 20 * 25) < 1e-6,
            ("\"25\" and Enter extrude 25 mm: " + std::to_string(volume(session))).c_str());
}

void dragArrow(QWidget& window, viewport::CadViewport& viewport, application::CadSession& session,
               document::FeatureId sketch) {
    open(window, session, sketch);
    test::render(viewport);
    const double before = distanceField(window)->value();
    const QPoint base = viewport::ViewportTestAccess::project(viewport, 20, 10, 0);
    const QPoint up = viewport::ViewportTestAccess::project(viewport, 20, 10, 30);
    const QPointF direction = QPointF(up - base) / std::hypot(up.x() - base.x(), up.y() - base.y());
    std::optional<QPoint> grip;
    for (int step = 0; step <= 150 && !grip; step += 3) {
        const QPoint candidate = base + (direction * step).toPoint();
        test::mouse(viewport, QEvent::MouseMove, candidate, Qt::NoButton, Qt::NoButton);
        if (viewport::ViewportTestAccess::pushPullArrowHovered(viewport))
            grip = candidate;
    }
    require(grip.has_value(), "The extrusion arrow stands on the profile");
    test::mouse(viewport, QEvent::MouseButtonPress, *grip, Qt::LeftButton, Qt::LeftButton);
    test::mouse(viewport, QEvent::MouseMove, *grip + (direction * 40).toPoint(), Qt::NoButton,
                Qt::LeftButton);
    test::mouse(viewport, QEvent::MouseButtonRelease, *grip + (direction * 40).toPoint(),
                Qt::LeftButton, Qt::NoButton);
    settle();
    const double dragged = distanceField(window)->value();
    require(dragged > before && std::abs(dragged - std::round(dragged)) < 1e-9,
            "Dragging the arrow lengthens the extrusion in snapped steps");
    auto* cancel = window.findChild<QPushButton*>(QStringLiteral("toolCancelButton"));
    require(cancel != nullptr, "Cancel");
    cancel->click();
    settle();
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
    document::SketchParameters rectangle{
        document::datumSketchPlane(document::DatumPlane::xy),
        {{{document::SketchElementId{1}, document::SketchRectangle{0, 0, 40, 20}}}, 2}};
    rectangle.datum = document::DatumPlane::xy;
    const auto created = session.createDatumSketchFeature(rectangle);
    require(created.feature().has_value(), "Sketch");
    const auto sketch = *created.feature();
    window.refreshAfterExternalChange();
    test::render(*viewport);
    test::activate(window);
    typeRightAway(window, session, sketch);
    require(session.undo().status() == application::CommandStatus::committed, "Undo");
    window.refreshAfterExternalChange();
    dragArrow(window, *viewport, session, sketch);
    std::cout << "PASS W03 extrude: typed distance right away, arrow on the profile\n";
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
