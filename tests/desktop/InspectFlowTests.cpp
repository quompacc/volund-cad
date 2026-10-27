// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <iostream>
#include <stdexcept>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// N08: "Überschneidungen" finds the common volume of two overlapping boxes and changes
// nothing; "Messen" takes corners and edge middles: the plate's top front corners lie 40 apart
// (ΔX 40), the top front edge's middle 20 from a corner.
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
QWidget* panel(QWidget& window) {
    auto* found = window.findChild<QWidget*>(QStringLiteral("activeToolPanel"));
    return found != nullptr && found->isVisible() ? found : nullptr;
}
QString guidance(QWidget& window) {
    for (auto* label : panel(window)->findChildren<QLabel*>())
        if (label->objectName() == QLatin1String("toolGuidance"))
            return label->text();
    throw std::runtime_error("Panel guidance missing");
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
QString value(double number) { return QLocale().toString(number, 'f', 3); }
void close(QWidget& window) {
    panel(window)->findChild<QPushButton*>(QStringLiteral("toolAcceptButton"))->click();
    settle();
}

void interference(desktop::MainWindow& window, application::CadSession& session) {
    session.newDocument();
    require(session.createBoxFeature({20, 20, 20}).feature().has_value() &&
                session.createBoxFeature({10, 10, 40}).feature().has_value() &&
                session.validBodies().size() == 2,
            "Two boxes, two bodies");
    window.refreshAfterExternalChange();
    const auto before = session.identifiedSnapshot().snapshot();
    auto& check = command(window, "solid.inspect.interference");
    require(check.isEnabled(), "Überschneidungen offered");
    check.trigger();
    settle();
    // Common part: 10 x 10 x 20.
    require(panel(window) != nullptr &&
                guidance(window).contains(QStringLiteral("Körper 1 ∩ Körper 2: ") + value(2000) +
                                          QStringLiteral(" mm³")),
            "The overlap and its volume: " + guidance(window).toStdString());
    close(window);
    require(panel(window) == nullptr && session.identifiedSnapshot().snapshot() == before,
            "Checking changes nothing");
}

void points(desktop::MainWindow& window, viewport::CadViewport& viewport,
            application::CadSession& session) {
    session.newDocument();
    require(session.createBoxFeature({40, 20, 10}).feature().has_value(), "Plate");
    window.refreshAfterExternalChange();
    viewport.setCameraView(viewport::CameraView::isometric);
    test::render(viewport);
    command(window, "solid.inspect.measure").trigger();
    settle();
    click(viewport, 0, 0, 10);
    click(viewport, 40, 0, 10);
    const QString corners = guidance(window);
    require(corners.contains(QStringLiteral("1. Punkt")) &&
                corners.contains(QStringLiteral("Abstand ") + value(40)) &&
                corners.contains(QStringLiteral("Δ X ") + value(40)),
            "Two corners: 40 apart along X: " + corners.toStdString());
    auto* edgeAs = panel(window)->findChild<QComboBox*>(QStringLiteral("toolChoice_edgeAs"));
    require(edgeAs != nullptr, "Kanten messen als");
    edgeAs->setCurrentIndex(1); // Mittelpunkt
    settle();
    click(viewport, 40, 0, 10); // a third pick starts over with this corner
    click(viewport, 30, 0, 10); // the top front edge, taken at its middle (20, 0, 10)
    const QString middle = guidance(window);
    require(middle.contains(QStringLiteral("2. Punkt")) &&
                middle.contains(QStringLiteral("Abstand ") + value(20)),
            "Corner to the edge middle: 20: " + middle.toStdString());
    close(window);
}

void run() {
    application::CadSession session;
    desktop::MainWindow window(session);
    window.resize(1300, 860);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    require(viewport != nullptr, "viewport");
    test::render(*viewport);
    interference(window, session);
    points(window, *viewport, session);
    std::cout << "PASS N08 interference and point measures through the shell\n";
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
