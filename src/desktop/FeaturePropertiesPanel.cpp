#include "FeaturePropertiesPanel.hpp"
#include "MeasurementInput.hpp"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QWidget>

#include <utility>

namespace voelund::desktop {
namespace {

constexpr const char* moveLabels[] = {"Move X", "Move Y", "Move Z"};
constexpr const char* rotationLabels[] = {"Rotate X", "Rotate Y", "Rotate Z"};

} // namespace

FeaturePropertiesPanel::FeaturePropertiesPanel(QWidget* parent) : QStackedWidget(parent) {
    createBoxPanel();
    createCylinderPanel();
    createExtrusionPanel();
    emptyPanel_ = new QLabel(tr("No feature selected"), this);
    addWidget(boxPanel_);
    addWidget(cylinderPanel_);
    addWidget(extrusionPanel_);
    addWidget(emptyPanel_);
    QWidget* pages[] = {boxPanel_, cylinderPanel_, extrusionPanel_};
    for (int index = 0; index < 3; ++index) {
        auto* button = new QPushButton(tr("Keep editable errors"), pages[index]);
        button->setObjectName(QStringLiteral("keepEditableErrorsButton"));
        button->setToolTip(tr(
            "Keep the rejected parameter change as a repairable error. Undo remains available."));
        static_cast<QFormLayout*>(pages[index]->layout())->addRow(button);
        button->hide();
        errorCommitButtons_[index] = button;
        connect(button, &QPushButton::clicked, this, [this] {
            const auto handler = errorCommitHandler_;
            if (handler)
                handler();
        });
    }
    connectEditors();
}

void FeaturePropertiesPanel::createExtrusionPanel() {
    extrusionPanel_ = new QWidget(this);
    auto* layout = new QFormLayout(extrusionPanel_);
    extrusionDistanceInput_ = makeLengthInput(extrusionPanel_);
    extrusionDistanceInput_->setObjectName(QStringLiteral("extrusionDistanceInput"));
    extrusionDistanceInput_->setMinimum(0.001);
    extrusionErrorLabel_ = new QLabel(extrusionPanel_);
    extrusionErrorLabel_->setWordWrap(true);
    extrusionErrorLabel_->setStyleSheet(
        QStringLiteral("QLabel { color: #ffb4ab; background: #4b2024; border: 1px solid #b94a55; "
                       "border-radius: 3px; padding: 6px; }"));
    layout->addRow(tr("Distance"), extrusionDistanceInput_);
    layout->addRow(extrusionErrorLabel_);
}

void FeaturePropertiesPanel::createBoxPanel() {
    boxPanel_ = new QWidget(this);
    auto* boxLayout = new QFormLayout(boxPanel_);
    lengthInput_ = makeLengthInput(boxPanel_);
    widthInput_ = makeLengthInput(boxPanel_);
    boxHeightInput_ = makeLengthInput(boxPanel_);
    lengthInput_->setObjectName(QStringLiteral("boxLengthInput"));
    widthInput_->setObjectName(QStringLiteral("boxWidthInput"));
    boxHeightInput_->setObjectName(QStringLiteral("boxHeightInput"));
    for (QDoubleSpinBox*& input : boxTranslationInputs_) {
        input = makeLengthInput(boxPanel_, true);
    }
    for (int index = 0; index < 3; ++index) {
        boxRotationInputs_[index] = makeAngleInput(boxPanel_);
        boxRotationInputs_[index]->setObjectName(QStringLiteral("boxRotation%1Input").arg(index));
    }
    boxLayout->addRow(tr("Length"), lengthInput_);
    boxLayout->addRow(tr("Width"), widthInput_);
    boxLayout->addRow(tr("Height"), boxHeightInput_);
    for (int index = 0; index < 3; ++index) {
        boxLayout->addRow(tr(moveLabels[index]), boxTranslationInputs_[index]);
    }
    for (int index = 0; index < 3; ++index) {
        boxLayout->addRow(tr(rotationLabels[index]), boxRotationInputs_[index]);
    }
    boxErrorLabel_ = new QLabel(boxPanel_);
    boxErrorLabel_->setWordWrap(true);
    boxLayout->addRow(boxErrorLabel_);
}

void FeaturePropertiesPanel::createCylinderPanel() {
    cylinderPanel_ = new QWidget(this);
    auto* cylinderLayout = new QFormLayout(cylinderPanel_);
    radiusInput_ = makeLengthInput(cylinderPanel_);
    cylinderHeightInput_ = makeLengthInput(cylinderPanel_);
    radiusInput_->setObjectName(QStringLiteral("cylinderRadiusInput"));
    cylinderHeightInput_->setObjectName(QStringLiteral("cylinderHeightInput"));
    for (int index = 0; index < 3; ++index) {
        translationInputs_[index] = makeLengthInput(cylinderPanel_, true);
        translationInputs_[index]->setObjectName(
            QStringLiteral("cylinderTranslation%1Input").arg(index));
    }
    for (QDoubleSpinBox*& input : rotationInputs_) {
        input = makeAngleInput(cylinderPanel_);
    }
    errorLabel_ = new QLabel(cylinderPanel_);
    errorLabel_->setObjectName(QStringLiteral("cylinderCommandError"));
    errorLabel_->setWordWrap(true);
    errorLabel_->setStyleSheet(
        QStringLiteral("QLabel { color: #ffb4ab; background: #4b2024; border: 1px solid #b94a55; "
                       "border-radius: 3px; padding: 6px; }"));

    cylinderLayout->addRow(tr("Radius"), radiusInput_);
    cylinderLayout->addRow(tr("Height"), cylinderHeightInput_);
    for (int index = 0; index < 3; ++index) {
        cylinderLayout->addRow(tr(moveLabels[index]), translationInputs_[index]);
    }
    for (int index = 0; index < 3; ++index) {
        cylinderLayout->addRow(tr(rotationLabels[index]), rotationInputs_[index]);
    }
    cylinderLayout->addRow(errorLabel_);
}

void FeaturePropertiesPanel::connectEditors() {
    for (QDoubleSpinBox* input :
         {lengthInput_, widthInput_, boxHeightInput_, boxTranslationInputs_[0],
          boxTranslationInputs_[1], boxTranslationInputs_[2], boxRotationInputs_[0],
          boxRotationInputs_[1], boxRotationInputs_[2]}) {
        connect(input, &QDoubleSpinBox::editingFinished, this, [this]() {
            if (boxChangeHandler_) {
                boxChangeHandler_();
            }
        });
    }
    for (QDoubleSpinBox* input :
         {radiusInput_, cylinderHeightInput_, translationInputs_[0], translationInputs_[1],
          translationInputs_[2], rotationInputs_[0], rotationInputs_[1], rotationInputs_[2]}) {
        connect(input, &QDoubleSpinBox::editingFinished, this, [this]() {
            if (cylinderChangeHandler_) {
                cylinderChangeHandler_();
            }
        });
    }
    connect(extrusionDistanceInput_, &QDoubleSpinBox::editingFinished, this, [this]() {
        if (extrusionChangeHandler_) {
            extrusionChangeHandler_();
        }
    });
}

void FeaturePropertiesPanel::setBoxChangeHandler(std::function<void()> handler) {
    boxChangeHandler_ = std::move(handler);
}

void FeaturePropertiesPanel::setCylinderChangeHandler(std::function<void()> handler) {
    cylinderChangeHandler_ = std::move(handler);
}

void FeaturePropertiesPanel::setExtrusionChangeHandler(std::function<void()> handler) {
    extrusionChangeHandler_ = std::move(handler);
}

void FeaturePropertiesPanel::showNone() {
    emptyPanel_->setText(tr("No feature selected"));
    emptyPanel_->setStyleSheet({});
    setCurrentWidget(emptyPanel_);
}

void FeaturePropertiesPanel::showFeatureInfo(const QString& title, const QString& errorText) {
    emptyPanel_->setText(errorText.isEmpty() ? title : title + QStringLiteral("\n\n") + errorText);
    emptyPanel_->setWordWrap(true);
    emptyPanel_->setStyleSheet(errorText.isEmpty()
                                   ? QString{}
                                   : QStringLiteral("QLabel { color: #ffb4ab; background: #4b2024; "
                                                    "border: 1px solid #b94a55; padding: 6px; }"));
    setCurrentWidget(emptyPanel_);
}

void FeaturePropertiesPanel::setErrorCommitHandler(std::function<void()> handler) {
    errorCommitHandler_ = std::move(handler);
    for (auto* button : errorCommitButtons_)
        button->setVisible(errorCommitHandler_ && button->parentWidget() == currentWidget());
}

void FeaturePropertiesPanel::showCommandError(const QString& message) {
    QLabel* label = currentWidget() == boxPanel_         ? boxErrorLabel_
                    : currentWidget() == cylinderPanel_  ? errorLabel_
                    : currentWidget() == extrusionPanel_ ? extrusionErrorLabel_
                                                         : emptyPanel_;
    label->setWordWrap(true);
    label->setText(message);
    label->setStyleSheet(QStringLiteral("QLabel { color: #ffb4ab; background: #4b2024; border: 1px "
                                        "solid #b94a55; padding: 6px; }"));
    label->show();
}
void FeaturePropertiesPanel::showBox(const document::BoxParameters& parameters) {
    setCurrentWidget(boxPanel_);
    boxErrorLabel_->clear();
    boxErrorLabel_->hide();
    const QSignalBlocker blockers[] = {QSignalBlocker(lengthInput_),
                                       QSignalBlocker(widthInput_),
                                       QSignalBlocker(boxHeightInput_),
                                       QSignalBlocker(boxTranslationInputs_[0]),
                                       QSignalBlocker(boxTranslationInputs_[1]),
                                       QSignalBlocker(boxTranslationInputs_[2]),
                                       QSignalBlocker(boxRotationInputs_[0]),
                                       QSignalBlocker(boxRotationInputs_[1]),
                                       QSignalBlocker(boxRotationInputs_[2])};
    lengthInput_->setValue(parameters.dimensions.width);
    widthInput_->setValue(parameters.dimensions.depth);
    boxHeightInput_->setValue(parameters.dimensions.height);
    const double translations[] = {parameters.placement.translationX,
                                   parameters.placement.translationY,
                                   parameters.placement.translationZ};
    const double rotations[] = {kernel::toDegrees(parameters.placement.rotationX).value(),
                                kernel::toDegrees(parameters.placement.rotationY).value(),
                                kernel::toDegrees(parameters.placement.rotationZ).value()};
    for (int index = 0; index < 3; ++index) {
        boxTranslationInputs_[index]->setValue(translations[index]);
        boxRotationInputs_[index]->setValue(rotations[index]);
    }
    static_cast<void>(blockers);
}

void FeaturePropertiesPanel::showCylinder(const document::CylinderParameters& parameters,
                                          const QString& errorText) {
    setCurrentWidget(cylinderPanel_);
    const QSignalBlocker blockers[] = {
        QSignalBlocker(radiusInput_),          QSignalBlocker(cylinderHeightInput_),
        QSignalBlocker(translationInputs_[0]), QSignalBlocker(translationInputs_[1]),
        QSignalBlocker(translationInputs_[2]), QSignalBlocker(rotationInputs_[0]),
        QSignalBlocker(rotationInputs_[1]),    QSignalBlocker(rotationInputs_[2])};
    radiusInput_->setValue(parameters.radius);
    cylinderHeightInput_->setValue(parameters.height);
    const double translations[] = {parameters.placement.translationX,
                                   parameters.placement.translationY,
                                   parameters.placement.translationZ};
    const double rotations[] = {kernel::toDegrees(parameters.placement.rotationX).value(),
                                kernel::toDegrees(parameters.placement.rotationY).value(),
                                kernel::toDegrees(parameters.placement.rotationZ).value()};
    for (int index = 0; index < 3; ++index) {
        translationInputs_[index]->setValue(translations[index]);
        rotationInputs_[index]->setValue(rotations[index]);
    }
    errorLabel_->setText(errorText);
    errorLabel_->setVisible(!errorText.isEmpty());
    static_cast<void>(blockers);
}

void FeaturePropertiesPanel::showExtrusion(const document::ExtrusionParameters& parameters,
                                           const QString& errorText) {
    setCurrentWidget(extrusionPanel_);
    const QSignalBlocker blocker(extrusionDistanceInput_);
    extrusionDistanceInput_->setValue(parameters.distance);
    extrusionProfiles_ = parameters.profiles;
    extrusionErrorLabel_->setText(errorText);
    extrusionErrorLabel_->setVisible(!errorText.isEmpty());
}

document::BoxParameters FeaturePropertiesPanel::boxParameters() const {
    return {
        kernel::BoxDimensions{lengthInput_->value(), widthInput_->value(),
                              boxHeightInput_->value()},
        kernel::RigidTransform{boxTranslationInputs_[0]->value(), boxTranslationInputs_[1]->value(),
                               boxTranslationInputs_[2]->value(),
                               kernel::toRadians(kernel::Degrees(boxRotationInputs_[0]->value())),
                               kernel::toRadians(kernel::Degrees(boxRotationInputs_[1]->value())),
                               kernel::toRadians(kernel::Degrees(boxRotationInputs_[2]->value()))}};
}

document::CylinderParameters
FeaturePropertiesPanel::cylinderParameters(document::CylinderResultMode resultMode) const {
    return {radiusInput_->value(), cylinderHeightInput_->value(),
            kernel::RigidTransform{translationInputs_[0]->value(), translationInputs_[1]->value(),
                                   translationInputs_[2]->value(),
                                   kernel::toRadians(kernel::Degrees(rotationInputs_[0]->value())),
                                   kernel::toRadians(kernel::Degrees(rotationInputs_[1]->value())),
                                   kernel::toRadians(kernel::Degrees(rotationInputs_[2]->value()))},
            resultMode};
}

document::ExtrusionParameters FeaturePropertiesPanel::extrusionParameters() const {
    return {extrusionDistanceInput_->value(), extrusionProfiles_};
}

void FeaturePropertiesPanel::previewMove(const kernel::RigidTransform& base, double deltaX,
                                         double deltaY, double deltaZ) {
    const double values[] = {base.translationX + deltaX, base.translationY + deltaY,
                             base.translationZ + deltaZ};
    QDoubleSpinBox** inputs =
        currentWidget() == boxPanel_ ? boxTranslationInputs_ : translationInputs_;
    for (int index = 0; index < 3; ++index) {
        const QSignalBlocker blocker(inputs[index]);
        inputs[index]->setValue(values[index]);
    }
}

void FeaturePropertiesPanel::previewRotation(const kernel::RigidTransform& base, int axisIndex,
                                             double angleDegrees) {
    if (axisIndex < 0 || axisIndex > 2) {
        return;
    }
    const double values[] = {kernel::toDegrees(base.rotationX).value(),
                             kernel::toDegrees(base.rotationY).value(),
                             kernel::toDegrees(base.rotationZ).value()};
    QDoubleSpinBox** inputs = currentWidget() == boxPanel_ ? boxRotationInputs_ : rotationInputs_;
    const QSignalBlocker blocker(inputs[axisIndex]);
    inputs[axisIndex]->setValue(values[axisIndex] + angleDegrees);
}

} // namespace voelund::desktop
