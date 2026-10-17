#include "AssistantPanel.hpp"
#include "MainWindowState.hpp"
#include "MainWindowSupport.hpp"
#include "ModelTreePanel.hpp"

#include <voelund/application/Actions.hpp>

#include <QApplication>
#include <QClipboard>
#include <QDockWidget>
#include <QLabel>
#include <QMenu>
#include <QStatusBar>
#include <QTimer>
#include <QWidgetAction>

#include <algorithm>

// AI04c: the Claude chip in the header (user decision 2026-10-05): on/off, address, copy the
// connect command, new token. Every change made over MCP is announced in the status bar,
// lights the chip up once and marks the changed features in the model tree.
namespace voelund::desktop {
namespace {
constexpr qint64 connectedWindowMs = 5 * 60 * 1000;

QWidgetAction* labelAction(QMenu& menu, QLabel* label) {
    auto* action = new QWidgetAction(&menu);
    label->setContentsMargins(12, 4, 12, 4);
    action->setDefaultWidget(label);
    return action;
}

QString featureNames(const application::CadSession& session,
                     const std::vector<document::FeatureId>& ids) {
    QStringList names;
    for (const auto& feature : session.features())
        if (std::ranges::find(ids, feature.id) != ids.end())
            names.append(QString::fromStdString(feature.name));
    return names.join(QStringLiteral(", "));
}

QString parameterNames(const application::CadSession& session,
                       const std::vector<document::ParameterId>& ids) {
    QStringList names;
    for (const auto id : ids)
        if (const auto* parameter = document::findParameter(session.parameterTable(), id))
            names.append(QString::fromStdString(parameter->name));
    return names.join(QStringLiteral(", "));
}
} // namespace

void MainWindow::createAssistant() {
    const auto settings = loadMcpSettings();
    impl_->mcp = std::make_unique<McpAccess>(impl_->session, settings);
    impl_->mcp->setChangeHandler(
        [this](const application::ActionResult& result) { announceExternalChange(result); });
    impl_->mcp->setActivityHandler([this] {
        impl_->mcpActivity.start();
        syncAssistant();
    });

    auto* menu = new QMenu(this);
    menu->setObjectName(QStringLiteral("assistantMenu"));
    impl_->mcpAction = new QAction(tr("Claude-Zugang (MCP) aktiv"), this);
    impl_->mcpAction->setCheckable(true);
    setCommandId(*impl_->mcpAction, "assistant.mcp");
    connect(impl_->mcpAction, &QAction::triggered, this,
            [this](bool checked) { static_cast<void>(setMcpAccessEnabled(checked)); });
    impl_->mcpAddress = new QLabel(menu);
    impl_->mcpAddress->setObjectName(QStringLiteral("assistantAddress"));
    impl_->mcpCopyAction = new QAction(tr("Befehl für Claude Code kopieren"), this);
    setCommandId(*impl_->mcpCopyAction, "assistant.mcp_copy_command");
    connect(impl_->mcpCopyAction, &QAction::triggered, this, [this] {
        QApplication::clipboard()->setText(mcpConnectCommand());
        statusBar()->showMessage(tr("Befehl kopiert – in einem Terminal mit Claude Code ausführen"),
                                 5000);
    });
    impl_->mcpTokenAction = new QAction(tr("Token neu erzeugen"), this);
    setCommandId(*impl_->mcpTokenAction, "assistant.mcp_new_token");
    connect(impl_->mcpTokenAction, &QAction::triggered, this, [this] {
        impl_->mcp->regenerateToken();
        impl_->mcpActivity.invalidate();
        syncAssistant();
        statusBar()->showMessage(
            tr("Neuer Token – Claude Code mit dem neuen Befehl erneut verbinden"), 5000);
    });
    impl_->aiAllowedAction = new QAction(tr("KI-Funktionen erlauben"), this);
    impl_->aiAllowedAction->setCheckable(true);
    setCommandId(*impl_->aiAllowedAction, "assistant.allow");
    connect(impl_->aiAllowedAction, &QAction::triggered, this, [this](bool allowed) {
        if (!setAiAllowed(allowed)) {
            syncAssistant();
            return;
        }
        if (!allowed)
            static_cast<void>(setMcpAccessEnabled(false));
        syncAssistant();
        statusBar()->showMessage(allowed ? tr("KI-Funktionen erlaubt")
                                         : tr("KI-Funktionen aus – Völund öffnet keinen Port "
                                              "und startet kein Claude Code"),
                                 6000);
    });
    // Also reachable through the command search while the chip is hidden.
    addAction(impl_->aiAllowedAction);
    auto* hint = new QLabel(tr("Jede Änderung von Claude ist ein Rückgängig-Schritt."), menu);
    hint->setObjectName(QStringLiteral("assistantHint"));
    menu->addAction(impl_->mcpAction);
    menu->addAction(labelAction(*menu, impl_->mcpAddress));
    menu->addSeparator();
    menu->addAction(impl_->mcpCopyAction);
    menu->addAction(impl_->mcpTokenAction);
    menu->addSeparator();
    menu->addAction(labelAction(*menu, hint));
    menu->addSeparator();
    menu->addAction(impl_->aiAllowedAction);
    impl_->header->setAssistantMenu(*menu);
    createAssistantPanel();

    // "verbunden" fades to "bereit" when the client has been quiet for a while.
    auto* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this] { syncAssistant(); });
    timer->start(30 * 1000);
    if (settings.enabled && aiAllowed())
        static_cast<void>(setMcpAccessEnabled(true));
    syncAssistant();
}

