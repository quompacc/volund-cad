#include "../McpTestClient.hpp"
#include "DiscardPrompt.hpp"
#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QStatusBar>
#include <QTcpSocket>
#include <QToolButton>
#include <QTreeWidget>
#include <QUrl>
#include <iostream>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// AI04c: the running shell with its MCP server. The header chip switches the access, copies the
// connect command and shows off/ready/connected. A change over MCP appears in the model tree
// (marked), the status bar and the chip, and is one undo step for the user. A sketch the
// user is drawing stays open; an older draft never overwrites Claude's change.
namespace {
using namespace voelund;
using test::mcpclient::Client;
using test::mcpclient::structured;

void check(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
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
void click(viewport::CadViewport& viewport, double x, double y) {
    const QPoint point = viewport::ViewportTestAccess::project(viewport, x, y, 0);
    test::mouse(viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
    test::mouse(viewport, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
    test::mouse(viewport, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
    test::render(viewport);
    settle();
}
QPushButton* acceptButton(QWidget& window) {
    auto* panel = window.findChild<QWidget*>(QStringLiteral("activeToolPanel"));
    return panel == nullptr ? nullptr
                            : panel->findChild<QPushButton*>(QStringLiteral("toolAcceptButton"));
}

constexpr const char* token = "shell-test-token-0123456789abcdef";

void run() {
    // The access was switched on in an earlier run; port 0 picks a free port.
    QSettings settings;
    settings.setValue(QStringLiteral("mcp/enabled"), true);
    settings.setValue(QStringLiteral("mcp/port"), 0);
    settings.setValue(QStringLiteral("mcp/token"), QByteArray(token));
    application::CadSession session;
    session.newDocument();
    desktop::MainWindow window(session);
    window.resize(1300, 860);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    check(viewport != nullptr, "viewport");
    test::render(*viewport);
    viewport->setCameraView(viewport::CameraView::isometric);
    test::render(*viewport);

    auto* chip = window.findChild<QToolButton*>(QStringLiteral("assistantChip"));
    check(chip != nullptr && chip->property("state") == QStringLiteral("ready") &&
              command(window, "assistant.mcp").isChecked(),
          "The remembered access is on after the start");
    command(window, "assistant.mcp_copy_command").trigger();
    const auto copied = QApplication::clipboard()->text();
    const auto match =
        QRegularExpression(QStringLiteral("^claude mcp add --transport http volund "
                                          "http://127\\.0\\.0\\.1:(\\d+)/mcp --header "
                                          "\"Authorization: Bearer ([0-9a-z-]+)\"$"))
            .match(copied);
    check(match.hasMatch() && match.captured(2) == QLatin1String(token),
          "The copied command connects Claude Code: " + copied.toStdString());
    Client client(static_cast<quint16>(match.captured(1).toUInt()), token);
    static_cast<void>(client.open());
    settle();
    check(chip->property("state") == QStringLiteral("connected"), "The chip shows the client");

    // The user starts a sketch on XY and draws a rectangle.
    click(*viewport, 30, 30);
    command(window, "solid.create.sketch").trigger();
    settle();
    test::render(*viewport);
    command(window, "sketch.tool.rectangle").trigger();
    click(*viewport, -20, -10);
    click(*viewport, 20, 10);
    check(acceptButton(window) != nullptr && acceptButton(window)->isEnabled(),
          "The rectangle can be accepted");

    // Meanwhile Claude adds a box over MCP.
    auto context = structured(client.call("get_context", {}));
    const auto box = structured(
        client.call("create_box", {{"requestId", "box-1"},
                                   {"documentId", context.value("documentId")},
                                   {"stamp", context.value("stamp")},
                                   {"width", QJsonObject{{"value", 10}, {"unit", "mm"}}},
                                   {"depth", QJsonObject{{"value", 10}, {"unit", "mm"}}},
                                   {"height", QJsonObject{{"value", 10}, {"unit", "mm"}}}}));
    settle();
    check(box.value("status") == QLatin1String("committed") && session.features().size() == 1,
          "The box exists");
    auto* tree = window.findChild<QTreeWidget*>(QStringLiteral("modelTree"));
    const auto treeItem = [&](const std::string& name) {
        const auto items =
            tree->findItems(QString::fromStdString(name), Qt::MatchRecursive | Qt::MatchExactly);
        return items.isEmpty() ? nullptr : items.front();
    };
    check(tree != nullptr && treeItem(session.features().front().name) != nullptr,
          "The model tree shows the box");
    check(window.statusBar()->currentMessage().startsWith(QStringLiteral("Claude: ")) &&
              chip->property("flash").toBool() &&
              treeItem(session.features().front().name)->font(0).bold(),
          "Status bar, chip and model tree announce Claude's change");
    check(acceptButton(window) != nullptr && acceptButton(window)->isVisible(),
          "The user's sketch stays open");

    // A new base-plane sketch depends on nothing else: it is accepted beside the box.
    acceptButton(window)->click();
    settle();
    check(session.features().size() == 2 &&
              session.features().back().kind == document::FeatureKind::sketch,
          "The user's sketch is created beside the box");
    const auto sketch = session.features().back();
    const auto stored = [&] { return session.sketchParameters(sketch.id); };
    const auto drawn = stored();

    // The user edits the sketch while Claude changes the same sketch.
    auto* item = treeItem(sketch.name);
    check(item != nullptr, "The sketch is in the model tree");
    tree->setCurrentItem(item);
    settle();
    command(window, "solid.modify.edit_sketch").trigger();
    settle();
    check(acceptButton(window) != nullptr && acceptButton(window)->isVisible(),
          "The sketch editor is open");
    context = structured(client.call("get_context", {}));
    QJsonObject content;
    for (const auto feature : context.value("features").toArray())
        if (feature.toObject().value("id").toInteger() == static_cast<qint64>(sketch.id.value()))
            content = feature.toObject().value("parameters").toObject().value("sketch").toObject();
    content.remove("plane");
    auto elements = content.value("elements").toArray();
    elements.append(QJsonObject{{"id", static_cast<double>(drawn.elements.nextId)},
                                {"type", "circle"},
                                {"center", QJsonArray{0, 0}},
                                {"radius", 4}});
    content.insert("elements", elements);
    const auto edited =
        structured(client.call("edit_sketch", {{"requestId", "edit-1"},
                                               {"documentId", context.value("documentId")},
                                               {"stamp", context.value("stamp")},
                                               {"sketchId", static_cast<double>(sketch.id.value())},
                                               {"sketch", content}}));
    settle();
    const auto claudes = stored();
    check(edited.value("status") == QLatin1String("committed") &&
              claudes.elements.values.size() == drawn.elements.values.size() + 1,
          "Claude's edit is stored: " + QJsonDocument(edited).toJson().toStdString());
    acceptButton(window)->click();
    settle();
    check(stored() == claudes && acceptButton(window) != nullptr &&
              acceptButton(window)->isVisible(),
          "The user's older draft does not overwrite Claude's edit; the editor stays open");
    auto* panel = window.findChild<QWidget*>(QStringLiteral("activeToolPanel"));
    auto* cancel = panel->findChild<QPushButton*>(QStringLiteral("toolCancelButton"));
    check(cancel != nullptr, "Cancel button");
    test::answerDiscardQuestion(QStringLiteral("Verwerfen")); // S13: the user's draft changed
    cancel->click();
    settle();

    // Claude's change is one undo step for the user.
    command(window, "history.undo").trigger();
    settle();
    check(stored() == drawn && session.features().size() == 2, "Undo restores the drawn sketch");
    // Switched off: no open port, and the choice is remembered.
    command(window, "assistant.mcp").trigger();
    settle();
    QTcpSocket probe;
    probe.connectToHost(QHostAddress::LocalHost, static_cast<quint16>(match.captured(1).toUInt()));
    check(!probe.waitForConnected(1000) && chip->property("state") == QStringLiteral("off") &&
              !QSettings().value(QStringLiteral("mcp/enabled")).toBool(),
          "Switching off closes the port and is remembered");
    std::cout << "PASS AI04c: MCP changes in the running shell\n";
}

// Privacy switch: with AI functions off there is no port and no chip, also after a restart;
// VOELUND_DISABLE_AI locks this for an installation.
void privacySwitch() {
    QSettings settings;
    settings.setValue(QStringLiteral("mcp/enabled"), true);
    settings.setValue(QStringLiteral("mcp/port"), 0);
    settings.setValue(QStringLiteral("assistant/disabled"), false);
    application::CadSession session;
    session.newDocument();
    {
        desktop::MainWindow window(session);
        auto* chip = window.findChild<QToolButton*>(QStringLiteral("assistantChip"));
        check(!chip->isHidden() && command(window, "assistant.mcp").isChecked(),
              "Access on before the switch");
        command(window, "assistant.allow").trigger();
        check(chip->isHidden() && !command(window, "assistant.mcp").isChecked() &&
                  !command(window, "assistant.mcp").isEnabled() &&
                  !window.setMcpAccessEnabled(true) &&
                  QSettings().value(QStringLiteral("assistant/disabled")).toBool(),
              "AI functions off: no port, no chip, access cannot be switched on");
    }
    {
        desktop::MainWindow restarted(session);
        auto* chip = restarted.findChild<QToolButton*>(QStringLiteral("assistantChip"));
        check(chip->isHidden() && !command(restarted, "assistant.mcp").isChecked(),
              "The switch survives a restart");
        command(restarted, "assistant.allow").trigger();
        check(!chip->isHidden() && command(restarted, "assistant.allow").isChecked() &&
                  !command(restarted, "assistant.mcp").isChecked(),
              "Allowed again; the access itself stays off until switched on");
    }
    qputenv("VOELUND_DISABLE_AI", "1");
    {
        desktop::MainWindow locked(session);
        auto& allow = command(locked, "assistant.allow");
        allow.trigger();
        check(!allow.isEnabled() && !locked.setMcpAccessEnabled(true) &&
                  locked.findChild<QToolButton*>(QStringLiteral("assistantChip"))->isHidden(),
              "VOELUND_DISABLE_AI locks all AI functions");
    }
    qunsetenv("VOELUND_DISABLE_AI");
    std::cout << "PASS privacy switch\n";
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    try {
        run();
        privacySwitch();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
