// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "NativeSolidFeatures.hpp"
#include <TDataStd_Integer.hxx>
#include <TDataStd_RealArray.hxx>
#include <stdexcept>

// M27 (schema 25): shape code on the label (0 sphere, 1 cone, 2 torus, 3 wedge), the four sizes
// and the placement (translation, rotation in radians) as reals (1), the result mode (2: 1 join,
// 2 cut, 3 intersect, 4 separate). The input body, if any, is the feature's child 5.
namespace voelund::document::detail {
namespace {
void require(bool value) {
    if (!value)
        throw std::invalid_argument("Invalid native primitive data");
}
TDF_Label child(const TDF_Label& label, int tag) {
    const auto result = label.FindChild(tag, false);
    require(!result.IsNull());
    return result;
}
int integer(const TDF_Label& label) {
    Handle(TDataStd_Integer) value;
    require(label.FindAttribute(TDataStd_Integer::GetID(), value));
    return value->Get();
}
} // namespace

void writeNativePrimitive(const TDF_Label& label, const PrimitiveParameters& parameters) {
    TDataStd_Integer::Set(label, static_cast<int>(parameters.shape));
    const auto values = TDataStd_RealArray::Set(label.FindChild(1), 1, 10);
    const auto& p = parameters.placement;
    const double all[]{parameters.sizes[0], parameters.sizes[1], parameters.sizes[2],
                       parameters.sizes[3], p.translationX,      p.translationY,
                       p.translationZ,      p.rotationX.value(), p.rotationY.value(),
                       p.rotationZ.value()};
    for (int index = 0; index < 10; ++index)
        values->SetValue(index + 1, all[index]);
    TDataStd_Integer::Set(label.FindChild(2), static_cast<int>(parameters.resultMode) + 1);
}

PrimitiveParameters readNativePrimitive(const TDF_Label& label, int schemaVersion) {
    require(schemaVersion >= 25 && label.NbChildren() == 2);
    const int shape = integer(label);
    require(shape >= 0 && shape <= static_cast<int>(kernel::PrimitiveShape::wedge));
    Handle(TDataStd_RealArray) values;
    require(child(label, 1).FindAttribute(TDataStd_RealArray::GetID(), values) &&
            values->Lower() == 1 && values->Upper() == 10);
    const int mode = integer(child(label, 2));
    require(mode >= 1 && mode <= 4);
    PrimitiveParameters result{
        static_cast<kernel::PrimitiveShape>(shape),
        {values->Value(1), values->Value(2), values->Value(3), values->Value(4)},
        {values->Value(5), values->Value(6), values->Value(7), kernel::Radians{values->Value(8)},
         kernel::Radians{values->Value(9)}, kernel::Radians{values->Value(10)}},
        static_cast<CylinderResultMode>(mode - 1)};
    validatePrimitiveParameters(result);
    return result;
}

} // namespace voelund::document::detail
