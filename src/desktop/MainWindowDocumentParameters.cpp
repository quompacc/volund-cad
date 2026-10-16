#include "CommandFeedback.hpp"
#include "MainWindowState.hpp"
#include "MainWindowSupport.hpp"
#include "OverlayCard.hpp"
#include "ParameterPanel.hpp"
#include "ShellHeader.hpp"
#include "ToolPanelHost.hpp"

#include <QAction>
#include <QKeySequence>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QVBoxLayout>

#include <algorithm>
#include <string>

// PAR02 (docs/parameter-table.md, user decision 2026-10-16): the parameter card under the model
// tree, its switch in the header (Ctrl+P), and "=Name" in the fields of the tools.
namespace voelund::desktop {
namespace {
using application::CommandStatus;
using document::ParameterUnit;

// "P1", "P2", ...: the first name the table does not have yet (case-insensitive).
std::string freeName(const document::ParameterTable& table) {
    for (int number = 1;; ++number) {
        const QString name = QStringLiteral("P%1").arg(number);
        if (std::ranges::none_of(table.values, [&](const document::DocumentParameter& entry) {
                return QString::fromStdString(entry.name).compare(name, Qt::CaseInsensitive) == 0;
            }))
            return name.toStdString();
    }
}

ParameterUnit unitOf(ToolValueKind kind) {
    return kind == ToolValueKind::angle ? ParameterUnit::angle : ParameterUnit::length;
}

QString withUnit(double value, ParameterUnit unit) {
    return QLocale().toString(value, 'f', 3) +
           (unit == ParameterUnit::angle ? QStringLiteral("°") : QStringLiteral(" mm"));
}
} // namespace

int MainWindow::parameterCardHeight(int column) const {
    return std::min(impl_->parameterPanel->contentHeight() + 40, std::max(110, column / 2));
}

void MainWindow::createParameterCard() {
    auto* card = new OverlayCard(*impl_->viewport, QStringLiteral("parameterCard"));
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(8, 10, 8, 8);
    layout->setSpacing(4);
    auto* title = new QLabel(tr("Parameter"), card);
    title->setObjectName(QStringLiteral("overlayTitle"));
    layout->addWidget(title);
    auto* panel = new ParameterPanel(card);
    layout->addWidget(panel);
    impl_->parameterCard = card;
    impl_->parameterPanel = panel;
    card->setPlacement([this](const QWidget& area) {
        const int column = area.height() - overlay::columnTop - overlay::bottomBand;
        const int top = impl_->modelCard->geometry().bottom() + 1 + overlay::gap;
        return QRect(overlay::margin, top, overlay::treeWidth, parameterCardHeight(column));
    });
    card->hide();

    impl_->parametersAction = new QAction(tr("Parameter"), this);
    impl_->parametersAction->setCheckable(true);
    impl_->parametersAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_P));
    impl_->parametersAction->setToolTip(tr("Parametertabelle ein- und ausblenden (Strg+P)"));
    setCommandId(*impl_->parametersAction, "view.parameters");
    addAction(impl_->parametersAction);
    connect(impl_->parametersAction, &QAction::toggled, this, [this](bool shown) {
        impl_->parameterCard->setVisible(shown);
        syncParameters();
        impl_->modelCard->place();
        impl_->parameterCard->place();
    });
    impl_->header->setParametersAction(*impl_->parametersAction);

    // Every edit is one command; a refusal says why and the table shows the document again.
    const auto run = [this](const application::CommandResult& result) {
        if (result.status() == CommandStatus::rejected) {
            if (result.reason() != application::RejectionReason::noChange)
                statusBar()->showMessage(commandFeedbackMessage(result));
            syncParameters();
            return false;
        }
        refreshScene();
        statusBar()->showMessage(result.status() == CommandStatus::committedWithErrors
                                     ? commandFeedbackMessage(result)
                                     : tr("Parameter geändert"),
                                 result.status() == CommandStatus::committed ? 3000 : 0);
        return true;
    };
    panel->setHandlers({
        [this, run](document::ParameterId id, const QString& name) {
            static_cast<void>(run(impl_->session.renameParameter(id, name.toStdString())));
        },
        [this, run](document::ParameterId id, double value) {
            static_cast<void>(run(impl_->session.setParameterValue(id, value)));
        },
        [this, run](ParameterUnit unit) {
            const auto name = freeName(impl_->session.parameterTable());
            if (!run(impl_->session.createParameter(
                    name, unit, unit == ParameterUnit::angle ? 90.0 : 10.0)))
                return;
            for (const auto& entry : impl_->session.parameterTable().values)
                if (entry.name == name)
                    impl_->parameterPanel->editName(entry.id);
        },
        [this, run](document::ParameterId id) {
            const auto* parameter = document::findParameter(impl_->session.parameterTable(), id);
            if (parameter == nullptr)
                return;
            const auto uses = impl_->session.parameterUseCount(id);
            if (uses == 0) {
                static_cast<void>(run(impl_->session.deleteParameter(id)));
                return;
            }
            // Rule 6: a parameter in use is never deleted silently.
            QMessageBox ask(QMessageBox::Question, tr("Parameter entfernen"),
                            tr("„%1“ wird %n-mal verwendet. Diese Maße behalten ihren "
                               "aktuellen Wert als feste Zahl.",
                               nullptr, static_cast<int>(uses))
                                .arg(QString::fromStdString(parameter->name)),
                            QMessageBox::NoButton, this);
            ask.setObjectName(QStringLiteral("deleteParameterQuestion"));
            auto* unbind = ask.addButton(tr("Lösen und entfernen"), QMessageBox::AcceptRole);
            ask.addButton(tr("Abbrechen"), QMessageBox::RejectRole);
            ask.exec();
            if (ask.clickedButton() == unbind)
                static_cast<void>(run(impl_->session.deleteParameter(
                    id, application::ParameterDeletion::unbindUses)));
            else
                syncParameters();
        },
        [this](const QString& message) { statusBar()->showMessage(message); }});

    // "=Name" in the fields of the tools: names of the field's unit, a new one after a question.
    impl_->toolPanel->setParameterNames([this](ToolValueKind kind) {
        QStringList names;
        for (const auto& entry : impl_->session.parameterTable().values)
            if (entry.unit == unitOf(kind))
                names.append(QString::fromStdString(entry.name));
        return names;
    });
    impl_->toolPanel->setParameterResolver(
        [this](const QString& name, ToolValueKind kind,
               double value) -> std::optional<ResolvedParameter> {
            const auto unit = unitOf(kind);
            for (const auto& entry : impl_->session.parameterTable().values)
                if (QString::fromStdString(entry.name).compare(name, Qt::CaseInsensitive) == 0) {
                    if (entry.unit == unit)
                        return ResolvedParameter{entry.id, false};
                    statusBar()->showMessage(
                        entry.unit == ParameterUnit::angle
                            ? tr("„%1“ ist ein Winkel, hier wird eine Länge gebraucht").arg(name)
                            : tr("„%1“ ist eine Länge, hier wird ein Winkel gebraucht").arg(name));
                    return std::nullopt;
                }
            QMessageBox ask(QMessageBox::Question, tr("Parameter anlegen"),
                            tr("Den Parameter „%1“ = %2 anlegen?").arg(name, withUnit(value, unit)),
                            QMessageBox::Yes | QMessageBox::No, this);
            ask.setObjectName(QStringLiteral("createParameterQuestion"));
            ask.setDefaultButton(QMessageBox::Yes);
            if (ask.exec() != QMessageBox::Yes)
                return std::nullopt;
            const auto result = impl_->session.createParameter(name.toStdString(), unit, value);
            if (result.status() != CommandStatus::committed) {
                statusBar()->showMessage(commandFeedbackMessage(result));
                return std::nullopt;
            }
            syncParameters();
            for (const auto& entry : impl_->session.parameterTable().values)
                if (entry.name == name.toStdString())
                    return ResolvedParameter{entry.id, true};
            return std::nullopt;
        });
    syncParameters();
}

void MainWindow::syncParameters() {
    if (impl_->parameterPanel == nullptr)
        return;
    std::vector<ParameterPanel::Row> rows;
    for (const auto& entry : impl_->session.parameterTable().values)
        rows.push_back({entry, impl_->session.parameterUseCount(entry.id)});
    impl_->parameterPanel->show(std::move(rows), !impl_->shell().toolBlocksCommands());
    if (impl_->parameterCard->isVisible()) {
        impl_->modelCard->place();
        impl_->parameterCard->place();
    }
}

} // namespace voelund::desktop
