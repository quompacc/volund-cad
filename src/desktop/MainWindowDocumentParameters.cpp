#include "CommandFeedback.hpp"
#include "FeaturePropertiesPanel.hpp"
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
#include <QRegularExpression>
#include <QStatusBar>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>
#include <optional>
#include <string>

// PAR02 (docs/parameter-table.md, user decision 2026-10-16): the parameter card under the model
// tree, its switch in the header (Ctrl+P), and "=Name" in the fields of the tools.
namespace voelund::desktop {
namespace {
using application::CommandStatus;
using document::ParameterUnit;

// "P1", "P2", ...: the first name the table does not have yet (case-insensitive).
std::string freeName(const document::ParameterTable& table,
                     const QString& prefix = QStringLiteral("P")) {
    for (int number = 1;; ++number) {
        const QString name = prefix + QString::number(number);
        if (std::ranges::none_of(table.values, [&](const document::DocumentParameter& entry) {
                return QString::fromStdString(entry.name).compare(name, Qt::CaseInsensitive) == 0;
            }))
            return name.toStdString();
    }
}

using Report = std::function<void(const QString&)>;

ParameterUnit unitOf(ToolValueKind kind) {
    return kind == ToolValueKind::angle   ? ParameterUnit::angle
           : kind == ToolValueKind::count ? ParameterUnit::number
                                          : ParameterUnit::length;
}

QString withUnit(double value, ParameterUnit unit) {
    if (unit == ParameterUnit::number) // N01b
        return QLocale().toString(value, 'g', 12);
    return QLocale().toString(value, 'f', 3) +
           (unit == ParameterUnit::angle ? QStringLiteral("°") : QStringLiteral(" mm"));
}

QString unitName(ParameterUnit unit) {
    return unit == ParameterUnit::angle    ? QObject::tr("ein Winkel")
           : unit == ParameterUnit::number ? QObject::tr("eine Zahl")
                                           : QObject::tr("eine Länge");
}

// N01b: the parameter a field of `feature` follows (an extrusion's distance and offset too).
std::optional<document::ParameterId> boundParameter(const application::CadSession& session,
                                                    document::FeatureId feature,
                                                    const std::string& field) {
    for (const auto& binding : session.parameterTable().bindings)
        if (binding.feature == feature && binding.field == field)
            return binding.parameter;
    const auto features = session.features();
    const auto found = std::ranges::find(features, feature, &document::FeatureDescriptor::id);
    if (found == features.end() || found->kind != document::FeatureKind::extrusion ||
        (field != "distance" && field != "offset"))
        return std::nullopt;
    const auto extrusion = session.extrusionParameters(feature);
    return field == "distance" ? extrusion.distanceParameter : extrusion.offsetParameter;
}

// N01b: the panel's field bindings go to the session; after OK the shell makes those the tool's
// command did not take (a binding alone changes no value, so the tool may commit nothing).
FieldBindingHooks fieldBindingHooks(application::CadSession& session,
                                    std::function<void()> changed) {
    FieldBindingHooks hooks;
    hooks.boundName = [&session](document::FeatureId feature, const std::string& field) {
        const auto id = boundParameter(session, feature, field);
        const auto* parameter =
            id ? document::findParameter(session.parameterTable(), *id) : nullptr;
        return parameter ? std::optional{QString::fromStdString(parameter->name)} : std::nullopt;
    };
    hooks.parameter = [&session](document::ParameterId id) {
        const auto* parameter = document::findParameter(session.parameterTable(), id);
        return parameter ? std::optional{std::pair{QString::fromStdString(parameter->name),
                                                   parameter->value}}
                         : std::nullopt;
    };
    hooks.requested = [&session](const std::vector<application::FieldBindingRequest>& requests) {
        session.pendingFieldBindings() = requests;
    };
    hooks.accepted = [&session, changed = std::move(changed)](
                         const std::vector<application::FieldBindingRequest>& requests) {
        session.pendingFieldBindings().clear();
        bool any = false;
        for (const auto& request : requests)
            if (request.feature &&
                boundParameter(session, *request.feature, request.field) != request.parameter)
                any = session.bindFeatureField(*request.feature, request.field, request.parameter)
                              .status() != CommandStatus::rejected ||
                      any;
        if (any)
            changed();
    };
    return hooks;
}

// N01b: "=Name" in the property fields of the selected feature binds at once (one undo step).
PropertyBindingHandlers propertyBindings(application::CadSession& session, ToolPanelHost& panel,
                                         Report report, std::function<void()> changed) {
    PropertyBindingHandlers handlers;
    handlers.boundName = [&session](const std::string& field) -> std::optional<QString> {
        const auto feature = session.selectedFeature();
        const auto id = feature ? boundParameter(session, *feature, field) : std::nullopt;
        const auto* parameter =
            id ? document::findParameter(session.parameterTable(), *id) : nullptr;
        return parameter ? std::optional{QString::fromStdString(parameter->name)} : std::nullopt;
    };
    handlers.requested = [&session, &panel, report, changed = std::move(changed)](
                             const std::string& field, std::optional<QString> name, double value) {
        const auto feature = session.selectedFeature();
        if (!feature)
            return;
        std::optional<document::ParameterId> id;
        if (name) {
            const auto resolved = panel.resolveParameter(*name, ToolValueKind::length, value);
            if (!resolved)
                return;
            id = resolved->id;
        }
        const auto result = session.bindFeatureField(*feature, field, id);
        if (result.status() == CommandStatus::rejected &&
            result.reason() != application::RejectionReason::noChange)
            report(commandFeedbackMessage(result));
        changed();
    };
    handlers.lengthNames = [&session] {
        QStringList names;
        for (const auto& entry : session.parameterTable().values)
            if (entry.unit == ParameterUnit::length)
                names.append(QString::fromStdString(entry.name));
        return names;
    };
    return handlers;
}

std::optional<ResolvedParameter> created(const application::CadSession& session,
                                         const std::string& name) {
    for (const auto& entry : session.parameterTable().values)
        if (entry.name == name)
            return ResolvedParameter{entry.id, true};
    return std::nullopt;
}

// Where a new parameter is asked for and a refusal is told.
struct Asking final {
    application::CadSession& session;
    QWidget& parent;
    Report report;
};

// "Den Parameter ... anlegen?" answered with Yes; then the parameter is created.
std::optional<ResolvedParameter> createAfterQuestion(const Asking& asking, const QString& question,
                                                     const document::DocumentParameter& draft) {
    QMessageBox ask(QMessageBox::Question, QObject::tr("Parameter anlegen"), question,
                    QMessageBox::Yes | QMessageBox::No, &asking.parent);
    ask.setObjectName(QStringLiteral("createParameterQuestion"));
    ask.setDefaultButton(QMessageBox::Yes);
    if (ask.exec() != QMessageBox::Yes)
        return std::nullopt;
    const auto result =
        asking.session.createParameter(draft.name, draft.unit, draft.value, {}, draft.expression);
    if (result.status() != CommandStatus::committed) {
        asking.report(commandFeedbackMessage(result));
        return std::nullopt;
    }
    return created(asking.session, draft.name);
}

// An existing parameter of the field's unit, or a new one after a question.
std::optional<ResolvedParameter> namedParameter(const Asking& asking, const QString& name,
                                                ParameterUnit unit, double value) {
    for (const auto& entry : asking.session.parameterTable().values)
        if (QString::fromStdString(entry.name).compare(name, Qt::CaseInsensitive) == 0) {
            if (entry.unit == unit)
                return ResolvedParameter{entry.id, false};
            asking.report(QObject::tr("„%1“ ist %2, hier wird %3 gebraucht")
                              .arg(name, unitName(entry.unit), unitName(unit)));
            return std::nullopt;
        }
    return createAfterQuestion(
        asking, QObject::tr("Den Parameter „%1“ = %2 anlegen?").arg(name, withUnit(value, unit)),
        {document::ParameterId{0}, name.toStdString(), unit, value});
}

// N01: "=A*2" in a field: a new parameter "AusdruckN" holding the expression, after a question.
std::optional<ResolvedParameter>
expressionParameter(const Asking& asking, const QString& expression, ParameterUnit unit) {
    const auto name = freeName(asking.session.parameterTable(), QStringLiteral("Ausdruck"));
    return createAfterQuestion(
        asking,
        QObject::tr("Für den Ausdruck „%1“ den Parameter „%2“ anlegen?")
            .arg(expression, QString::fromStdString(name)),
        {document::ParameterId{0}, name, unit, 0, {}, expression.toStdString()});
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
    panel->setHandlers(
        {[this, run](document::ParameterId id, const QString& name) {
             static_cast<void>(run(impl_->session.renameParameter(id, name.toStdString())));
         },
         [this, run](document::ParameterId id, double value) {
             static_cast<void>(run(impl_->session.setParameterValue(id, value)));
         },
         [this, run](ParameterUnit unit) {
             const auto name = freeName(impl_->session.parameterTable());
             if (!run(impl_->session.createParameter(name, unit,
                                                     unit == ParameterUnit::angle ? 90.0 : 10.0)))
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
    // N01: "=A*2" in a value cell sets the parameter's expression.
    panel->setExpressionHandler([run, this](document::ParameterId id, const QString& expression) {
        static_cast<void>(run(impl_->session.setParameterExpression(id, expression.toStdString())));
    });

    // "=Name" in the fields of the tools: names of the field's unit, a new one after a question.
    impl_->toolPanel->setParameterNames([this](ToolValueKind kind) {
        QStringList names;
        for (const auto& entry : impl_->session.parameterTable().values)
            if (entry.unit == unitOf(kind))
                names.append(QString::fromStdString(entry.name));
        return names;
    });
    impl_->toolPanel->setParameterResolver([this](
                                               const QString& text, ToolValueKind kind,
                                               double value) -> std::optional<ResolvedParameter> {
        const Report report = [this](const QString& message) { statusBar()->showMessage(message); };
        static const QRegularExpression plainName(QStringLiteral("^[A-Za-z][A-Za-z0-9_]*$"));
        const auto resolved =
            plainName.match(text).hasMatch()
                ? namedParameter({impl_->session, *this, report}, text, unitOf(kind), value)
                : expressionParameter({impl_->session, *this, report}, text, unitOf(kind));
        if (resolved && resolved->created)
            syncParameters();
        return resolved;
    });
    const auto changed = [this] {
        refreshScene();
        syncParameters();
    };
    impl_->toolPanel->setFieldBindingHooks(fieldBindingHooks(impl_->session, changed));
    impl_->propertyPanel->setBindingHandlers(propertyBindings(
        impl_->session, *impl_->toolPanel,
        [this](const QString& message) { statusBar()->showMessage(message); }, changed));
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
