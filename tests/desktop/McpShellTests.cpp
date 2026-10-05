#include "../McpTestClient.hpp"
#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <QAction>
#include <QApplication>
#include <QPushButton>
#include <QStatusBar>
#include <QTreeWidget>
#include <QUrl>
#include <iostream>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>
#include <voelund/desktop/McpAccess.hpp>

// AI04c: the running shell with its MCP server. A change over MCP appears in the model tree
// and is one undo step for the user. A sketch the user is drawing stays open; accepting it
// afterwards is refused as stale instead of overwriting the change.
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

void run() {
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

    desktop::McpAccess access(session, window, {0, "shell-test-token-0123456789abcdef"});
    check(access.start() && access.isRunning(), "The MCP server starts");
    check(access.claudeCommand().contains(access.url()) &&
              access.claudeCommand().contains(QStringLiteral("Bearer shell-test-token")),
          "The connect command names URL and token");
    Client client(static_cast<quint16>(QUrl(access.url()).port()), "shell-test-token-0123456789abcdef");
    static_cast<void>(client.open());

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
    const auto box = structured(client.call(
        "create_box", {{"requestId", "box-1"},
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
        const auto items = tree->findItems(QString::fromStdString(name),
                                           Qt::MatchRecursive | Qt::MatchExactly);
        return items.isEmpty() ? nullptr : items.front();
    };
    check(tree != nullptr && treeItem(session.features().front().name) != nullptr,
          "The model tree shows the box");
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
    const auto edited = structured(client.call("edit_sketch", {{"requestId", "edit-1"},
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
    check(stored() == claudes && acceptButton(window) != nullptr && acceptButton(window)->isVisible(),
          "The user's older draft does not overwrite Claude's edit; the editor stays open");
    auto* panel = window.findChild<QWidget*>(QStringLiteral("activeToolPanel"));
    auto* cancel = panel->findChild<QPushButton*>(QStringLiteral("toolCancelButton"));
    check(cancel != nullptr, "Cancel button");
    cancel->click();
    settle();

    // Claude's change is one undo step for the user.
    command(window, "history.undo").trigger();
    settle();
    check(stored() == drawn && session.features().size() == 2, "Undo restores the drawn sketch");
    std::cout << "PASS AI04c: MCP changes in the running shell\n";
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
