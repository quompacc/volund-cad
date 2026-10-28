// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "NativeSolidFeatures.hpp"
#include <TDataStd_AsciiString.hxx>
#include <TDataStd_Integer.hxx>
#include <TDataStd_Real.hxx>
#include <TDataStd_RealArray.hxx>
#include <charconv>
#include <cmath>
#include <stdexcept>
#include <string>

// M10 (schema 32): a pattern keeps its spacing on the label, body (1), kind (2), count (3),
// result (4), reversed (5), angle (6), and its axis: a base axis or edge as a construction axis
// stores it (7) or a construction axis (8). A copy keeps its index on the label and its
// pattern (1). N11a (schema 47): a grid's second direction (9) keeps its spacing on its label,
// count (1), reversed (2) and its axis (3, or 4 for a construction axis); skipped copies (10).
namespace voelund::document::detail {
namespace {
void require(bool value) {
    if (!value)
        throw std::invalid_argument("Invalid native pattern data");
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
double real(const TDF_Label& label) {
    Handle(TDataStd_Real) value;
    require(label.FindAttribute(TDataStd_Real::GetID(), value));
    return value->Get();
}
void writeId(const TDF_Label& label, std::uint64_t id) {
    TDataStd_AsciiString::Set(label, std::to_string(id).c_str());
}
std::uint64_t readId(const TDF_Label& label) {
    Handle(TDataStd_AsciiString) value;
    require(label.NbChildren() == 0 && label.FindAttribute(TDataStd_AsciiString::GetID(), value) &&
            value->Get().Length() <= 20);
    const std::string text{value->Get().ToCString(),
                           static_cast<std::size_t>(value->Get().Length())};
    std::uint64_t id = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), id);
    require(parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() && id != 0 &&
            text == std::to_string(id));
    return id;
}
} // namespace

std::size_t nativePatternRecords(const PatternParameters& parameters) {
    return nativeDatumAxisRecords(parameters.axis) +
           (parameters.second ? nativeDatumAxisRecords(parameters.second->axis) : 0);
}

namespace {
void writeSecond(const TDF_Label& label, const PatternDirection& second) {
    TDataStd_Real::Set(label, second.spacing);
    TDataStd_Integer::Set(label.FindChild(1), static_cast<int>(second.count));
    TDataStd_Integer::Set(label.FindChild(2), second.reversed ? 1 : 0);
    if (second.datumAxis)
        writeId(label.FindChild(4), second.datumAxis->value());
    else
        writeNativeDatumAxis(label.FindChild(3), second.axis);
}

PatternDirection readSecond(const TDF_Label& label, std::size_t remainingRecords,
                            int schemaVersion) {
    require(label.NbChildren() == 3);
    PatternDirection result;
    const int count = integer(child(label, 1));
    const int reversed = integer(child(label, 2));
    require(count >= 2 && static_cast<std::uint32_t>(count) <= maximumPatternCount &&
            (reversed == 0 || reversed == 1));
    result.count = static_cast<std::uint32_t>(count);
    result.reversed = reversed == 1;
    result.spacing = real(label);
    if (const auto axis = label.FindChild(4, false); !axis.IsNull())
        result.datumAxis = FeatureId{readId(axis)};
    else
        result.axis = readNativeDatumAxis(child(label, 3), remainingRecords, schemaVersion);
    return result;
}

// The skipped copies' indices, as whole numbers in a real array.
void writeSkipped(const TDF_Label& label, const std::vector<std::uint32_t>& skipped) {
    const auto array = TDataStd_RealArray::Set(label, 1, static_cast<int>(skipped.size()));
    for (std::size_t index = 0; index < skipped.size(); ++index)
        array->SetValue(static_cast<int>(index) + 1, static_cast<double>(skipped[index]));
}

std::vector<std::uint32_t> readSkipped(const TDF_Label& label) {
    Handle(TDataStd_RealArray) array;
    require(label.NbChildren() == 0 && label.FindAttribute(TDataStd_RealArray::GetID(), array) &&
            array->Lower() == 1 && array->Length() <= static_cast<int>(maximumPatternCount));
    std::vector<std::uint32_t> result;
    for (int index = array->Lower(); index <= array->Upper(); ++index) {
        const double value = array->Value(index);
        require(value >= 1.0 && value < maximumPatternCount && value == std::floor(value));
        result.push_back(static_cast<std::uint32_t>(value));
    }
    return result;
}

// Children 1 to 6 and the axis (7 or 8); since schema 47 a grid (9) and skipped copies (10).
void requireLayout(const TDF_Label& label, int schemaVersion) {
    const int grid = label.FindChild(9, false).IsNull() ? 0 : 1;
    const int skipped = label.FindChild(10, false).IsNull() ? 0 : 1;
    require(schemaVersion >= 32 && label.NbChildren() == 7 + grid + skipped &&
            (schemaVersion >= 47 || grid + skipped == 0));
}

void readGrid(const TDF_Label& label, PatternParameters& result, std::size_t remainingRecords,
              int schemaVersion) {
    if (const auto second = label.FindChild(9, false); !second.IsNull())
        result.second = readSecond(second, remainingRecords, schemaVersion);
    if (const auto skipped = label.FindChild(10, false); !skipped.IsNull())
        result.skipped = readSkipped(skipped);
}
} // namespace

