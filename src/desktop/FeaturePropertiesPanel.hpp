#pragma once

#include <voelund/document/ModelDocument.hpp>
#include <voelund/kernel/ShapeFactory.hpp>

#include <QStackedWidget>

#include <functional>
#include <optional>
#include <vector>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

namespace voelund::desktop {

class FeaturePropertiesPanel final : public QStackedWidget {
  public:
    explicit FeaturePropertiesPanel(QWidget* parent = nullptr);

    void setBoxChangeHandler(std::function<void()> handler);
    void setCylinderChangeHandler(std::function<void()> handler);
    void setExtrusionChangeHandler(std::function<void()> handler);
    void showNone();
    void showCommandError(const QString& message);
    void setErrorCommitHandler(std::function<void()> handler);
    void showFeatureInfo(const QString& title, const QString& errorText);
    void showBox(const document::BoxParameters& parameters);
    void showCylinder(const document::CylinderParameters& parameters, const QString& errorText);
    // M27: a sphere, cone, torus or wedge on the cylinder page, with its own size fields.
    void showPrimitive(const document::PrimitiveParameters& parameters, const QString& errorText);
    [[nodiscard]] bool showsPrimitive() const noexcept { return primitiveShape_.has_value(); }
    [[nodiscard]] document::PrimitiveParameters primitiveParameters() const;
    void showExtrusion(const document::ExtrusionParameters& parameters, const QString& errorText);
    [[nodiscard]] document::BoxParameters boxParameters() const;
    [[nodiscard]] document::CylinderParameters cylinderParameters() const;
    [[nodiscard]] document::ExtrusionParameters extrusionParameters() const;
    void previewMove(const kernel::RigidTransform& base, double deltaX, double deltaY,
                     double deltaZ);
    void previewRotation(const kernel::RigidTransform& base, int axisIndex, double angleDegrees);

  private:
    void createBoxPanel();
    void createCylinderPanel();
    void createExtrusionPanel();
    void connectEditors();
    void showPlacement(const kernel::RigidTransform& placement, const QString& errorText);

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
    QComboBox* cylinderResultModeInput_{nullptr};
    QDoubleSpinBox* extraSizeInputs_[2]{}; // M27: third and fourth size of a primitive
    std::optional<kernel::PrimitiveShape> primitiveShape_;
    QDoubleSpinBox* translationInputs_[3]{};
    QDoubleSpinBox* rotationInputs_[3]{};
    QLabel* errorLabel_{nullptr};
    QLabel* boxErrorLabel_{nullptr};
    QPushButton* errorCommitButtons_[3]{};
    std::function<void()> errorCommitHandler_;
    QDoubleSpinBox* extrusionDistanceInput_{nullptr};
    QLabel* extrusionErrorLabel_{nullptr};
    document::ExtrusionParameters extrusionSelection_{0.0, {}};
    std::function<void()> boxChangeHandler_;
    std::function<void()> cylinderChangeHandler_;
    std::function<void()> extrusionChangeHandler_;
};

} // namespace voelund::desktop