bool MainWindow::setMcpAccessEnabled(bool enabled) {
    const bool running = impl_->mcp->setEnabled(enabled) && impl_->mcp->isRunning() == enabled;
    if (!enabled)
        impl_->mcpActivity.invalidate();
    if (enabled && !impl_->mcp->isRunning())
        statusBar()->showMessage(tr("Claude-Zugang: %1 ist belegt").arg(impl_->mcp->url()), 8000);
    syncAssistant();
    return running;
}

QString MainWindow::mcpConnectCommand() const { return impl_->mcp->claudeCommand(); }

void MainWindow::syncAssistant() {
    const bool allowed = aiAllowed();
    if (!allowed && impl_->assistantBackend->isRunning())
        impl_->assistantBackend->cancel();
    impl_->assistantPanelAction->setEnabled(allowed);
    if (!allowed) {
        impl_->assistantPanelAction->setChecked(false);
        impl_->assistantDock->hide();
    }
    const bool locked = aiLockedByEnvironment();
    impl_->aiAllowedAction->setChecked(allowed);
    impl_->aiAllowedAction->setEnabled(!locked);
    impl_->aiAllowedAction->setToolTip(locked ? tr("Durch VOELUND_DISABLE_AI gesperrt")
                                              : QString());
    impl_->header->setAssistantVisible(allowed);
    impl_->mcpAction->setEnabled(allowed);
    impl_->mcpTokenAction->setEnabled(allowed);
    const bool running = impl_->mcp->isRunning();
    const bool connected =
        running && impl_->mcpActivity.isValid() && impl_->mcpActivity.elapsed() < connectedWindowMs;
    impl_->header->showAssistant(!running    ? AssistantState::off
                                 : connected ? AssistantState::connected
                                             : AssistantState::ready);
    impl_->mcpAction->setChecked(running);
    impl_->mcpCopyAction->setEnabled(running);
    impl_->mcpAddress->setText(running
                                   ? impl_->mcp->url().replace(QStringLiteral("/mcp"), QString())
                                   : tr("Aus – kein offener Port"));
}

void MainWindow::announceExternalChange(const application::ActionResult& result) {
    refreshAfterExternalChange();
    QString subject;
    QString verb;
    if (!result.createdFeatures.empty()) {
        subject = featureNames(impl_->session, result.createdFeatures);
        verb = tr("angelegt");
    } else if (!result.changedFeatures.empty()) {
        subject = featureNames(impl_->session, result.changedFeatures);
        verb = tr("geändert");
    } else {
        subject = tr("Parameter %1")
                      .arg(parameterNames(impl_->session, result.createdParameters.empty()
                                                              ? result.changedParameters
                                                              : result.createdParameters));
        verb = result.createdParameters.empty() ? tr("geändert") : tr("angelegt");
    }
    statusBar()->showMessage(tr("Claude: %1 %2 – Strg+Z nimmt es zurück").arg(subject, verb), 8000);
    impl_->header->flashAssistant();
    std::vector<document::FeatureId> marked = result.createdFeatures;
    marked.insert(marked.end(), result.changedFeatures.begin(), result.changedFeatures.end());
    impl_->modelTree->flashFeatures(marked, themeTokens(impl_->themeMode).accent);
}

