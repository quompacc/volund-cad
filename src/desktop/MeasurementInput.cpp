#include "MeasurementInput.hpp"

#include <QDoubleSpinBox>

namespace voelund::desktop {

QDoubleSpinBox* makeLengthInput(QWidget* parent, bool allowNegative)
{
    auto* input = new QDoubleSpinBox(parent);
    input->setRange(allowNegative ? -1'000'000.0 : 0.01, 1'000'000.0);
    input->setDecimals(3);
    input->setSingleStep(1.0);
    input->setSuffix(QStringLiteral(" mm"));
    input->setKeyboardTracking(false);
    return input;
}

QDoubleSpinBox* makeAngleInput(QWidget* parent)
{
    auto* input = new QDoubleSpinBox(parent);
    input->setRange(-360'000.0, 360'000.0);
    input->setDecimals(3);
    input->setSingleStep(5.0);
    input->setSuffix(QStringLiteral("°"));
    input->setKeyboardTracking(false);
    return input;
}

} // namespace voelund::desktop
