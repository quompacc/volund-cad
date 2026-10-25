// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "ToolPanelHost.hpp"

#include "MeasurementInput.hpp"

#include "ParameterInput.hpp"
#include <QAbstractSpinBox>
#include <QApplication>
#include <QButtonGroup>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QPushButton>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStyle>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <utility>

namespace voelund::desktop {
namespace {
// Enter's shortcut fires before the focused spin box sees it: typed text is taken first.
void applyTypedValue() {
    for (QWidget* at = QApplication::focusWidget(); at != nullptr; at = at->parentWidget())
        if (auto* spin = qobject_cast<QAbstractSpinBox*>(at))
            return spin->interpretText();
}
// A number field of a tool; bindable ones (PAR02, N01b) take "=Name" too.
QDoubleSpinBox* makeToolInput(QWidget* parent, const ToolLengthParameter& parameter) {
    if (parameter.bindable && parameter.kind != ToolValueKind::factor)
        return makeParameterInput(parent, parameter.kind == ToolValueKind::angle,
                                  parameter.kind == ToolValueKind::count);
    return parameter.kind == ToolValueKind::angle    ? makeAngleInput(parent)
           : parameter.kind == ToolValueKind::count  ? makeCountInput(parent)
           : parameter.kind == ToolValueKind::factor ? makeFactorInput(parent)
                                                     : makeLengthInput(parent);
}
} // namespace
QString compactActionLabel(const std::string& id, const QString& fallback) {
    static const std::vector<std::pair<std::string, QString>> labels{
        {"horizontal", QObject::tr("Waagrecht")},
        {"vertical", QObject::tr("Senkrecht")},
        {"free", QObject::tr("Frei")},
        {"parallel", QObject::tr("Parallel")},
        {"perpendicular", QObject::tr("Rechtwinklig")},
        {"collinear", QObject::tr("Kollinear")},
        {"coincident", QObject::tr("Deckungsgleich")},
        {"horizontal_points", QObject::tr("Waagrecht ausr.")},
        {"vertical_points", QObject::tr("Senkrecht ausr.")},
        {"symmetric", QObject::tr("Symmetrisch")},
        {"midpoint", QObject::tr("Mittelpunkt")},
        {"perpendicular_bisector", QObject::tr("Halbierend")},
        {"point_on_object", QObject::tr("Punkt auf Objekt")},
        {"fixed", QObject::tr("Fixieren")},
        {"equal", QObject::tr("Gleich")},
        {"tangent", QObject::tr("Tangential")},
        {"concentric", QObject::tr("Konzentrisch")},
        {"diameter", QObject::tr("Durchmesser")},
        {"radius", QObject::tr("Radius")},
        {"point_distance", QObject::tr("Distanz")},
        {"point_line_distance", QObject::tr("Punkt–Linie")},
        {"point_distance_x", QObject::tr("Abstand X")},
        {"point_distance_y", QObject::tr("Abstand Y")},
        {"angle", QObject::tr("Winkel")},
        {"construction", QObject::tr("Konstruktion")},
        {"lock_point", QObject::tr("Punkt sperren")},
        {"unlock_point", QObject::tr("Punkt freigeben")},
        {"remove_constraint", QObject::tr("Bedingung entfernen")},
        {"undo_sketch", QObject::tr("Rückgängig")},
        {"redo_sketch", QObject::tr("Wiederholen")},
        {"trim", QObject::tr("Stutzen")},
        {"extend", QObject::tr("Verlängern")},
        {"split", QObject::tr("Teilen")},
        {"fillet", QObject::tr("Verrunden")},
        {"offset", QObject::tr("Versetzen")},
        {"mirror", QObject::tr("Spiegeln")},
        {"linear_pattern", QObject::tr("Lineares Muster")},
        {"circular_pattern", QObject::tr("Kreismuster")},
        {"edit_pattern", QObject::tr("Muster ändern")},
        {"dissolve_pattern", QObject::tr("Muster auflösen")},
        {"delete", QObject::tr("Löschen")}};
    const auto match = std::ranges::find(labels, id, &decltype(labels)::value_type::first);
    return match == labels.end() ? fallback : match->second;
}

ToolPanelHost::ToolPanelHost(QWidget* host)
    : FloatingCard(*host, QStringLiteral("activeToolPanel")) {
    setTitle(tr("Werkzeug"));
    content_ = new QWidget(this);
    auto* layout = new QVBoxLayout(content_);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(7);
    status_ = new QLabel(content_);
    status_->setObjectName(QStringLiteral("toolStatus"));
    status_->setWordWrap(true);
    status_->hide();
    guidance_ = new QLabel(content_);
    guidance_->setWordWrap(true);
    guidance_->setObjectName(QStringLiteral("toolGuidance"));
    parameterContainer_ = new QGroupBox(tr("Aktive Werte"), content_);
    parameterLayout_ = new QFormLayout(parameterContainer_);
    parameterLayout_->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    actionTabs_ = new QTabWidget(content_);
    actionTabs_->setDocumentMode(true);
    actionTabs_->setUsesScrollButtons(true); // Groups stay reachable in the narrow card.
    actionTabs_->setElideMode(Qt::ElideNone);
    auto* buttons = new QHBoxLayout();
    acceptButton_ = new QPushButton(tr("OK"), content_);
    cancelButton_ = new QPushButton(tr("Abbrechen"), content_);
    acceptButton_->setObjectName(QStringLiteral("toolAcceptButton"));
    cancelButton_->setObjectName(QStringLiteral("toolCancelButton"));
    errorCommitButton_ = new QPushButton(tr("Mit Folgefehlern übernehmen"), content_);
    errorCommitButton_->setObjectName(QStringLiteral("keepSketchErrorsButton"));
    errorCommitButton_->hide();
    connect(errorCommitButton_, &QPushButton::clicked, this, [this] {
        const auto handler = errorCommitHandler_;
        if (handler)
            handler();
    });
    escapeShortcut_ = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    undoShortcut_ = new QShortcut(QKeySequence::Undo, this);
    redoShortcut_ = new QShortcut(QKeySequence::Redo, this);
    // Enter confirms like OK when the tool allows it (K04); a spin box first applies its value.
    // S13: a tool with its own Enter step (sketch: end the polyline) confirms with Ctrl+Enter.
    const auto accept = [this]() {
        applyTypedValue();
        if (acceptButton_->isEnabled())
            acceptButton_->click();
    };
    const auto enter = [this, accept]() {
        applyTypedValue();
        if (definition_.returnPressed)
            definition_.returnPressed();
        else
            accept();
    };
    acceptShortcut_ = new QShortcut(QKeySequence(Qt::Key_Return), this);
    connect(acceptShortcut_, &QShortcut::activated, this, enter);
    connect(new QShortcut(QKeySequence(Qt::Key_Enter), this), &QShortcut::activated, this, enter);
    for (const auto key : {Qt::CTRL | Qt::Key_Return, Qt::CTRL | Qt::Key_Enter})
        connect(new QShortcut(QKeySequence(key), this), &QShortcut::activated, this, accept);
    connect(escapeShortcut_, &QShortcut::activated, this, [this]() {
        if (definition_.stepBack)
            definition_.stepBack();
        else if (definition_.cancel)
            definition_.cancel();
    });
    const auto triggerAction = [this](const std::string& id) {
        const auto match =
            std::ranges::find(actionInputs_, id, &decltype(actionInputs_)::value_type::first);
        if (match != actionInputs_.end() && match->second->isEnabled())
            match->second->click();
    };
    connect(undoShortcut_, &QShortcut::activated, this,
            [triggerAction]() { triggerAction("undo_sketch"); });
    // S13: Delete removes the selected sketch elements or relation.
    connect(new QShortcut(QKeySequence::Delete, this), &QShortcut::activated, this,
            [triggerAction]() { triggerAction("delete"); });
    connect(redoShortcut_, &QShortcut::activated, this,
            [triggerAction]() { triggerAction("redo_sketch"); });
    acceptButton_->setDefault(true);
    buttons->addStretch();
    buttons->addWidget(acceptButton_);
    buttons->addWidget(cancelButton_);
    layout->addWidget(status_);
    layout->addWidget(guidance_);
    layout->addWidget(parameterContainer_);
    layout->addWidget(actionTabs_, 1);
    layout->addStretch();
    footer().addLayout(buttons);
    footer().addWidget(errorCommitButton_);
    body().addWidget(content_);
    connect(acceptButton_, &QPushButton::clicked, this, &ToolPanelHost::acceptPressed);
    connect(cancelButton_, &QPushButton::clicked, this, [this]() {
        if (definition_.cancel) {
            definition_.cancel();
        }
    });
}

void ToolPanelHost::begin(ToolPanelDefinition definition) {
    setErrorCommitHandler({});
    clearParameters();
    definition_ = std::move(definition);
    setTitle(definition_.title);
    guidance_->setText(definition_.guidance);
    setStatus({});
    fieldRequests_.clear();
    for (const ToolLengthParameter& parameter : definition_.lengthParameters)
        addLengthInput(parameter);
    for (const ToolChoiceParameter& parameter : definition_.choiceParameters) {
        if (parameter.id == "tool") {
            auto* choices = new QWidget(parameterContainer_);
            auto* choicesLayout = new QGridLayout(choices);
            choicesLayout->setContentsMargins(0, 0, 0, 0);
            choicesLayout->setSpacing(3);
            auto* group = new QButtonGroup(choices);
            group->setExclusive(true);
            for (std::size_t index = 0; index < parameter.choices.size(); ++index) {
                auto* button = new QToolButton(choices);
                button->setText(parameter.choices[index]);
                button->setCheckable(true);
                button->setChecked(static_cast<int>(index) == parameter.currentIndex);
                button->setToolButtonStyle(Qt::ToolButtonTextOnly);
                button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
                group->addButton(button, static_cast<int>(index));
                choicesLayout->addWidget(button, static_cast<int>(index / 3),
                                         static_cast<int>(index % 3));
            }
            connect(group, &QButtonGroup::idClicked, this,
                    [this, parameterId = parameter.id](int index) {
                        chooseOption(parameterId, index);
                    });
            // The tool choice spans the card; its buttons name themselves.
            parameterLayout_->addRow(choices);
            choiceIndices_.emplace_back(parameter.id, parameter.currentIndex);
            toolGroup_ = group;
            toolChoices_ = choices;
            choices->setVisible(!toolsInRail_);
            continue;
        }
        auto* input = new QComboBox(content_);
        for (const QString& choice : parameter.choices) {
            input->addItem(choice);
        }
        input->setCurrentIndex(parameter.currentIndex);
        input->setObjectName(QString::fromStdString("toolChoice_" + parameter.id));
        const std::string parameterId = parameter.id;
        connect(input, &QComboBox::currentIndexChanged, this,
                [this, parameterId](int index) { chooseOption(parameterId, index); });
        parameterLayout_->addRow(parameter.label, input);
        choiceInputs_.emplace_back(parameter.id, input);
        choiceIndices_.emplace_back(parameter.id, parameter.currentIndex);
    }
    struct ActionGroup final {
        QString name;
        QGridLayout* layout;
        int count;
    };
    std::vector<ActionGroup> actionGroups;
    for (const ToolAction& action : definition_.actions) {
        const QString group = action.group.isEmpty() ? tr("Aktionen") : action.group;
        auto match = std::ranges::find_if(
            actionGroups, [&group](const auto& item) { return item.name == group; });
        if (match == actionGroups.end()) {
            auto* page = new QWidget(actionTabs_);
            auto* pageLayout = new QGridLayout(page);
            pageLayout->setContentsMargins(4, 4, 4, 4);
            pageLayout->setSpacing(4);
            pageLayout->setAlignment(Qt::AlignTop);
            actionTabs_->addTab(page, group);
            actionGroups.push_back({group, pageLayout, 0});
            match = std::prev(actionGroups.end());
        }
        auto* button = new QPushButton(compactActionLabel(action.id, action.label), content_);
        button->setToolTip(action.label);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        const std::string actionId = action.id;
        connect(button, &QPushButton::clicked, this, [this, actionId]() {
            if (definition_.actionTriggered)
                definition_.actionTriggered(actionId);
        });
        match->layout->addWidget(button, match->count / 2, match->count % 2);
        ++match->count;
        actionInputs_.emplace_back(action.id, button);
    }
    actionTabs_->setVisible(!definition_.actions.empty() && !toolsInRail_);
    acceptButton_->setEnabled(false);
    active_ = true;
    syncParameterGroup();
    show();
    raise();
    if (activityHandler_)
        activityHandler_();
}

void ToolPanelHost::acceptPressed() {
    if (!definition_.accept)
        return;
    const auto requests = fieldRequests_;
    definition_.accept();
    if (!active_ && !requests.empty() && fieldHooks_.accepted)
        fieldHooks_.accepted(requests);
}

void ToolPanelHost::addLengthInput(const ToolLengthParameter& parameter) {
    QDoubleSpinBox* input = makeToolInput(content_, parameter);
    input->setRange(parameter.minimum, parameter.maximum);
    input->setValue(parameter.value);
    const std::string parameterId = parameter.id;
    connect(input, &QDoubleSpinBox::valueChanged, this, [this, parameterId](double value) {
        if (definition_.parameterChanged)
            definition_.parameterChanged(parameterId, value);
    });
    connect(input, &QDoubleSpinBox::editingFinished, this, [this, parameterId] {
        if (definition_.parameterFinished)
            definition_.parameterFinished(parameterId);
    });
    if (auto* bound = dynamic_cast<ParameterSpinBox*>(input)) {
        const auto kind = parameter.kind;
        bound->setNamesProvider([this, kind] { return names_ ? names_(kind) : QStringList{}; });
        auto boundName = parameter.boundName;
        if (!boundName && definition_.editedFeature && fieldHooks_.boundName &&
            !definition_.parameterBound)
            boundName = fieldHooks_.boundName(*definition_.editedFeature, parameterId);
        bound->setBound(boundName);
        bound->setBindingRequested([this, parameterId, kind](std::optional<QString> name) {
            bindField(parameterId, kind, std::move(name));
        });
    }
    input->setObjectName(QString::fromStdString("toolLength_" + parameter.id));
    parameterLayout_->addRow(parameter.label, input);
    lengthInputs_.emplace_back(parameter.id, input);
}

// "=Name" in a bindable field (nullopt: a number typed into a bound one).
void ToolPanelHost::bindField(const std::string& id, ToolValueKind kind,
                              std::optional<QString> name) {
    double current = 0.0;
    for (const auto& [field, input] : lengthInputs_)
        if (field == id)
            current = input->value();
    std::optional<ResolvedParameter> resolved;
    if (name) {
        resolved = resolveParameter(*name, kind, current);
        if (!resolved)
            return;
    }
    if (definition_.parameterBound) // PAR02: the tool binds itself
        definition_.parameterBound(id, resolved);
    else
        bindThroughDocument(id, resolved);
}

void ToolPanelHost::bindThroughDocument(const std::string& id,
                                        std::optional<ResolvedParameter> resolved) {
    if (resolved && resolved->created && definition_.parameterTableChanged)
        definition_.parameterTableChanged();
    std::erase_if(fieldRequests_, [&](const auto& request) { return request.field == id; });
    const auto parameter =
        resolved ? std::optional{resolved->id} : std::optional<document::ParameterId>{};
    fieldRequests_.push_back({definition_.editedFeature, id, parameter});
    showBinding(id, parameter);
    requestFields();
}

// The field shows the parameter's name and value; the tool previews the value.
void ToolPanelHost::showBinding(const std::string& id, std::optional<document::ParameterId> bound) {
    const auto parameter =
        bound && fieldHooks_.parameter ? fieldHooks_.parameter(*bound) : std::nullopt;
    setLengthBinding(id, parameter ? std::optional{parameter->first} : std::nullopt);
    if (!parameter)
        return;
    setLengthValue(id, parameter->second);
    if (definition_.parameterChanged)
        definition_.parameterChanged(id, parameter->second);
}

void ToolPanelHost::requestFields() {
    if (fieldHooks_.requested)
        fieldHooks_.requested(fieldRequests_);
}

void ToolPanelHost::setFieldBindingHooks(FieldBindingHooks hooks) {
    fieldHooks_ = std::move(hooks);
}

ToolPanelDefinition withFieldBindings(ToolPanelDefinition definition,
                                      std::optional<document::FeatureId> edited,
                                      std::function<void()> tableChanged) {
    for (auto& parameter : definition.lengthParameters)
        parameter.bindable = parameter.kind != ToolValueKind::factor;
    definition.editedFeature = edited;
    definition.parameterTableChanged = std::move(tableChanged);
    return definition;
}

void ToolPanelHost::end() {
    fieldRequests_.clear();
    requestFields();
    setErrorCommitHandler({});
    hide();
    clearParameters();
    definition_ = {};
    active_ = false;
    if (activityHandler_)
        activityHandler_();
}

void ToolPanelHost::setToolsInRail(bool inRail) {
    toolsInRail_ = inRail;
    if (toolChoices_ != nullptr)
        toolChoices_->setVisible(!inRail);
    actionTabs_->setVisible(!definition_.actions.empty() && !inRail);
    syncParameterGroup();
}

void ToolPanelHost::syncParameterGroup() {
    // Hide "Aktive Werte" when none of its rows is shown, so the card stays compact.
    bool any = toolChoices_ != nullptr && !toolsInRail_;
    for (const auto& [id, input] : lengthInputs_)
        any = any || !input->isHidden();
    any = any || !choiceInputs_.empty();
    parameterContainer_->setVisible(any);
    refit();
}

void ToolPanelHost::setStateHandler(std::function<void()> handler) {
    stateHandler_ = std::move(handler);
}

void ToolPanelHost::chooseOption(const std::string& choiceId, int index) {
    for (auto& [id, current] : choiceIndices_)
        if (id == choiceId)
            current = index;
    if (toolGroup_ != nullptr && choiceId == "tool")
        if (auto* button = toolGroup_->button(index))
            button->setChecked(true);
    // A choice the tool or a test sets shows in its field without a second notification.
    for (auto& [id, input] : choiceInputs_)
        if (id == choiceId && input->currentIndex() != index) {
            const QSignalBlocker blocker(input);
            input->setCurrentIndex(index);
        }
    if (definition_.choiceChanged)
        definition_.choiceChanged(choiceId, index);
    if (stateHandler_)
        stateHandler_();
}

void ToolPanelHost::setActivityHandler(std::function<void()> handler) {
    activityHandler_ = std::move(handler);
}

void ToolPanelHost::setAcceptEnabled(bool enabled) { acceptButton_->setEnabled(enabled); }

void ToolPanelHost::setErrorCommitHandler(std::function<void()> handler) {
    errorCommitHandler_ = std::move(handler);
    errorCommitButton_->setVisible(static_cast<bool>(errorCommitHandler_));
}

void ToolPanelHost::setGuidance(const QString& guidance) { guidance_->setText(guidance); }

void ToolPanelHost::setStatus(const QString& status, bool good) {
    status_->setText(status);
    status_->setVisible(!status.isEmpty());
    status_->setProperty("good", good);
    status_->style()->unpolish(status_);
    status_->style()->polish(status_);
}

void ToolPanelHost::setLengthValue(const std::string& parameterId, double value) {
    for (const auto& [id, input] : lengthInputs_) {
        if (id == parameterId) {
            const QSignalBlocker blocker(input);
            input->setValue(value);
            return;
        }
    }
}

void ToolPanelHost::focusLength(const std::string& parameterId) {
    for (const auto& [id, input] : lengthInputs_)
        if (id == parameterId && input->isEnabled()) {
            input->setFocus(Qt::OtherFocusReason);
            input->selectAll();
            announceMainField(input);
            return;
        }
}

void ToolPanelHost::setLengthBinding(const std::string& parameterId, std::optional<QString> name) {
    for (const auto& [id, input] : lengthInputs_)
        if (auto* bound = dynamic_cast<ParameterSpinBox*>(input); bound && id == parameterId) {
            const QSignalBlocker blocker(input);
            bound->setBound(std::move(name));
            return;
        }
}

void ToolPanelHost::setParameterResolver(ParameterResolver resolver) {
    resolver_ = std::move(resolver);
}

void ToolPanelHost::setParameterNames(std::function<QStringList(ToolValueKind)> names) {
    names_ = std::move(names);
}

std::optional<ResolvedParameter>
ToolPanelHost::resolveParameter(const QString& name, ToolValueKind kind, double value) const {
    return resolver_ ? resolver_(name, kind, value) : std::nullopt;
}

void ToolPanelHost::setLengthEnabled(const std::string& parameterId, bool enabled) {
    for (const auto& [id, input] : lengthInputs_) {
        if (id == parameterId) {
            input->setEnabled(enabled);
            return;
        }
    }
}

void ToolPanelHost::setLengthVisible(const std::string& parameterId, bool visible) {
    for (const auto& [id, input] : lengthInputs_) {
        if (id != parameterId)
            continue;
        input->setVisible(visible);
        if (QWidget* label = parameterLayout_->labelForField(input))
            label->setVisible(visible);
        syncParameterGroup();
        return;
    }
}

void ToolPanelHost::setChoiceEnabled(const std::string& choiceId, bool enabled) {
    for (const auto& [id, input] : choiceInputs_)
        if (id == choiceId)
            input->setEnabled(enabled);
}

void ToolPanelHost::setChoiceVisible(const std::string& choiceId, bool visible) {
    for (const auto& [id, input] : choiceInputs_) {
        if (id != choiceId)
            continue;
        input->setVisible(visible);
        if (QWidget* label = parameterLayout_->labelForField(input))
            label->setVisible(visible);
        syncParameterGroup();
        return;
    }
}

void ToolPanelHost::setActionEnabled(const std::string& actionId, bool enabled) {
    for (const auto& [id, input] : actionInputs_) {
        if (id == actionId) {
            if (input->isEnabled() == enabled)
                return;
            input->setEnabled(enabled);
            if (stateHandler_)
                stateHandler_();
            return;
        }
    }
}

void ToolPanelHost::clearParameters() {
    announceMainField(nullptr); // until the next tool names its main value
    while (QLayoutItem* item = parameterLayout_->takeAt(0)) {
        // Hidden at once: the deferred deletion may come after the next session's fields.
        if (QWidget* itemWidget = item->widget()) {
            itemWidget->hide();
            itemWidget->deleteLater();
        }
        delete item;
    }
    lengthInputs_.clear();
    choiceInputs_.clear();
    actionInputs_.clear();
    choiceIndices_.clear();
    toolGroup_ = nullptr;
    toolChoices_ = nullptr;
    while (actionTabs_->count() > 0) {
        QWidget* page = actionTabs_->widget(0);
        actionTabs_->removeTab(0);
        page->deleteLater();
    }
}

} // namespace voelund::desktop
