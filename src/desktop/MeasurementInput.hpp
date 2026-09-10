#pragma once

class QDoubleSpinBox;
class QWidget;

namespace voelund::desktop {

[[nodiscard]] QDoubleSpinBox* makeLengthInput(QWidget* parent, bool allowNegative = false);
[[nodiscard]] QDoubleSpinBox* makeAngleInput(QWidget* parent);

} // namespace voelund::desktop
