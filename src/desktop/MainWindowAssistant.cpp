#include "MainWindowState.hpp"
#include "MainWindowSupport.hpp"
#include "ModelTreePanel.hpp"

#include <voelund/application/Actions.hpp>

#include <QApplication>
#include <QClipboard>
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
        statusBar()->showMessage(
            tr("Befehl kopiert – in einem Terminal mit Claude Code ausführen"), 5000);
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
    auto* hint = new QLabel(tr("Jede Änderung von Claude ist ein Rückgängig-Schritt."), menu);
    hint->setObjectName(QStringLiteral("assistantHint"));
    menu->addAction(impl_->mcpAction);
    menu->addAction(labelAction(*menu, impl_->mcpAddress));
    menu->addSeparator();
    menu->addAction(impl_->mcpCopyAction);
    menu->addAction(impl_->mcpTokenAction);
    menu->addSeparator();
    menu->addAction(labelAction(*menu, hint));
    impl_->header->setAssistantMenu(*menu);

    // "verbunden" fades to "bereit" when the client has been quiet for a while.
    auto* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this] { syncAssistant(); });
    timer->start(30 * 1000);
    if (settings.enabled)
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
    const bool running = impl_->mcp->isRunning();
    const bool connected = running && impl_->mcpActivity.isValid() &&
                           impl_->mcpActivity.elapsed() < connectedWindowMs;
    impl_->header->showAssistant(!running   ? AssistantState::off
                                 : connected ? AssistantState::connected
                                             : AssistantState::ready);
    impl_->mcpAction->setChecked(running);
    impl_->mcpCopyAction->setEnabled(running);
    impl_->mcpAddress->setText(running ? impl_->mcp->url().replace(QStringLiteral("/mcp"), QString())
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
        subject = tr("Parameter %1").arg(parameterNames(
            impl_->session,
            result.createdParameters.empty() ? result.changedParameters : result.createdParameters));
        verb = result.createdParameters.empty() ? tr("geändert") : tr("angelegt");
    }
    statusBar()->showMessage(
        tr("Claude: %1 %2 – Strg+Z nimmt es zurück").arg(subject, verb), 8000);
    impl_->header->flashAssistant();
    std::vector<document::FeatureId> marked = result.createdFeatures;
    marked.insert(marked.end(), result.changedFeatures.begin(), result.changedFeatures.end());
    impl_->modelTree->flashFeatures(marked, themeTokens(impl_->themeMode).accent);
}

} // namespace voelund::desktop
