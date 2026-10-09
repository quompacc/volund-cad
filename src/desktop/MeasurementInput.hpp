#pragma once

class QDoubleSpinBox;
class QWidget;

namespace voelund::desktop {

[[nodiscard]] QDoubleSpinBox* makeLengthInput(QWidget* parent, bool allowNegative = false);
[[nodiscard]] QDoubleSpinBox* makeAngleInput(QWidget* parent);
// U11: a whole number without a unit (counts, sides).
[[nodiscard]] QDoubleSpinBox* makeCountInput(QWidget* parent);

} // namespace voelund::desktop
