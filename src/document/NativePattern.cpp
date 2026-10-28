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
// N11b (schema 48): the repeated steps (11), one ID per child 1..n. N11c (schema 49): a pattern
// along a curve has its path (12) instead of an axis: following on its label, the sketch (1)
// and its elements (children of 2).
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

// N11b: repeated steps, one ID per child 1..n.
void writeNativeSteps(const TDF_Label& label, const std::vector<FeatureId>& steps) {
    for (std::size_t index = 0; index < steps.size(); ++index)
        writeId(label.FindChild(static_cast<int>(index) + 1), steps[index].value());
}

std::vector<FeatureId> readNativeSteps(const TDF_Label& label) {
    const int count = label.NbChildren();
    require(count >= 1 && static_cast<std::size_t>(count) <= maximumPatternFeatures);
    std::vector<FeatureId> result;
    for (int index = 1; index <= count; ++index)
        result.push_back(FeatureId{readId(child(label, index))});
    return result;
}

std::size_t nativePatternRecords(const PatternParameters& parameters) {
    return nativeDatumAxisRecords(parameters.axis) +
           (parameters.second ? nativeDatumAxisRecords(parameters.second->axis) : 0) +
           (parameters.path ? parameters.path->elements.size() : 0);
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

// Children 1 to 6 and the axis (7 or 8, or the path 12 since schema 49); since schema 47 a
// grid (9) and skipped copies (10), since 48 the repeated steps (11).
void requireLayout(const TDF_Label& label, int schemaVersion) {
    const auto has = [&](int tag) { return label.FindChild(tag, false).IsNull() ? 0 : 1; };
    const int placements = has(7) + has(8) + has(12);
    const int extras = has(9) + has(10) + has(11);
    const int since = has(12) != 0 ? 49 : has(11) != 0 ? 48 : extras != 0 ? 47 : 32;
    require(schemaVersion >= since && placements == 1 &&
            label.NbChildren() == 6 + placements + extras);
}

void writePath(const TDF_Label& label, const SweepPath& path, bool follow) {
    TDataStd_Integer::Set(label, follow ? 1 : 0);
    writeId(label.FindChild(1), path.sketch.value());
    const auto elements = label.FindChild(2);
    for (std::size_t index = 0; index < path.elements.size(); ++index)
        writeId(elements.FindChild(static_cast<int>(index) + 1), path.elements[index].value());
}

void readPath(const TDF_Label& label, PatternParameters& result) {
    const auto elements = child(label, 2);
    const int follow = integer(label);
    require(label.NbChildren() == 2 && (follow == 0 || follow == 1) && elements.NbChildren() >= 1 &&
            static_cast<std::size_t>(elements.NbChildren()) <= maximumSweepPathElements);
    SweepPath path{FeatureId{readId(child(label, 1))}, {}};
    for (int index = 1; index <= elements.NbChildren(); ++index)
        path.elements.push_back(SketchElementId{readId(child(elements, index))});
    result.path = std::move(path);
    result.followPath = follow == 1;
}

// The axis: a construction axis (8), a path (12) or a base axis or edge (7).
void readPlacement(const TDF_Label& label, PatternParameters& result, std::size_t remainingRecords,
                   int schemaVersion) {
    if (const auto axis = label.FindChild(8, false); !axis.IsNull())
        result.datumAxis = FeatureId{readId(axis)};
    else if (const auto path = label.FindChild(12, false); !path.IsNull())
        readPath(path, result);
    else
        result.axis = readNativeDatumAxis(child(label, 7), remainingRecords, schemaVersion);
}

void readGrid(const TDF_Label& label, PatternParameters& result, std::size_t remainingRecords,
              int schemaVersion) {
    if (const auto second = label.FindChild(9, false); !second.IsNull())
        result.second = readSecond(second, remainingRecords, schemaVersion);
    if (const auto skipped = label.FindChild(10, false); !skipped.IsNull())
        result.skipped = readSkipped(skipped);
    if (const auto sources = label.FindChild(11, false); !sources.IsNull())
        result.features = readNativeSteps(sources);
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
    else if (parameters.path)
        writePath(label.FindChild(12), *parameters.path, parameters.followPath);
    else
        writeNativeDatumAxis(label.FindChild(7), parameters.axis);
    if (parameters.second)
        writeSecond(label.FindChild(9), *parameters.second);
    if (!parameters.skipped.empty())
        writeSkipped(label.FindChild(10), parameters.skipped);
    if (!parameters.features.empty())
        writeNativeSteps(label.FindChild(11), parameters.features);
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
    require(kind >= 1 && kind <= 3 && (mode == 1 || mode == 2) &&
            (reversed == 0 || reversed == 1) && count >= 2 &&
            static_cast<std::uint32_t>(count) <= maximumPatternCount);
    result.kind = static_cast<PatternKind>(kind);
    result.count = static_cast<std::uint32_t>(count);
    result.mode = static_cast<PatternMode>(mode);
    result.reversed = reversed == 1;
    result.spacing = real(label);
    result.angle = kernel::Radians{real(child(label, 6))};
    readPlacement(label, result, remainingRecords, schemaVersion);
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
