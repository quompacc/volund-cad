#pragma once

class QDoubleSpinBox;
class QWidget;

namespace voelund::desktop {

[[nodiscard]] QDoubleSpinBox* makeLengthInput(QWidget* parent, bool allowNegative = false);
[[nodiscard]] QDoubleSpinBox* makeAngleInput(QWidget* parent);
// U11: a whole number without a unit (counts, sides).
[[nodiscard]] QDoubleSpinBox* makeCountInput(QWidget* parent);
// S20: a plain factor with decimals, such as a scale.
[[nodiscard]] QDoubleSpinBox* makeFactorInput(QWidget* parent);

} // namespace voelund::desktop
