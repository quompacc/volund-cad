#include "DiscardPrompt.hpp"
#include "IsolatedSettings.hpp"
#include "UiDriver.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QPushButton>
#include <QStatusBar>
#include <iostream>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// S13 (user decision 2026-10-09): Enter ends a polyline, Ctrl+Enter accepts the sketch; Escape
// steps back through preview, drawing tool and selection and never discards the sketch;
// Cancel asks before it discards changes.
namespace {
using namespace voelund;
void require(bool condition, const std::string& message) { test::require(condition, message.c_str()); }

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
QString key(QWidget& window, viewport::CadViewport& viewport, int code,
            Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    // Window shortcuts need the active window; after the question dialog it may not be yet.
    test::activate(window);
    QKeyEvent press(QEvent::KeyPress, code, modifiers);
    QApplication::sendEvent(&viewport, &press);
    settle();
    test::render(viewport);
    return window.findChild<QStatusBar*>()->currentMessage();
}
bool editing(QWidget& window) {
    auto* panel = window.findChild<QWidget*>(QStringLiteral("activeToolPanel"));
    return panel != nullptr && panel->isVisible();
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

    command(window, "sketch.tool.polyline").trigger();
    click(*viewport, 0, 0);
    click(*viewport, 20, 0);
    click(*viewport, 20, 10);
    const QString ended = key(window, *viewport, Qt::Key_Return);
    require(ended == QStringLiteral("Polylinie beendet") && editing(window),
            "Enter ends the polyline, the sketch stays open: " + ended.toStdString());

    const QString tool = key(window, *viewport, Qt::Key_Escape);
    require(tool == QStringLiteral("Auswahlwerkzeug") &&
                command(window, "sketch.tool.select").isChecked(),
            "Escape returns from the drawing tool to Select: " + tool.toStdString());
    click(*viewport, 10, 0);
    const QString deselected = key(window, *viewport, Qt::Key_Escape);
    require(deselected == QStringLiteral("Auswahl aufgehoben"),
            "Escape then clears the selection: " + deselected.toStdString());
    const QString hint = key(window, *viewport, Qt::Key_Escape);
    require(hint.contains(QStringLiteral("Strg+Enter")) && editing(window),
            "Escape never leaves the sketch: " + hint.toStdString());

    bool asked = false;
    test::answerDiscardQuestion(QStringLiteral("Weiter bearbeiten"), &asked);
    window.findChild<QWidget*>(QStringLiteral("activeToolPanel"))
        ->findChild<QPushButton*>(QStringLiteral("toolCancelButton"))
        ->click();
    settle();
    require(asked && editing(window), "Cancel asks; \"Weiter bearbeiten\" keeps the sketch");

    static_cast<void>(key(window, *viewport, Qt::Key_Return, Qt::ControlModifier));
    require(!editing(window) && session.features().size() == 1 &&
                session.sketchParameters(session.features().front().id).elements.values.size() ==
                    2,
            "Ctrl+Enter accepts the two polyline segments");
    std::cout << "PASS sketch keyboard: Enter, Escape steps, Cancel question, Ctrl+Enter\n";
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
