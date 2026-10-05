#include "../McpTestClient.hpp"
#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <QAction>
#include <QApplication>
#include <QMenu>
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
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    directory = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral(".");
    try {
        run();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
