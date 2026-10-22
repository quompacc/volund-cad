// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <QAction>
#include <QApplication>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTimer>
#include <cmath>
#include <iostream>
#include <numbers>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// V01c: the whole first CAD flow through the real UI with real events (ADR 0012 shell):
// empty document -> XY -> rectangle with inner circle -> extrude the ring (volume checked
// analytically) -> change distance -> undo/redo -> save -> new -> open with equal volume.
namespace {
using namespace voelund;
using test::require;

constexpr double halfWidth = 40;
constexpr double halfHeight = 20;
constexpr double radius = 10;

double ringVolume(double distance) {
    return (4 * halfWidth * halfHeight - std::numbers::pi * radius * radius) * distance;
}

double volume(const application::CadSession& session) {
    GProp_GProps properties;
    BRepGProp::VolumeProperties(session.currentShape(), properties);
    return properties.Mass();
}

bool matches(double a, double b) { return std::abs(a - b) < 1e-6 * std::max(1.0, std::abs(b)); }

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

void click(viewport::CadViewport& viewport, double x, double y) {
    const QPoint point = viewport::ViewportTestAccess::project(viewport, x, y, 0);
    test::mouse(viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
    test::mouse(viewport, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
    test::mouse(viewport, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
    test::render(viewport);
    settle();
}

QPushButton& panelButton(QWidget& window, const char* name) {
    auto* panel = window.findChild<QWidget*>(QStringLiteral("activeToolPanel"));
    auto* button = panel == nullptr ? nullptr : panel->findChild<QPushButton*>(QLatin1String(name));
    require(button != nullptr, "Tool panel button missing");
    return *button;
}

void run() {
    QTemporaryDir fixture;
    require(fixture.isValid(), "fixture directory");
    const QString path = fixture.path() + QStringLiteral("/Halterung.volund");

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

    // 1. Pick XY in the viewport and start a sketch on it.
    click(*viewport, 30, 30);
    require(window.statusBar()->currentMessage().startsWith(QStringLiteral("XY")), "Pick XY");
    command(window, "solid.create.sketch").trigger();
    settle();
    test::render(*viewport);

    // 2. Rectangle and inner circle by mouse, then OK.
    command(window, "sketch.tool.rectangle").trigger();
    click(*viewport, -halfWidth, -halfHeight);
    click(*viewport, halfWidth, halfHeight);
    command(window, "sketch.tool.circle").trigger();
    click(*viewport, 0, 0);
    click(*viewport, radius, 0);
    QPushButton& accept = panelButton(window, "toolAcceptButton");
    require(accept.isEnabled(), "The drawn sketch can be confirmed");
    accept.click();
    settle();
    test::render(*viewport);
    require(session.features().size() == 1, "One sketch created");

    // 3. Pick the ring region by its outer edge and extrude it by 8.
    click(*viewport, 0, halfHeight);
    QAction& extrude = command(window, "solid.create.extrude_profiles");
    require(extrude.isEnabled(), "The ring region is selected");
    extrude.trigger();
    settle();
    auto* panel = window.findChild<QWidget*>(QStringLiteral("activeToolPanel"));
    QDoubleSpinBox* distance = nullptr; // earlier panels' fields may await deletion
    for (auto* field : panel->findChildren<QDoubleSpinBox*>(QStringLiteral("toolLength_distance")))
        if (field->isVisible())
            distance = field;
    require(distance != nullptr, "Extrusion distance field");
    distance->setValue(8.0);
    panelButton(window, "toolAcceptButton").click();
    settle();
    require(session.features().size() == 2 && matches(volume(session), ringVolume(8)),
            "Ring body with the analytic volume");

    // 4. Change the distance in the properties card.
    auto* properties = window.findChild<QWidget*>(QStringLiteral("featurePropertiesCard"));
    require(properties != nullptr && properties->isVisible(), "Extrusion properties shown");
    auto* edit = properties->findChild<QDoubleSpinBox*>(QStringLiteral("extrusionDistanceInput"));
    edit->setValue(5.0);
    emit edit->editingFinished();
    settle();
    require(matches(volume(session), ringVolume(5)), "Distance change updates the body");

    // 5. Undo and redo through the header actions.
    command(window, "history.undo").trigger();
    settle();
    require(matches(volume(session), ringVolume(8)), "Undo restores distance 8");
    command(window, "history.redo").trigger();
    settle();
    require(matches(volume(session), ringVolume(5)), "Redo applies distance 5 again");

    // 6. Save, start a new document, open the file again.
    QTimer responder;
    QObject::connect(&responder, &QTimer::timeout, [&] {
        auto* modal = QApplication::activeModalWidget();
        if (auto* dialog = qobject_cast<QFileDialog*>(modal)) {
            dialog->selectFile(path);
            if (auto* name = dialog->findChild<QLineEdit*>(QStringLiteral("fileNameEdit")))
                name->setText(path);
            QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
        } else if (auto* message = qobject_cast<QMessageBox*>(modal)) {
            message
                ->button(message->standardButtons().testFlag(QMessageBox::Discard)
                             ? QMessageBox::Discard
                             : QMessageBox::Ok)
                ->click();
        }
    });
    responder.start(50);
    command(window, "document.save_as").trigger();
    require(!session.isModified() && !session.currentFile().empty(), "Saved through the dialog");
    command(window, "document.new").trigger();
    settle();
    require(session.features().empty(), "New empty document");
    command(window, "document.open").trigger();
    settle();
    responder.stop();
    require(session.features().size() == 2 && matches(volume(session), ringVolume(5)),
            "Reopened document has the same volume");
    std::cout << "PASS V01 flow: plane, sketch, ring extrusion, edit, undo/redo, save/open\n";
}
} // namespace

int main(int argc, char** argv) {
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
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
