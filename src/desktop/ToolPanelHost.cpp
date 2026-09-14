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
        {"horizontal", QObject::tr("Horizontal")},
        {"vertical", QObject::tr("Vertical")},
        {"free", QObject::tr("Free")},
        {"parallel", QObject::tr("Parallel")},
        {"perpendicular", QObject::tr("Perpendicular")},
        {"collinear", QObject::tr("Collinear")},
        {"coincident", QObject::tr("Coincident")},
        {"horizontal_points", QObject::tr("Align H")},
        {"vertical_points", QObject::tr("Align V")},
        {"symmetric", QObject::tr("Symmetric")},
        {"midpoint", QObject::tr("Midpoint")},
        {"perpendicular_bisector", QObject::tr("Bisector")},
        {"point_on_object", QObject::tr("Point on object")},
        {"fixed", QObject::tr("Fix")},
        {"equal", QObject::tr("Equal")},
        {"tangent", QObject::tr("Tangent")},
        {"concentric", QObject::tr("Concentric")},
        {"diameter", QObject::tr("Diameter")},
        {"point_distance", QObject::tr("Distance")},
        {"point_line_distance", QObject::tr("Point–line")},
        {"point_distance_x", QObject::tr("Distance X")},
        {"point_distance_y", QObject::tr("Distance Y")},
        {"angle", QObject::tr("Angle")},
        {"construction", QObject::tr("Construction")},
        {"lock_point", QObject::tr("Lock point")},
        {"unlock_point", QObject::tr("Unlock point")},
        {"remove_constraint", QObject::tr("Remove constraint")},
        {"undo_sketch", QObject::tr("Undo")},
        {"redo_sketch", QObject::tr("Redo")},
        {"trim", QObject::tr("Trim")},
        {"extend", QObject::tr("Extend")},
        {"split", QObject::tr("Split")},
        {"fillet", QObject::tr("Fillet")},
        {"offset", QObject::tr("Offset")},
        {"mirror", QObject::tr("Mirror")},
        {"linear_pattern", QObject::tr("Linear pattern")},
        {"circular_pattern", QObject::tr("Circular pattern")},
        {"delete", QObject::tr("Delete")}};
    const auto match = std::ranges::find(labels, id, &decltype(labels)::value_type::first);
    return match == labels.end() ? fallback : match->second;
}

} // namespace

ToolPanelHost::ToolPanelHost(QWidget* parent) : QDockWidget(parent) {
    setObjectName(QStringLiteral("activeToolDock"));
    setWindowTitle(tr("Tool"));
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    setMinimumWidth(270);
    setMaximumWidth(360);
    content_ = new QWidget(this);
    auto* layout = new QVBoxLayout(content_);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(7);
    guidance_ = new QLabel(content_);
    guidance_->setWordWrap(true);
    guidance_->setObjectName(QStringLiteral("toolGuidance"));
    parameterContainer_ = new QGroupBox(tr("Active values"), content_);
    parameterLayout_ = new QFormLayout(parameterContainer_);
    parameterLayout_->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    actionTabs_ = new QTabWidget(content_);
    actionTabs_->setDocumentMode(true);
    auto* buttons = new QHBoxLayout();
    acceptButton_ = new QPushButton(tr("OK"), content_);
    cancelButton_ = new QPushButton(tr("Cancel"), content_);
    errorCommitButton_ = new QPushButton(tr("Keep editable errors"), content_);
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
    layout->addLayout(buttons);
    layout->addWidget(errorCommitButton_);
    setWidget(content_);
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
    setWindowTitle(definition_.title);
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
            parameterLayout_->addRow(parameter.label, choices);
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
        const QString group = action.group.isEmpty() ? tr("Actions") : action.group;
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
}

void ToolPanelHost::end() {
    setErrorCommitHandler({});
    hide();
    clearParameters();
    definition_ = {};
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
