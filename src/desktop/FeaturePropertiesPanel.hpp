// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/document/ModelDocument.hpp>
#include <voelund/kernel/ShapeFactory.hpp>

#include <QStackedWidget>

#include <functional>
#include <optional>
#include <string>
#include <vector>

class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

namespace voelund::desktop {

class ParameterSpinBox;

// N01b: "=Name" in the size and position fields of boxes, cylinders and primitives and in an
// extrusion's distance, named as document::featureField names them.
struct PropertyBindingHandlers final {
    std::function<std::optional<QString>(const std::string& field)> boundName;
    // "=Name" (the name) or a number typed into a bound field (nullopt), with the field's value.
    std::function<void(const std::string& field, std::optional<QString> name, double value)>
        requested;
    std::function<QStringList()> lengthNames;
};

class FeaturePropertiesPanel final : public QStackedWidget {
  public:
    explicit FeaturePropertiesPanel(QWidget* parent = nullptr);

    void setBoxChangeHandler(std::function<void()> handler);
    void setCylinderChangeHandler(std::function<void()> handler);
    void setExtrusionChangeHandler(std::function<void()> handler);
    void setBindingHandlers(PropertyBindingHandlers handlers);
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
    [[nodiscard]] QDoubleSpinBox* bindableInput(QWidget* parent, bool allowNegative = false);
    [[nodiscard]] std::string fieldOf(const QDoubleSpinBox* input) const;
    void syncBindings();

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
    QCheckBox* extrusionSeamsInput_{nullptr}; // T05: push/pulls that join
    document::ExtrusionParameters extrusionSelection_{0.0, {}};
    std::function<void()> boxChangeHandler_;
    std::function<void()> cylinderChangeHandler_;
    std::function<void()> extrusionChangeHandler_;
    std::vector<ParameterSpinBox*> bindable_;
    PropertyBindingHandlers bindingHandlers_;
};

} // namespace voelund::desktop
