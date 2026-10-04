#include "ToolPanelHost.hpp"

#include "MeasurementInput.hpp"

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
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <utility>

namespace voelund::desktop {
namespace {

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
        {"fillet", QObject::tr("Abrunden")},
        {"offset", QObject::tr("Versetzen")},
        {"mirror", QObject::tr("Spiegeln")},
        {"linear_pattern", QObject::tr("Lineares Muster")},
        {"circular_pattern", QObject::tr("Kreismuster")},
        {"delete", QObject::tr("Löschen")}};
    const auto match = std::ranges::find(labels, id, &decltype(labels)::value_type::first);
    return match == labels.end() ? fallback : match->second;
}

} // namespace

ToolPanelHost::ToolPanelHost(QWidget* host)
    : FloatingCard(*host, QStringLiteral("activeToolPanel")) {
    setTitle(tr("Werkzeug"));
    content_ = new QWidget(this);
    auto* layout = new QVBoxLayout(content_);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(7);
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
    errorCommitButton_ = new QPushButton(tr("Bearbeitbare Fehler behalten"), content_);
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
    acceptShortcut_ = new QShortcut(QKeySequence(Qt::Key_Return), this);
    connect(acceptShortcut_, &QShortcut::activated, this, [this]() {
        if (acceptButton_->isEnabled())
            acceptButton_->click();
    });
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
    connect(redoShortcut_, &QShortcut::activated, this,
            [triggerAction]() { triggerAction("redo_sketch"); });
    acceptButton_->setDefault(true);
    buttons->addStretch();
    buttons->addWidget(acceptButton_);
    buttons->addWidget(cancelButton_);
    layout->addWidget(guidance_);
    layout->addWidget(parameterContainer_);
    layout->addWidget(actionTabs_, 1);
    layout->addStretch();
    footer().addLayout(buttons);
    footer().addWidget(errorCommitButton_);
    body().addWidget(content_);
    connect(acceptButton_, &QPushButton::clicked, this, [this]() {
        if (definition_.accept) {
            definition_.accept();
        }
    });
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
    for (const ToolLengthParameter& parameter : definition_.lengthParameters) {
        QDoubleSpinBox* input = makeLengthInput(content_);
        input->setRange(parameter.minimum, parameter.maximum);
        input->setValue(parameter.value);
        const std::string parameterId = parameter.id;
        connect(input, &QDoubleSpinBox::valueChanged, this, [this, parameterId](double value) {
            if (definition_.parameterChanged) {
                definition_.parameterChanged(parameterId, value);
            }
        });
        parameterLayout_->addRow(parameter.label, input);
        lengthInputs_.emplace_back(parameter.id, input);
    }
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
                        if (definition_.choiceChanged)
                            definition_.choiceChanged(parameterId, index);
                    });
            // The tool choice spans the card; its buttons name themselves.
            parameterLayout_->addRow(choices);
            continue;
        }
        auto* input = new QComboBox(content_);
        for (const QString& choice : parameter.choices) {
            input->addItem(choice);
        }
        input->setCurrentIndex(parameter.currentIndex);
        const std::string parameterId = parameter.id;
        connect(input, &QComboBox::currentIndexChanged, this, [this, parameterId](int index) {
            if (definition_.choiceChanged) {
                definition_.choiceChanged(parameterId, index);
            }
        });
        parameterLayout_->addRow(parameter.label, input);
        choiceInputs_.emplace_back(parameter.id, input);
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
    actionTabs_->setVisible(!definition_.actions.empty());
    acceptButton_->setEnabled(false);
    show();
    raise();
    if (activityHandler_)
        activityHandler_();
}

void ToolPanelHost::end() {
    setErrorCommitHandler({});
    hide();
    clearParameters();
    definition_ = {};
    if (activityHandler_)
        activityHandler_();
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

void ToolPanelHost::setLengthValue(const std::string& parameterId, double value) {
    for (const auto& [id, input] : lengthInputs_) {
        if (id == parameterId) {
            const QSignalBlocker blocker(input);
            input->setValue(value);
            return;
        }
    }
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
        return;
    }
}

void ToolPanelHost::setActionEnabled(const std::string& actionId, bool enabled) {
    for (const auto& [id, input] : actionInputs_) {
        if (id == actionId) {
            input->setEnabled(enabled);
            return;
        }
    }
}

void ToolPanelHost::clearParameters() {
    while (QLayoutItem* item = parameterLayout_->takeAt(0)) {
        if (QWidget* itemWidget = item->widget()) {
            itemWidget->deleteLater();
        }
        delete item;
    }
    lengthInputs_.clear();
    choiceInputs_.clear();
    actionInputs_.clear();
    while (actionTabs_->count() > 0) {
        QWidget* page = actionTabs_->widget(0);
        actionTabs_->removeTab(0);
        page->deleteLater();
    }
}

} // namespace voelund::desktop
