#include "ToolPanelHost.hpp"

#include "MeasurementInput.hpp"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QPushButton>
#include <QShortcut>
#include <QSignalBlocker>
#include <QToolBox>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <utility>

namespace voelund::desktop {

ToolPanelHost::ToolPanelHost(QWidget* parent) : QDockWidget(parent) {
    setObjectName(QStringLiteral("activeToolDock"));
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    content_ = new QWidget(this);
    auto* layout = new QVBoxLayout(content_);
    guidance_ = new QLabel(content_);
    guidance_->setWordWrap(true);
    parameterLayout_ = new QFormLayout();
    actionToolBox_ = new QToolBox(content_);
    auto* buttons = new QHBoxLayout();
    acceptButton_ = new QPushButton(tr("OK"), content_);
    cancelButton_ = new QPushButton(tr("Cancel"), content_);
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
        const auto match = std::ranges::find(actionInputs_, id, &decltype(actionInputs_)::value_type::first);
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
    layout->addLayout(parameterLayout_);
    layout->addWidget(actionToolBox_);
    layout->addStretch();
    layout->addLayout(buttons);
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
    hide();
}

void ToolPanelHost::begin(ToolPanelDefinition definition) {
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
    std::vector<std::pair<QString, QVBoxLayout*>> actionGroups;
    for (const ToolAction& action : definition_.actions) {
        const QString group = action.group.isEmpty() ? tr("Actions") : action.group;
        auto match = std::ranges::find_if(
            actionGroups, [&group](const auto& item) { return item.first == group; });
        if (match == actionGroups.end()) {
            auto* page = new QWidget(actionToolBox_);
            auto* pageLayout = new QVBoxLayout(page);
            pageLayout->setContentsMargins(4, 4, 4, 4);
            actionToolBox_->addItem(page, group);
            actionGroups.emplace_back(group, pageLayout);
            match = std::prev(actionGroups.end());
        }
        auto* button = new QPushButton(action.label, content_);
        const std::string actionId = action.id;
        connect(button, &QPushButton::clicked, this, [this, actionId]() {
            if (definition_.actionTriggered)
                definition_.actionTriggered(actionId);
        });
        match->second->addWidget(button);
        actionInputs_.emplace_back(action.id, button);
    }
    actionToolBox_->setVisible(!definition_.actions.empty());
    acceptButton_->setEnabled(false);
    show();
    raise();
}

void ToolPanelHost::end() {
    hide();
    clearParameters();
    definition_ = {};
}

void ToolPanelHost::setAcceptEnabled(bool enabled) { acceptButton_->setEnabled(enabled); }

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
    while (actionToolBox_->count() > 0) {
        QWidget* page = actionToolBox_->widget(0);
        actionToolBox_->removeItem(0);
        page->deleteLater();
    }
}

} // namespace voelund::desktop
