#include "../McpTestClient.hpp"
#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QToolButton>
#include <iostream>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// AI04c capture helper (not a test): the Claude chip in its states for the review with the
// user (ADR 0012). Writes PNG files into the directory given as the first argument.
namespace {
using namespace voelund;
using test::mcpclient::Client;
using test::mcpclient::structured;

QString directory;
void settle() {
    for (int i = 0; i < 5; ++i)
        QApplication::processEvents();
}
void capture(QWidget& widget, const char* name) {
    settle();
    widget.grab().save(directory + "/ai04c-" + name + ".png");
    std::cout << "captured " << name << '\n';
}
QAction& command(QWidget& window, const char* id) {
    for (auto* action : window.findChildren<QAction*>())
        if (action->property("commandId").toString() == QLatin1String(id))
            return *action;
    throw std::runtime_error(std::string("Command is missing: ") + id);
}

void run() {
    QSettings().setValue(QStringLiteral("mcp/port"), 0);
    application::CadSession session;
    session.newDocument();
    desktop::MainWindow window(session);
    window.resize(1400, 860);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    test::render(*viewport);
    viewport->setCameraView(viewport::CameraView::isometric);
    test::render(*viewport);
    capture(window, "1-off");

    command(window, "assistant.mcp").trigger();
    auto* chip = window.findChild<QToolButton*>(QStringLiteral("assistantChip"));
    auto* menu = chip->menu();
    menu->popup(chip->mapToGlobal(QPoint(0, chip->height())));
    settle();
    capture(*menu, "2-menu");
    menu->hide();

    const auto command_ = window.mcpConnectCommand();
    const auto match = QRegularExpression(QStringLiteral(":(\\d+)/mcp .*Bearer ([0-9a-f]+)"))
                           .match(command_);
    Client client(static_cast<quint16>(match.captured(1).toUInt()), match.captured(2).toLatin1());
    static_cast<void>(client.open());
    const auto context = structured(client.call("get_context", {}));
    const QJsonObject rectangle{{"id", 1},
                                {"type", "rectangle"},
                                {"first", QJsonArray{-40, -20}},
                                {"second", QJsonArray{40, 20}}};
    const QJsonObject bore{{"id", 2}, {"type", "circle"}, {"center", QJsonArray{0, 0}}, {"radius", 8}};
    static_cast<void>(client.call(
        "create_sketch_extrusion",
        {{"requestId", "plate"},
         {"documentId", context.value("documentId")},
         {"stamp", context.value("stamp")},
         {"sketch", QJsonObject{{"plane", "xy"}, {"elements", QJsonArray{rectangle, bore}}}},
         {"distance", QJsonObject{{"value", 10}, {"unit", "mm"}}}}));
    viewport->fitView();
    test::render(*viewport);
    capture(window, "3-change");

    command(window, "view.theme").trigger();
    test::render(*viewport);
    capture(window, "4-dark-connected");

    // AI03b: the Claude panel after a run of the fake claude, the plate selected.
    if (qEnvironmentVariableIsEmpty("VOELUND_CLAUDE_PROGRAM"))
        return;
    command(window, "view.theme").trigger();
    command(window, "assistant.panel").trigger();
    qputenv("FAKE_CLAUDE_SCENARIO", "plate");
    auto* input = window.findChild<QLineEdit*>(QStringLiteral("assistantInput"));
    input->setText(QStringLiteral("Lege einen Parameter A = 8 mm an"));
    auto* send = window.findChild<QPushButton*>(QStringLiteral("assistantSendButton"));
    send->click();
    QElapsedTimer timer;
    timer.start();
    while (!send->isEnabled() && timer.elapsed() < 30000)
        QApplication::processEvents(QEventLoop::AllEvents, 20);
    test::render(*viewport);
    capture(window, "5-panel");
    command(window, "view.theme").trigger();
    test::render(*viewport);
    capture(window, "6-panel-dark");
}

// Sketch tool symbols (user request 2026-10-05), light and dark.
void sketchRail() {
    application::CadSession session;
    session.newDocument();
    desktop::MainWindow window(session);
    window.resize(1400, 860);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    test::render(*viewport);
    viewport->setCameraView(viewport::CameraView::isometric);
    test::render(*viewport);
    const QPoint xy = viewport::ViewportTestAccess::project(*viewport, 30, 30, 0);
    test::mouse(*viewport, QEvent::MouseMove, xy, Qt::NoButton, Qt::NoButton);
    test::mouse(*viewport, QEvent::MouseButtonPress, xy, Qt::LeftButton, Qt::LeftButton);
    test::mouse(*viewport, QEvent::MouseButtonRelease, xy, Qt::LeftButton, Qt::NoButton);
    command(window, "solid.create.sketch").trigger();
    test::render(*viewport);
    capture(window, "7-sketch-rail");
    command(window, "view.theme").trigger();
    test::render(*viewport);
    capture(window, "8-sketch-rail-dark");
    command(window, "view.theme").trigger();

    // Determination (user decision 2026-10-05): a rectangle, its top side fixed, one side as
    // construction geometry.
    const auto clickAt = [&](double x, double y) {
        const QPoint point = viewport::ViewportTestAccess::project(*viewport, x, y, 0);
        test::mouse(*viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
        test::mouse(*viewport, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
        test::mouse(*viewport, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
        test::render(*viewport);
        settle();
    };
    command(window, "sketch.tool.rectangle").trigger();
    clickAt(-30, -15);
    clickAt(30, 15);
    command(window, "sketch.tool.line").trigger();
    clickAt(-30, -15);
    clickAt(30, 15);
    command(window, "sketch.tool.select").trigger();
    clickAt(15, 7.5);
    command(window, "sketch.construction").trigger();
    clickAt(0, 15);
    command(window, "sketch.fixed").trigger();
    clickAt(200, 200);
    test::render(*viewport);
    capture(window, "9-sketch-state");
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    directory = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral(".");
    try {
        run();
        sketchRail();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
