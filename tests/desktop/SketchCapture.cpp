// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

// User report 2026-10-25: the sketch's labels are tiny on a high-DPI screen. By hand, under a
// Wayland session (QT_SCALE_FACTOR for the screen's scale):
//
//   VOELUND_CAPTURE_DIR=/tmp/shots voelund_sketch_capture
//
// Draws a sketch with a dimensioned rectangle and a circle and saves the window as
// sketch-<scale>.png.

#include "IsolatedSettings.hpp"
#include "UiDriver.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QKeyEvent>

#include <iostream>
#include <stdexcept>
#include <string>

namespace {
using namespace voelund;

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
void click(viewport::CadViewport& viewport, double x, double y) {
    const QPoint point = viewport::ViewportTestAccess::project(viewport, x, y, 0);
    test::mouse(viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
    test::mouse(viewport, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
    test::mouse(viewport, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
    test::render(viewport);
    settle();
}
void type(const QString& text) {
    for (const QChar character : text) {
        QWidget* target = QApplication::focusWidget();
        const int code = character == QLatin1Char('\t')   ? static_cast<int>(Qt::Key_Tab)
                         : character == QLatin1Char('\n') ? static_cast<int>(Qt::Key_Return)
                                                          : static_cast<int>(character.unicode());
        QKeyEvent press(QEvent::KeyPress, code, Qt::NoModifier,
                        character == QLatin1Char('\n') ? QString() : QString(character));
        QApplication::sendEvent(target, &press);
        settle();
    }
}

void run(const QString& directory) {
    application::CadSession session;
    session.newDocument();
    desktop::MainWindow window(session);
    window.resize(1500, 950);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    test::render(*viewport);
    viewport->setCameraView(viewport::CameraView::top);
    test::render(*viewport);
    click(*viewport, 30, 30);
    command(window, "solid.create.sketch").trigger();
    settle();
    command(window, "sketch.tool.rectangle").trigger();
    click(*viewport, -30, -15);
    type(QStringLiteral("60\t30\n"));
    command(window, "sketch.tool.circle").trigger();
    click(*viewport, 0, 0);
    type(QStringLiteral("16\n"));
    command(window, "sketch.tool.select").trigger();
    settle();
    test::render(*viewport);
    QDir().mkpath(directory);
    const QString scale = qEnvironmentVariable("QT_SCALE_FACTOR", QStringLiteral("1"));
    const QString file = directory + QStringLiteral("/sketch-") + scale + QStringLiteral(".png");
    if (!window.grab().save(file))
        throw std::runtime_error("capture failed");
    std::cout << "CAPTURED " << file.toStdString() << '\n';
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    const QString directory = qEnvironmentVariable("VOELUND_CAPTURE_DIR");
    if (directory.isEmpty()) {
        std::cout << "SKIP: VOELUND_CAPTURE_DIR names no directory\n";
        return 0;
    }
    try {
        run(directory);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
