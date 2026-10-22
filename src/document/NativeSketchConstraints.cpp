// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "NativeSketchConstraints.hpp"
#include "DocumentSerializationDetail.hpp"
#include "NativeParameters.hpp"
#include "NativeProjectedEdge.hpp"
#include <TDF_Reference.hxx>
#include <TDF_Tool.hxx>
#include <TDataStd_AsciiString.hxx>
#include <TDataStd_Integer.hxx>
#include <TDataStd_RealArray.hxx>
#include <array>
#include <charconv>
#include <map>
#include <string_view>
#include <voelund/document/IdCapacity.hpp>

namespace voelund::document::detail {
namespace {
void require(bool condition) {
    if (!condition)
        throw std::invalid_argument("Invalid native sketch constraint");
}
// Stable names, independent of C++ enum ordinals and the legacy text reader.
constexpr std::array kinds{
    std::pair{SketchConstraintKind::horizontal, std::string_view{"horizontal"}},
    std::pair{SketchConstraintKind::vertical, std::string_view{"vertical"}},
    std::pair{SketchConstraintKind::distance, std::string_view{"distance"}},
    std::pair{SketchConstraintKind::radius, std::string_view{"radius"}},
    std::pair{SketchConstraintKind::coincident, std::string_view{"coincident"}},
    std::pair{SketchConstraintKind::parallel, std::string_view{"parallel"}},
    std::pair{SketchConstraintKind::perpendicular, std::string_view{"perpendicular"}},
    std::pair{SketchConstraintKind::coordinateX, std::string_view{"coordinateX"}},
    std::pair{SketchConstraintKind::coordinateY, std::string_view{"coordinateY"}},
    std::pair{SketchConstraintKind::pointOnObject, std::string_view{"pointOnObject"}},
    std::pair{SketchConstraintKind::equal, std::string_view{"equal"}},
    std::pair{SketchConstraintKind::tangent, std::string_view{"tangent"}},
    std::pair{SketchConstraintKind::symmetric, std::string_view{"symmetric"}},
    std::pair{SketchConstraintKind::pointDistance, std::string_view{"pointDistance"}},
    std::pair{SketchConstraintKind::angle, std::string_view{"angle"}},
    std::pair{SketchConstraintKind::pointDistanceX, std::string_view{"pointDistanceX"}},
    std::pair{SketchConstraintKind::pointDistanceY, std::string_view{"pointDistanceY"}},
    std::pair{SketchConstraintKind::midpoint, std::string_view{"midpoint"}},
    std::pair{SketchConstraintKind::perpendicularBisector,
              std::string_view{"perpendicularBisector"}},
    std::pair{SketchConstraintKind::concentric, std::string_view{"concentric"}},
    std::pair{SketchConstraintKind::diameter, std::string_view{"diameter"}},
    std::pair{SketchConstraintKind::collinear, std::string_view{"collinear"}},
    std::pair{SketchConstraintKind::horizontalPoints, std::string_view{"horizontalPoints"}},
    std::pair{SketchConstraintKind::verticalPoints, std::string_view{"verticalPoints"}},
    std::pair{SketchConstraintKind::fixed, std::string_view{"fixed"}},
    std::pair{SketchConstraintKind::pointLineDistance, std::string_view{"pointLineDistance"}},
};
std::string_view name(SketchConstraintKind kind) {
    for (const auto& [key, value] : kinds)
        if (key == kind)
            return value;
    throw std::invalid_argument("Unknown native constraint kind");
}
SketchConstraintKind kind(std::string_view encoded) {
    for (const auto& [key, value] : kinds)
        if (value == encoded)
            return key;
    throw std::invalid_argument("Unknown native constraint kind");
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
std::string text(const TDF_Label& label) {
    Handle(TDataStd_AsciiString) value;
    require(label.FindAttribute(TDataStd_AsciiString::GetID(), value) &&
            value->Get().Length() <= 32);
    return {value->Get().ToCString(), static_cast<std::size_t>(value->Get().Length())};
}
std::uint64_t id(const TDF_Label& label) {
    const auto encoded = text(label);
    std::uint64_t result = 0;
    const auto parsed = std::from_chars(encoded.data(), encoded.data() + encoded.size(), result);
    require(parsed.ec == std::errc{} && parsed.ptr == encoded.data() + encoded.size() &&
            encoded == std::to_string(result) && hasIdCapacity(result, 1));
    return result;
}
std::string entry(const TDF_Label& label) {
    TCollection_AsciiString result;
    TDF_Tool::Entry(label, result);
    return result.ToCString();
}
int pointCode(SketchPointPosition point) {
    switch (point) {
    case SketchPointPosition::start:
        return 1;
    case SketchPointPosition::end:
        return 2;
    case SketchPointPosition::center:
        return 3;
    }
    throw std::invalid_argument("Unknown native point position");
}
SketchPointPosition pointValue(int code) {
    switch (code) {
    case 1:
        return SketchPointPosition::start;
    case 2:
        return SketchPointPosition::end;
    case 3:
        return SketchPointPosition::center;
    default:
        throw std::invalid_argument("Unknown native point position");
    }
}
void writeReference(const TDF_Label& label, const std::optional<SketchGeometryReference>& ref,
                    const std::map<std::uint64_t, TDF_Label>& elements) {
    TDataStd_Integer::Set(label, ref ? pointCode(ref->point) : 0);
    if (ref)
        TDF_Reference::Set(label, elements.at(ref->element.value()));
}
std::optional<SketchGeometryReference>
readReference(const TDF_Label& label, const TDF_Label& root,
              const std::map<std::string, SketchElementId>& elements) {
    require(label.NbChildren() == 0);
    const int code = integer(label);
    Handle(TDF_Reference) reference;
    const bool present = label.FindAttribute(TDF_Reference::GetID(), reference);
    if (code == 0) {
        require(!present);
        return std::nullopt;
    }
    const auto point = pointValue(code);
    require(present);
    const auto target = reference->Get();
    require(!target.IsNull() && target.Data() == root.Data());
    const auto found = elements.find(entry(target));
    require(found != elements.end());
    return SketchGeometryReference{found->second, point};
}
Handle(TDataStd_RealArray) reals(const TDF_Label& label, int size) {
    Handle(TDataStd_RealArray) result;
    require(label.FindAttribute(TDataStd_RealArray::GetID(), result));
    require(result->Lower() == 1 && result->Upper() == size);
    return result;
}
} // namespace
void validateNativeSketchConstraints(const SketchParameters& sketch) {
    require(sketch.constraints.values.size() <= maximumSketchConstraintCount);
    validateSketchConstraints(sketch.elements, sketch.constraints);
    for (const auto& constraint : sketch.constraints.values) {
        (void)name(constraint.kind);
        (void)pointCode(constraint.first.point);
        if (constraint.second)
            (void)pointCode(constraint.second->point);
        if (constraint.third)
            (void)pointCode(constraint.third->point);
    }
}
void writeNativeSketchConstraints(const TDF_Label& label, const TDF_Label& elements,
                                  const SketchParameters& sketch) {
    std::map<std::uint64_t, TDF_Label> labels;
    for (std::size_t index = 0; index < sketch.elements.values.size(); ++index)
        labels.emplace(sketch.elements.values[index].id.value(),
                       elements.FindChild(static_cast<int>(index + 1)));
    TDataStd_Integer::Set(label, static_cast<int>(sketch.constraints.values.size()));
    for (std::size_t index = 0; index < sketch.constraints.values.size(); ++index) {
        const auto& constraint = sketch.constraints.values[index];
        const auto native = label.FindChild(static_cast<int>(index + 1));
        if (constraint.projectedEdge)
            writeNativeProjectedEdge(native.FindChild(8), *constraint.projectedEdge);
        if (constraint.parameter)
            writeNativeParameterBinding(native.FindChild(9), *constraint.parameter);
        if (constraint.reference) // S18
            TDataStd_Integer::Set(native.FindChild(10), 1);
        TDataStd_AsciiString::Set(native.FindChild(1),
                                  std::to_string(constraint.id.value()).c_str());
        TDataStd_AsciiString::Set(native.FindChild(2), name(constraint.kind).data());
        writeReference(native.FindChild(3), constraint.first, labels);
        writeReference(native.FindChild(4), constraint.second, labels);
        writeReference(native.FindChild(5), constraint.third, labels);
        const auto values = TDataStd_RealArray::Set(native.FindChild(6), 1, 2);
        values->SetValue(1, constraint.value);
        values->SetValue(2, constraint.angleValue.value());
        const int count = static_cast<int>(constraint.auxiliaryValues.size());
        const auto auxiliary = native.FindChild(7);
        TDataStd_Integer::Set(auxiliary, count);
        if (count > 0) {
            const auto data = TDataStd_RealArray::Set(auxiliary, 1, count);
            for (int item = 1; item <= count; ++item)
                data->SetValue(item,
                               constraint.auxiliaryValues[static_cast<std::size_t>(item - 1)]);
        }
    }
}
SketchConstraints readNativeSketchConstraints(const TDF_Label& label, const TDF_Label& elements,
                                              const SketchElements& geometry, std::uint64_t nextId,
                                              std::size_t remainingRecords, int schemaVersion) {
    const int count = integer(label);
    require(count >= 0 && count <= static_cast<int>(maximumSketchConstraintCount) &&
            static_cast<std::size_t>(count) <= remainingRecords && label.NbChildren() == count);
    std::map<std::string, SketchElementId> labels;
    for (std::size_t index = 0; index < geometry.values.size(); ++index)
        labels.emplace(entry(child(elements, static_cast<int>(index + 1))),
                       geometry.values[index].id);
    SketchConstraints result{{}, nextId};
    for (int index = 1; index <= count; ++index) {
        const auto native = child(label, index);
        // Optional children: 8 projected edge (schema 3), 9 parameter binding (schema 7),
        // 10 reference dimension (schema 24).
        const bool projected = !native.FindChild(8, false).IsNull();
        const bool bound = !native.FindChild(9, false).IsNull();
        const bool reference = !native.FindChild(10, false).IsNull();
        require(native.NbChildren() ==
                    7 + (projected ? 1 : 0) + (bound ? 1 : 0) + (reference ? 1 : 0) &&
                (!projected || schemaVersion >= 3) && (!bound || schemaVersion >= 7) &&
                (!reference || (schemaVersion >= 24 && integer(child(native, 10)) == 1)));
        const auto first = readReference(child(native, 3), elements, labels);
        require(first.has_value());
        const auto second = readReference(child(native, 4), elements, labels);
        const auto third = readReference(child(native, 5), elements, labels);
        const auto values = reals(child(native, 6), 2);
        const auto auxiliary = child(native, 7);
        const int size = integer(auxiliary);
        require(size >= 0 && size <= 3 && auxiliary.NbChildren() == 0);
        std::vector<double> extra;
        if (size > 0) {
            const auto data = reals(auxiliary, size);
            for (int item = 1; item <= size; ++item)
                extra.push_back(data->Value(item));
        } else {
            Handle(TDataStd_RealArray) unexpected;
            require(!auxiliary.FindAttribute(TDataStd_RealArray::GetID(), unexpected));
        }
        SketchConstraint constraint{SketchConstraintId{id(child(native, 1))},
                                    kind(text(child(native, 2))),
                                    *first,
                                    second,
                                    values->Value(1),
                                    third,
                                    std::move(extra)};
        constraint.angleValue = kernel::Radians{values->Value(2)};
        if (projected)
            constraint.projectedEdge = readNativeProjectedEdge(child(native, 8));
        if (bound)
            constraint.parameter = readNativeParameterBinding(child(native, 9));
        constraint.reference = reference;
        result.values.push_back(std::move(constraint));
    }
    return result;
}
} // namespace voelund::document::detail
