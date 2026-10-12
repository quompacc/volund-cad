#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QElapsedTimer>
#include <QFile>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <iostream>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// AI03b: the Claude panel with a fake `claude` (VOELUND_CLAUDE_PROGRAM) that makes real MCP
// calls. Opening by the chip, the selection as context, the conversation with tool lines,
// Stop, a document change during a run, a missing Claude Code and the privacy switch.
namespace {
using namespace voelund;

void check(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
template <class Done> void spinUntil(Done done, const char* what) {
    QElapsedTimer timer;
    timer.start();
    while (!done()) {
        check(timer.elapsed() < 30000, std::string("Timeout: ") + what);
        QApplication::processEvents(QEventLoop::AllEvents, 20);
    }
}
QAction& command(QWidget& window, const char* id) {
    for (auto* action : window.findChildren<QAction*>())
        if (action->property("commandId").toString() == QLatin1String(id))
            return *action;
    throw std::runtime_error(std::string("Command is missing: ") + id);
}
template <class T> T& child(QWidget& window, const char* name) {
    auto* found = window.findChild<T*>(QLatin1String(name));
    check(found != nullptr, std::string("Missing widget: ") + name);
    return *found;
}
QStringList history(QWidget& window) {
    QStringList lines;
    auto& list = child<QListWidget>(window, "assistantHistory");
    for (int row = 0; row < list.count(); ++row)
        lines << list.item(row)->text();
    return lines;
}
bool contains(const QStringList& lines, const QString& text) {
    return std::ranges::any_of(lines, [&](const QString& line) { return line.contains(text); });
}
void say(QWidget& window, const QString& text) {
    child<QLineEdit>(window, "assistantInput").setText(text);
    child<QPushButton>(window, "assistantSendButton").click();
}
bool idle(QWidget& window) { return child<QPushButton>(window, "assistantSendButton").isEnabled(); }

void run(const QString& work) {
    const auto argumentsFile = work + "/args.txt";
    const auto trigger = work + "/trigger";
    qputenv("FAKE_CLAUDE_ARGS", argumentsFile.toLocal8Bit());
    qputenv("FAKE_CLAUDE_TRIGGER", trigger.toLocal8Bit());
    QSettings().setValue(QStringLiteral("mcp/port"), 0); // External access stays off.
    application::CadSession session;
    session.newDocument();
    desktop::MainWindow window(session);
    window.resize(1400, 860);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    test::render(*viewport);

    // The chip opens the panel; a box gives a selection.
    auto& dock = child<QDockWidget>(window, "assistantDock");
    check(dock.isHidden(), "The panel starts closed");
    child<QToolButton>(window, "assistantChip").click();
    QApplication::processEvents();
    check(!dock.isHidden() && command(window, "assistant.panel").isChecked(),
          "A click on the chip opens the panel");
    command(window, "solid.create.box").trigger();
    QApplication::processEvents();
    auto& selection = child<QToolButton>(window, "assistantSelection");
    check(!selection.isHidden() && selection.text().contains("Quader 1"),
          "The selection is shown above the input");

    // A run with the fake: tool lines, answer, change in the document.
    qputenv("FAKE_CLAUDE_SCENARIO", "plate");
    say(window, QStringLiteral("Lege A an"));
    spinUntil([&] { return idle(window); }, "first run");
    auto lines = history(window);
    check(lines.value(0) == "Lege A an" && contains(lines, "✓ Modell gelesen") &&
              contains(lines, "✓ Parameter anlegen") && contains(lines, "Parameter A angelegt.") &&
              session.parameterTable().values.size() == 1,
          "Conversation with tool lines and the change: " + lines.join(" | ").toStdString());
    // Tool lines follow the theme (they were unreadable in dark before).
    auto& list = child<QListWidget>(window, "assistantHistory");
    const auto toolColor = [&] { return list.item(1)->foreground().color(); };
    const auto light = toolColor();
    command(window, "view.theme").trigger();
    check(toolColor() != light, "Tool lines are recolored with the theme");
    command(window, "view.theme").trigger();
    QFile recorded(argumentsFile);
    check(recorded.open(QIODevice::ReadOnly) &&
              recorded.readAll().contains("Auswahl: Feature-ID 1, Name „Quader 1“"),
          "The selection went along as context");
    recorded.close();
    check(!window.findChild<QToolButton*>(QStringLiteral("assistantChip"))
               ->property("state")
               .toString()
               .isEmpty(),
          "Chip state is set");

    // Without the selection, and the conversation continues.
    selection.click();
    qputenv("FAKE_CLAUDE_SCENARIO", "noise");
    say(window, QStringLiteral("Und jetzt?"));
    spinUntil([&] { return idle(window); }, "second run");
    QFile again(argumentsFile);
    check(again.open(QIODevice::ReadOnly), "Arguments of the second run");
    const auto second = again.readAll();
    check(!second.contains("Auswahl:") && second.contains("--resume\nfake-session-1"),
          "Selection left out on request; the conversation is resumed");
    again.close();

    // Stop during a run: nothing changes afterwards.
    qputenv("FAKE_CLAUDE_SCENARIO", "late");
    const auto parameters = session.parameterTable().values.size();
    say(window, QStringLiteral("Lege noch etwas an"));
    spinUntil([&] { return contains(history(window), "✓ Modell gelesen") &&
                           history(window).count() > lines.count() + 2; },
              "late run reads the model");
    child<QPushButton>(window, "assistantStopButton").click();
    static_cast<void>(QFile(trigger).open(QIODevice::WriteOnly));
    spinUntil([&] { return idle(window); }, "stopped run");
    QElapsedTimer settle;
    settle.start();
    while (settle.elapsed() < 1500)
        QApplication::processEvents(QEventLoop::AllEvents, 20);
    check(contains(history(window), "Abgebrochen.") &&
              session.parameterTable().values.size() == parameters,
          "Stop: no change after the stop");
    QFile::remove(trigger);

    // Another document during a run ends run and conversation; no late change.
    // The run waits for the trigger after reading the model; "⋯" is only a passing state,
    // so wait for a further "Modell gelesen" while the run is active (flaky in CI 524, 541).
    const auto modelReads = [&] {
        return history(window).filter(QStringLiteral("Modell gelesen")).size();
    };
    const auto readsBefore = modelReads();
    say(window, QStringLiteral("Noch einmal"));
    spinUntil([&] { return !idle(window) && modelReads() > readsBefore; }, "run before new");
    QTimer responder;
    QObject::connect(&responder, &QTimer::timeout, [] {
        if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
            box->button(box->standardButtons().testFlag(QMessageBox::Discard)
                            ? QMessageBox::Discard
                            : QMessageBox::Ok)
                ->click();
    });
    responder.start(50);
    command(window, "document.new").trigger();
    responder.stop();
    static_cast<void>(QFile(trigger).open(QIODevice::WriteOnly));
    spinUntil([&] { return idle(window); }, "run ended by the new document");
    settle.restart();
    while (settle.elapsed() < 1500)
        QApplication::processEvents(QEventLoop::AllEvents, 20);
    check(session.features().empty() && session.parameterTable().values.empty() &&
              contains(history(window), "Anderes Dokument"),
          "The new document stays untouched and starts a new conversation: " +
              history(window).join(" | ").toStdString());
    QFile::remove(trigger);

    // Missing Claude Code.
    qputenv("VOELUND_CLAUDE_PROGRAM", "voelund-no-such-claude");
    {
        application::CadSession other;
        other.newDocument();
        desktop::MainWindow missing(other);
        command(missing, "assistant.panel").trigger();
        say(missing, QStringLiteral("Hallo"));
        spinUntil([&] { return idle(missing); }, "missing claude");
        check(contains(history(missing), "nicht gefunden"), "A missing Claude Code is explained");
    }

    // Privacy switch: the panel closes and cannot be opened.
    command(window, "assistant.allow").trigger();
    QApplication::processEvents();
    check(dock.isHidden() && !command(window, "assistant.panel").isEnabled(),
          "AI functions off close the panel");
    command(window, "assistant.allow").trigger();
    std::cout << "PASS AI03b: Claude panel\n";
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    try {
        if (argc < 2)
            throw std::runtime_error("Path of the fake claude expected");
        qputenv("VOELUND_CLAUDE_PROGRAM", QByteArray(argv[1]));
        QTemporaryDir work;
        run(work.path());
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
