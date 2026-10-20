#include "FeaturePropertiesPanel.hpp"
#include "MainWindowSupport.hpp"
#include "MeasurementInput.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QWidget>

#include <utility>

namespace voelund::desktop {
namespace {

// Marked for translation with QT_TRANSLATE_NOOP so tr() finds them at run time.
constexpr const char* moveLabels[] = {
    QT_TRANSLATE_NOOP("voelund::desktop::FeaturePropertiesPanel", "Verschieben X"),
    QT_TRANSLATE_NOOP("voelund::desktop::FeaturePropertiesPanel", "Verschieben Y"),
    QT_TRANSLATE_NOOP("voelund::desktop::FeaturePropertiesPanel", "Verschieben Z")};
constexpr const char* rotationLabels[] = {
    QT_TRANSLATE_NOOP("voelund::desktop::FeaturePropertiesPanel", "Drehen X"),
    QT_TRANSLATE_NOOP("voelund::desktop::FeaturePropertiesPanel", "Drehen Y"),
    QT_TRANSLATE_NOOP("voelund::desktop::FeaturePropertiesPanel", "Drehen Z")};

// M27: the size fields of each primitive shape, in the order of PrimitiveParameters::sizes.
QStringList sizeLabels(std::optional<kernel::PrimitiveShape> shape) {
    using kernel::PrimitiveShape;
    using Panel = FeaturePropertiesPanel;
    if (!shape)
        return {Panel::tr("Radius"), Panel::tr("Höhe")};
    switch (*shape) {
    case PrimitiveShape::sphere:
        return {Panel::tr("Radius")};
    case PrimitiveShape::cone:
        return {Panel::tr("Grundradius"), Panel::tr("Deckradius"), Panel::tr("Höhe")};
    case PrimitiveShape::torus:
        return {Panel::tr("Ringradius"), Panel::tr("Rohrradius")};
    case PrimitiveShape::wedge:
        return {Panel::tr("Breite"), Panel::tr("Tiefe"), Panel::tr("Höhe"),
                Panel::tr("Oberbreite")};
    }
    return {};
}

} // namespace

FeaturePropertiesPanel::FeaturePropertiesPanel(QWidget* parent) : QStackedWidget(parent) {
    createBoxPanel();
    createCylinderPanel();
    createExtrusionPanel();
    emptyPanel_ = new QLabel(tr("Kein Feature gewählt"), this);
    addWidget(boxPanel_);
    addWidget(cylinderPanel_);
    addWidget(extrusionPanel_);
    addWidget(emptyPanel_);
    QWidget* pages[] = {boxPanel_, cylinderPanel_, extrusionPanel_};
    for (int index = 0; index < 3; ++index) {
        auto* button = new QPushButton(tr("Mit Folgefehlern übernehmen"), pages[index]);
        button->setObjectName(QStringLiteral("keepEditableErrorsButton"));
        button->setToolTip(tr("Die abgelehnte Änderung als reparierbaren Fehler behalten. "
                              "Rückgängig bleibt möglich."));
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
    // The floating card is as tall as the visible page, not the tallest one (U05).
    connect(this, &QStackedWidget::currentChanged, this, [this](int current) {
        for (int index = 0; index < count(); ++index)
            widget(index)->setSizePolicy(QSizePolicy::Preferred, index == current
                                                                     ? QSizePolicy::Preferred
                                                                     : QSizePolicy::Ignored);
        updateGeometry();
    });
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
    layout->addRow(tr("Distanz"), extrusionDistanceInput_);
    // T05 (user decision 2026-10-17): a pulling push/pull merges its seams; older ones can.
    extrusionSeamsInput_ = new QCheckBox(tr("Naht entfernen"), extrusionPanel_);
    extrusionSeamsInput_->setObjectName(QStringLiteral("extrusionMergeSeamsInput"));
    extrusionSeamsInput_->setToolTip(
        tr("Flächen, die der Zug eben fortsetzt, werden mit dem Körper zu einer Fläche"));
    layout->addRow(extrusionSeamsInput_);
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
    for (int index = 0; index < 3; ++index) {
        boxTranslationInputs_[index] = makeLengthInput(boxPanel_, true);
        boxTranslationInputs_[index]->setObjectName(
            QStringLiteral("boxTranslation%1Input").arg(index));
    }
    for (int index = 0; index < 3; ++index) {
        boxRotationInputs_[index] = makeAngleInput(boxPanel_);
        boxRotationInputs_[index]->setObjectName(QStringLiteral("boxRotation%1Input").arg(index));
    }
    boxLayout->addRow(tr("Länge"), lengthInput_);
    boxLayout->addRow(tr("Breite"), widthInput_);
    boxLayout->addRow(tr("Höhe"), boxHeightInput_);
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
    // The connection belongs to the selected cylinder, not to a global toolbar (U01).
    cylinderResultModeInput_ = new QComboBox(cylinderPanel_);
    cylinderResultModeInput_->setObjectName(QStringLiteral("cylinderResultModeInput"));
    cylinderResultModeInput_->addItems(
        {tr("Vereinigen"), tr("Abziehen"), tr("Schneiden"), tr("Getrennt")});
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
    cylinderLayout->addRow(tr("Höhe"), cylinderHeightInput_);
    for (int index = 0; index < 2; ++index) {
        extraSizeInputs_[index] = makeLengthInput(cylinderPanel_);
        extraSizeInputs_[index]->setObjectName(
            QStringLiteral("primitiveSize%1Input").arg(index + 2));
        cylinderLayout->addRow(QStringLiteral("-"), extraSizeInputs_[index]);
        cylinderLayout->setRowVisible(extraSizeInputs_[index], false);
    }
    cylinderLayout->addRow(tr("Verbindung"), cylinderResultModeInput_);
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
         {radiusInput_, cylinderHeightInput_, extraSizeInputs_[0], extraSizeInputs_[1],
          translationInputs_[0], translationInputs_[1], translationInputs_[2], rotationInputs_[0],
          rotationInputs_[1], rotationInputs_[2]}) {
        connect(input, &QDoubleSpinBox::editingFinished, this, [this]() {
            if (cylinderChangeHandler_) {
                cylinderChangeHandler_();
            }
        });
    }
    connect(cylinderResultModeInput_, &QComboBox::activated, this, [this](int) {
        if (cylinderChangeHandler_) {
            cylinderChangeHandler_();
        }
    });
    connect(extrusionDistanceInput_, &QDoubleSpinBox::editingFinished, this, [this]() {
        if (extrusionChangeHandler_) {
            extrusionChangeHandler_();
        }
    });
    connect(extrusionSeamsInput_, &QCheckBox::toggled, this, [this](bool) {
        if (extrusionChangeHandler_)
            extrusionChangeHandler_();
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
    emptyPanel_->setText(tr("Kein Feature gewählt"));
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
    primitiveShape_.reset();
    const QSignalBlocker blockers[] = {QSignalBlocker(radiusInput_),
                                       QSignalBlocker(cylinderHeightInput_),
                                       QSignalBlocker(cylinderResultModeInput_)};
    cylinderResultModeInput_->setCurrentIndex(resultModeIndex(parameters.resultMode));
    showPlacement(parameters.placement, errorText); // labels and fields of a cylinder
    radiusInput_->setValue(parameters.radius);
    cylinderHeightInput_->setValue(parameters.height);
    static_cast<void>(blockers);
}

void FeaturePropertiesPanel::showPrimitive(const document::PrimitiveParameters& parameters,
                                           const QString& errorText) {
    primitiveShape_ = parameters.shape;
    QDoubleSpinBox* sizes[] = {radiusInput_, cylinderHeightInput_, extraSizeInputs_[0],
                               extraSizeInputs_[1]};
    const QSignalBlocker blockers[] = {QSignalBlocker(sizes[0]), QSignalBlocker(sizes[1]),
                                       QSignalBlocker(sizes[2]), QSignalBlocker(sizes[3]),
                                       QSignalBlocker(cylinderResultModeInput_)};
    cylinderResultModeInput_->setCurrentIndex(resultModeIndex(parameters.resultMode));
    showPlacement(parameters.placement, errorText);
    for (int index = 0; index < 4; ++index)
        sizes[index]->setValue(parameters.sizes[static_cast<std::size_t>(index)]);
    static_cast<void>(blockers);
}

void FeaturePropertiesPanel::showPlacement(const kernel::RigidTransform& placement,
                                           const QString& errorText) {
    setCurrentWidget(cylinderPanel_);
    auto* layout = static_cast<QFormLayout*>(cylinderPanel_->layout());
    QDoubleSpinBox* sizes[] = {radiusInput_, cylinderHeightInput_, extraSizeInputs_[0],
                               extraSizeInputs_[1]};
    const auto labels = sizeLabels(primitiveShape_);
    for (int index = 0; index < 4; ++index) {
        const bool shown = index < labels.size();
        layout->setRowVisible(sizes[index], shown);
        if (shown)
            static_cast<QLabel*>(layout->labelForField(sizes[index]))->setText(labels[index]);
        // A cone may end in an apex (radius 0); every other size is a positive length.
        sizes[index]->setMinimum(
            primitiveShape_ == kernel::PrimitiveShape::cone && index < 2 ? 0.0 : 0.01);
    }
    const double translations[] = {placement.translationX, placement.translationY,
                                   placement.translationZ};
    const double rotations[] = {kernel::toDegrees(placement.rotationX).value(),
                                kernel::toDegrees(placement.rotationY).value(),
                                kernel::toDegrees(placement.rotationZ).value()};
    for (int index = 0; index < 3; ++index) {
        const QSignalBlocker translation(translationInputs_[index]);
        const QSignalBlocker rotation(rotationInputs_[index]);
        translationInputs_[index]->setValue(translations[index]);
        rotationInputs_[index]->setValue(rotations[index]);
    }
    errorLabel_->setText(errorText);
    errorLabel_->setVisible(!errorText.isEmpty());
}

document::PrimitiveParameters FeaturePropertiesPanel::primitiveParameters() const {
    const auto cylinder = cylinderParameters();
    return {primitiveShape_.value_or(kernel::PrimitiveShape::sphere),
            {radiusInput_->value(), cylinderHeightInput_->value(), extraSizeInputs_[0]->value(),
             extraSizeInputs_[1]->value()},
            cylinder.placement,
            cylinder.resultMode};
}

void FeaturePropertiesPanel::showExtrusion(const document::ExtrusionParameters& parameters,
                                           const QString& errorText) {
    setCurrentWidget(extrusionPanel_);
    const QSignalBlocker blocker(extrusionDistanceInput_);
    extrusionDistanceInput_->setValue(parameters.distance);
    extrusionSelection_ = parameters;
    {
        const QSignalBlocker seams(extrusionSeamsInput_);
        extrusionSeamsInput_->setChecked(parameters.mergeSeams);
    }
    extrusionSeamsInput_->setVisible(parameters.faceProfile.has_value() &&
                                     parameters.operation == document::ExtrusionOperation::join);
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

document::CylinderParameters FeaturePropertiesPanel::cylinderParameters() const {
    return {radiusInput_->value(), cylinderHeightInput_->value(),
            kernel::RigidTransform{translationInputs_[0]->value(), translationInputs_[1]->value(),
                                   translationInputs_[2]->value(),
                                   kernel::toRadians(kernel::Degrees(rotationInputs_[0]->value())),
                                   kernel::toRadians(kernel::Degrees(rotationInputs_[1]->value())),
                                   kernel::toRadians(kernel::Degrees(rotationInputs_[2]->value()))},
            resultModeFromIndex(cylinderResultModeInput_->currentIndex())};
}

document::ExtrusionParameters FeaturePropertiesPanel::extrusionParameters() const {
    // Only the distance is edited here; the legacy or region selection passes through.
    auto parameters = extrusionSelection_;
    parameters.distance = extrusionDistanceInput_->value();
    if (parameters.faceProfile && parameters.operation == document::ExtrusionOperation::join)
        parameters.mergeSeams = extrusionSeamsInput_->isChecked();
    return parameters;
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