void writeNativePattern(const TDF_Label& label, const PatternParameters& parameters) {
    validatePatternParameters(parameters);
    TDataStd_Real::Set(label, parameters.spacing);
    writeId(label.FindChild(1), parameters.body.value());
    TDataStd_Integer::Set(label.FindChild(2), static_cast<int>(parameters.kind));
    TDataStd_Integer::Set(label.FindChild(3), static_cast<int>(parameters.count));
    TDataStd_Integer::Set(label.FindChild(4), static_cast<int>(parameters.mode));
    TDataStd_Integer::Set(label.FindChild(5), parameters.reversed ? 1 : 0);
    TDataStd_Real::Set(label.FindChild(6), parameters.angle.value());
    if (parameters.datumAxis)
        writeId(label.FindChild(8), parameters.datumAxis->value());
    else
        writeNativeDatumAxis(label.FindChild(7), parameters.axis);
    if (parameters.second)
        writeSecond(label.FindChild(9), *parameters.second);
    if (!parameters.skipped.empty())
        writeSkipped(label.FindChild(10), parameters.skipped);
}

PatternParameters readNativePattern(const TDF_Label& label, std::size_t remainingRecords,
                                    int schemaVersion) {
    requireLayout(label, schemaVersion);
    PatternParameters result{BodyId{readId(child(label, 1))},
                             PatternKind::linear,
                             {},
                             std::nullopt,
                             false,
                             2,
                             10.0,
                             kernel::Radians{0.0},
                             PatternMode::newBodies};
    const int kind = integer(child(label, 2));
    const int count = integer(child(label, 3));
    const int mode = integer(child(label, 4));
    const int reversed = integer(child(label, 5));
    require((kind == 1 || kind == 2) && (mode == 1 || mode == 2) &&
            (reversed == 0 || reversed == 1) && count >= 2 &&
            static_cast<std::uint32_t>(count) <= maximumPatternCount);
    result.kind = static_cast<PatternKind>(kind);
    result.count = static_cast<std::uint32_t>(count);
    result.mode = static_cast<PatternMode>(mode);
    result.reversed = reversed == 1;
    result.spacing = real(label);
    result.angle = kernel::Radians{real(child(label, 6))};
    if (const auto axis = label.FindChild(8, false); !axis.IsNull())
        result.datumAxis = FeatureId{readId(axis)};
    else
        result.axis = readNativeDatumAxis(child(label, 7), remainingRecords, schemaVersion);
    readGrid(label, result, remainingRecords, schemaVersion);
    validatePatternParameters(result);
    return result;
}

void writeNativePatternCopy(const TDF_Label& label, const PatternCopyParameters& parameters) {
    TDataStd_Integer::Set(label, static_cast<int>(parameters.index));
    writeId(label.FindChild(1), parameters.pattern.value());
}

PatternCopyParameters readNativePatternCopy(const TDF_Label& label, int schemaVersion) {
    require(schemaVersion >= 32 && label.NbChildren() == 1);
    const int index = integer(label);
    require(index >= 1 && static_cast<std::uint32_t>(index) < maximumPatternCount);
    return {FeatureId{readId(child(label, 1))}, static_cast<std::uint32_t>(index)};
}

} // namespace voelund::document::detail