// AI03b: the Claude panel at the right. Claude Code runs per message against this window's
// MCP server with a token per run; Stop and a document change revoke it.
void MainWindow::createAssistantPanel() {
    impl_->assistantPanel = new AssistantPanel(this);
    impl_->assistantDock = new QDockWidget(tr("Claude"), this);
    impl_->assistantDock->setObjectName(QStringLiteral("assistantDock"));
    impl_->assistantDock->setAllowedAreas(Qt::RightDockWidgetArea);
    impl_->assistantDock->setWidget(impl_->assistantPanel);
    addDockWidget(Qt::RightDockWidgetArea, impl_->assistantDock);
    impl_->assistantDock->hide();

    impl_->assistantPanelAction = new QAction(tr("Claude-Panel"), this);
    impl_->assistantPanelAction->setCheckable(true);
    setCommandId(*impl_->assistantPanelAction, "assistant.panel");
    addAction(impl_->assistantPanelAction);
    connect(impl_->assistantPanelAction, &QAction::toggled, this,
            [this](bool open) { impl_->assistantDock->setVisible(open && aiAllowed()); });
    impl_->header->setAssistantClicked([this] { impl_->assistantPanelAction->toggle(); });

    assistant::ClaudeCodeOptions options;
    options.program = qEnvironmentVariable("VOELUND_CLAUDE_PROGRAM", QStringLiteral("claude"));
    options.beginRun = [this] {
        const auto run = impl_->mcp->beginPanelRun();
        return assistant::RunAccess{run.url, run.token};
    };
    options.endRun = [this](const QByteArray& token) {
        impl_->mcp->endPanelRun(token);
        syncAssistant();
    };
    impl_->assistantBackend = std::make_unique<assistant::ClaudeCodeBackend>(std::move(options));
    impl_->assistantBackend->setHandler([this](const assistant::AssistantEvent& event) {
        impl_->assistantPanel->present(event);
        impl_->assistantPanel->setRunning(impl_->assistantBackend->isRunning() &&
                                          event.kind != assistant::AssistantEvent::Kind::finished &&
                                          event.kind != assistant::AssistantEvent::Kind::failed &&
                                          event.kind != assistant::AssistantEvent::Kind::cancelled);
    });
    impl_->assistantPanel->setHandlers({[this](const QString& message, bool withSelection) {
                                            sendToAssistant(message, withSelection);
                                        },
                                        [this] { impl_->assistantBackend->cancel(); },
                                        [this] {
                                            impl_->assistantBackend->cancel();
                                            impl_->assistantBackend->reset();
                                            impl_->assistantPanel->clear();
                                        }});
    impl_->assistantGeneration = impl_->session.sceneStatus().document.generation;
}

void MainWindow::sendToAssistant(const QString& message, bool withSelection) {
    QString prompt = message;
    const auto selected = impl_->session.selectedFeature();
    if (withSelection && selected) {
        // The selection goes along as data; names are quoted and never instructions.
        for (const auto& feature : impl_->session.features())
            if (feature.id == *selected)
                prompt = tr("[Völund-Kontext, nur Daten] Auswahl: Feature-ID %1, Name „%2“.")
                             .arg(selected->value())
                             .arg(QString::fromStdString(feature.name)) +
                         QStringLiteral("\n\n") + message;
    }
    impl_->assistantPanel->addUserMessage(message);
    impl_->assistantPanel->setRunning(true);
    if (!impl_->assistantBackend->send(prompt)) {
        impl_->assistantPanel->setRunning(false);
        if (impl_->assistantBackend->isRunning())
            impl_->assistantPanel->present({assistant::AssistantEvent::Kind::failed,
                                            tr("Claude arbeitet noch an der letzten Nachricht.")});
    }
    syncAssistant();
}

void MainWindow::syncAssistantContext() {
    if (impl_->assistantPanel == nullptr)
        return;
    const auto generation = impl_->session.sceneStatus().document.generation;
    if (generation != impl_->assistantGeneration) {
        impl_->assistantGeneration = generation;
        impl_->assistantBackend->cancel();
        impl_->assistantBackend->reset();
        impl_->assistantPanel->clear(tr("Anderes Dokument – neues Gespräch."));
    }
    const auto selected = impl_->session.selectedFeature();
    QString name;
    if (selected)
        for (const auto& feature : impl_->session.features())
            if (feature.id == *selected)
                name = QString::fromStdString(feature.name);
    impl_->assistantPanel->setSelection(name);
}

} // namespace voelund::desktop
