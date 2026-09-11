#pragma once

#include <voelund/document/ModelDocument.hpp>
#include <voelund/kernel/ShapeFactory.hpp>

#include <QStackedWidget>

#include <functional>

class QDoubleSpinBox;
class QLabel;

namespace voelund::desktop {

class FeaturePropertiesPanel final : public QStackedWidget {
public:
    explicit FeaturePropertiesPanel(QWidget* parent = nullptr);

    void setBoxChangeHandler(std::function<void()> handler);
    void setCylinderChangeHandler(std::function<void()> handler);
    void setExtrusionChangeHandler(std::function<void()> handler);
    void showNone();
    void showFeatureInfo(const QString& title, const QString& errorText);
    void showBox(const document::BoxParameters& parameters);
    void showCylinder(
        const document::CylinderParameters& parameters,
        const QString& errorText);
    void showExtrusion(
        const document::ExtrusionParameters& parameters,
        const QString& errorText);
    [[nodiscard]] document::BoxParameters boxParameters() const;
    [[nodiscard]] document::CylinderParameters cylinderParameters(
        document::CylinderResultMode resultMode) const;
    [[nodiscard]] document::ExtrusionParameters extrusionParameters() const;
    void previewMove(
        const kernel::RigidTransform& base,
        double deltaX,
        double deltaY,
        double deltaZ);
    void previewRotation(
        const kernel::RigidTransform& base,
        int axisIndex,
        double angleDegrees);

private:
    void createBoxPanel();
    void createCylinderPanel();
    void createExtrusionPanel();
    void connectEditors();

    QWidget* boxPanel_{nullptr};
    QWidget* cylinderPanel_{nullptr};
    QWidget* extrusionPanel_{nullptr};
    QLabel* emptyPanel_{nullptr};
    QDoubleSpinBox* lengthInput_{nullptr};
    QDoubleSpinBox* widthInput_{nullptr};
    QDoubleSpinBox* boxHeightInput_{nullptr};
    QDoubleSpinBox* boxTranslationInputs_[3]{};
    QDoubleSpinBox* boxRotationInputs_[3]{};
    QDoubleSpinBox* radiusInput_{nullptr};
    QDoubleSpinBox* cylinderHeightInput_{nullptr};
    QDoubleSpinBox* translationInputs_[3]{};
    QDoubleSpinBox* rotationInputs_[3]{};
    QLabel* errorLabel_{nullptr};
    QDoubleSpinBox* extrusionDistanceInput_{nullptr};
    QLabel* extrusionErrorLabel_{nullptr};
    std::function<void()> boxChangeHandler_;
    std::function<void()> cylinderChangeHandler_;
    std::function<void()> extrusionChangeHandler_;
};

} // namespace voelund::desktop
