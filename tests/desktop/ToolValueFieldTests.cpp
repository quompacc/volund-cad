// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "IsolatedSettings.hpp"
#include "UiDriver.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <QAction>
#include <QApplication>
#include <QDoubleSpinBox>
#include <QKeyEvent>
#include <QLineEdit>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// User report 2026-10-25 (as in Fusion): a tool's main value is typed in a field beside its
// arrow in the view, which takes the keyboard when the tool opens and mirrors the panel's field
// both ways. Fillet "3" Enter gives R 3, chamfer "1.5" Enter a 1.5 mm chamfer.
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
// Just outside the body, away from its middle, as a user clicks a silhouette edge.
void pickEdge(viewport::CadViewport& viewport, double x, double y, double z) {
    test::render(viewport);
    const QPoint middle = viewport::ViewportTestAccess::project(viewport, 10, 10, 5);
    const QPoint at = viewport::ViewportTestAccess::project(viewport, x, y, z);
    const QPointF away(at - middle);
    const double length = std::max(1.0, std::hypot(away.x(), away.y()));
    const QPoint point = at + QPoint(qRound(2 * away.x() / length), qRound(2 * away.y() / length));
    test::mouse(viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
    test::mouse(viewport, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
    test::mouse(viewport, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
    test::render(viewport);
    settle();
}
void type(const QString& text) {
    for (const QChar character : text) {
        QKeyEvent press(QEvent::KeyPress,
                        character.isDigit() ? Qt::Key_0 + character.digitValue() : Qt::Key_Period,
                        Qt::NoModifier, QString(character));
        QApplication::sendEvent(QApplication::focusWidget(), &press);
    }
    QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QApplication::sendEvent(QApplication::focusWidget(), &enter);
    settle();
}
QLineEdit& valueField(QWidget& window, const std::string& tool) {
    auto* field = window.findChild<QLineEdit*>(QStringLiteral("toolValueField"));
    require(field != nullptr && field->isVisible() && QApplication::focusWidget() == field,
            tool + ": the value field beside the arrow takes the keyboard");
    return *field;
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
    require(session.createBoxFeature({20, 20, 10}).feature().has_value(), "Box");
    window.refreshAfterExternalChange();
    viewport->setCameraView(viewport::CameraView::isometric);
    test::render(*viewport);
    test::activate(window);
    Bnd_Box box;
    BRepBndLib::Add(session.validBodies().front().shape, box);
    double x0 = 0, y0 = 0, z0 = 0, x1 = 0, y1 = 0, z1 = 0;
    box.Get(x0, y0, z0, x1, y1, z1);

    pickEdge(*viewport, x1, y1, (z0 + z1) / 2);
    command(window, "solid.modify.fillet").trigger();
    settle();
    auto& field = valueField(window, "Verrunden");
    // The panel's value shows beside the arrow.
    for (auto* panelField :
         window.findChildren<QDoubleSpinBox*>(QStringLiteral("toolLength_radius")))
        if (panelField->isVisible())
            panelField->setValue(2.5);
    require(field.text().startsWith(QLocale().toString(2.5, 'f', 3)),
            "The panel's radius shows in the field: " + field.text().toStdString());
    field.setFocus();
    field.selectAll();
    type(QStringLiteral("3"));
    const auto fillet = session.features().back().id;
    require(session.features().size() == 2 &&
                std::abs(session.filletParameters(fillet).radius - 3) < 1e-9,
            "\"3\" and Enter round the edge with R 3");
    require(!field.isVisible(), "The field goes with the tool");

    pickEdge(*viewport, x1, y0, (z0 + z1) / 2);
    command(window, "solid.modify.chamfer").trigger();
    settle();
    valueField(window, "Fase");
    type(QStringLiteral("1.5"));
    const auto chamfer = session.features().back().id;
    require(session.features().size() == 3 &&
                std::abs(session.chamferParameters(chamfer).distance - 1.5) < 1e-9,
            "\"1.5\" and Enter chamfer the edge by 1.5 mm");
    std::cout << "PASS value field beside the arrow: fillet and chamfer typed directly\n";
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
