// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <QAction>
#include <QApplication>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <cmath>
#include <iostream>
#include <numbers>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// AI04d acceptance helper (not a CI test; needs a logged-in Claude Code): a real Völund window
// with its MCP access and Claude Code as the client, started headless twice (two processes,
// two MCP sessions). While Claude builds the plate, the "user" adds a box in the shell; Claude
// has to cope with the stale stamp, and the box must survive. The second run changes the
// parameter and the thickness. Prints the checks and Claude's answers.
namespace {
using namespace voelund;

int failures = 0;
void check(bool condition, const std::string& message) {
    std::cout << (condition ? "  ok   " : "  FAIL ") << message << '\n';
    if (!condition)
        ++failures;
}
double volume(const application::CadSession& session) {
    GProp_GProps properties;
    BRepGProp::VolumeProperties(session.currentShape(), properties);
    return properties.Mass();
}
bool approximately(double a, double b) {
    return std::abs(a - b) < 1e-4 * std::max(1.0, std::abs(b));
}
QAction& command(QWidget& window, const char* id) {
    for (auto* action : window.findChildren<QAction*>())
        if (action->property("commandId").toString() == QLatin1String(id))
            return *action;
    throw std::runtime_error(std::string("Command is missing: ") + id);
}
const document::DocumentParameter* parameterA(const application::CadSession& session) {
    return document::findParameter(session.parameterTable(), "A");
}

// Runs Claude Code headless against this window's server; the GUI keeps running meanwhile.
QJsonObject claude(const QString& config, const QString& prompt,
                   const std::function<void()>& whileRunning) {
    QProcess process;
    process.setProgram(QStringLiteral("claude"));
    process.setArguments({"-p", prompt, "--mcp-config", config, "--strict-mcp-config",
                          "--allowedTools", "mcp__volund__*", "--output-format", "json"});
    process.setStandardInputFile(QProcess::nullDevice());
    QEventLoop loop;
    QObject::connect(&process, &QProcess::finished, &loop, &QEventLoop::quit);
    QObject::connect(&process, &QProcess::errorOccurred, &loop, &QEventLoop::quit);
    QTimer poll;
    QObject::connect(&poll, &QTimer::timeout, whileRunning);
    poll.start(50);
    QTimer::singleShot(15 * 60 * 1000, &loop, &QEventLoop::quit);
    process.start();
    loop.exec();
    poll.stop();
    if (process.state() != QProcess::NotRunning) {
        process.kill();
        process.waitForFinished();
    }
    return QJsonDocument::fromJson(process.readAllStandardOutput()).object();
}

void report(const QJsonObject& run) {
    std::cout << "  Claude (" << run.value("num_turns").toInt()
              << " turns): " << run.value("result").toString().trimmed().toStdString() << '\n';
}

void run(const QString& workDirectory) {
    QSettings().setValue(QStringLiteral("mcp/enabled"), true);
    QSettings().setValue(QStringLiteral("mcp/port"), 0);
    application::CadSession session;
    session.newDocument();
    desktop::MainWindow window(session);
    window.resize(1400, 860);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    test::render(*viewport);

    const auto match = QRegularExpression(QStringLiteral("(http://[^ ]+) .*Bearer ([0-9a-f]+)"))
                           .match(window.mcpConnectCommand());
    if (!match.hasMatch())
        throw std::runtime_error("No connect command");
    const QString config = workDirectory + QStringLiteral("/mcp.json");
    QFile file(config);
    if (!file.open(QIODevice::WriteOnly))
        throw std::runtime_error("Cannot write the MCP config");
    file.write(
        QJsonDocument(
            QJsonObject{
                {"mcpServers",
                 QJsonObject{
                     {"volund",
                      QJsonObject{{"type", "http"},
                                  {"url", match.captured(1)},
                                  {"headers", QJsonObject{{"Authorization",
                                                           "Bearer " + match.captured(2)}}}}}}}})
            .toJson());
    file.close();

    std::cout << "Run 1: plate with a bore bound to A; the user adds a box meanwhile\n";
    bool userChanged = false;
    const auto first = claude(
        config,
        QStringLiteral(
            "In Völund (MCP-Server volund): Lege einen Längenparameter A = 8 mm an. Erzeuge eine "
            "Platte 80 x 40 mm, 10 mm dick, auf der XY-Ebene, zentriert im Ursprung, mit einer "
            "runden Bohrung in der Mitte, deren Radius an A gebunden ist. Der Nutzer arbeitet "
            "gleichzeitig im selben Dokument; ändere nichts, was du nicht selbst angelegt hast. "
            "Antworte am Ende in einem Satz."),
        [&] {
            // The user's own change while Claude is working, after Claude's first change.
            if (!userChanged && !session.parameterTable().values.empty()) {
                userChanged = true;
                command(window, "solid.create.box").trigger();
            }
        });
    report(first);
    QApplication::processEvents();
    const double box = 20.0 * 20.0 * 40.0;
    const auto* a = parameterA(session);
    check(userChanged, "The user changed the document while Claude was working");
    check(a != nullptr && a->value == 8, "Parameter A = 8 mm");
    check(std::ranges::count(session.features(), document::FeatureKind::box,
                             &document::FeatureDescriptor::kind) == 1,
          "The user's box is still there");
    check(approximately(volume(session), (3200.0 - 64.0 * std::numbers::pi) * 10.0 + box),
          "Volume: plate with bore r = 8 plus the box (" + std::to_string(volume(session)) + ")");
    const auto revision = session.sceneStatus().document.revision;

    std::cout << "Run 2: new process and session; A = 10 mm, thickness 12 mm\n";
    const auto second = claude(
        config,
        QStringLiteral("In Völund (MCP-Server volund): Setze den Parameter A auf 10 mm und die "
                       "Dicke der Platte auf 12 mm. Ändere sonst nichts. Antworte am Ende in "
                       "einem Satz."),
        [] {});
    report(second);
    QApplication::processEvents();
    a = parameterA(session);
    check(a != nullptr && a->value == 10, "Parameter A = 10 mm");
    check(approximately(volume(session), (3200.0 - 100.0 * std::numbers::pi) * 12.0 + box),
          "Volume: plate 12 mm with bore r = 10 plus the box (" + std::to_string(volume(session)) +
              ")");
    const auto steps = session.sceneStatus().document.revision - revision;
    for (std::uint64_t step = 0; step < steps; ++step)
        command(window, "history.undo").trigger();
    QApplication::processEvents();
    check(steps >= 2 && parameterA(session)->value == 8 &&
              approximately(volume(session), (3200.0 - 64.0 * std::numbers::pi) * 10.0 + box),
          "Undoing Claude's " + std::to_string(steps) + " changes restores run 1");
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    QTemporaryDir work;
    try {
        run(work.path());
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
    std::cout << (failures == 0 ? "PASS" : "FAIL") << " AI04d with Claude Code\n";
    return failures == 0 ? 0 : 1;
}
