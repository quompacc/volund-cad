#include "NativeSolidFeatures.hpp"
#include <TDataStd_AsciiString.hxx>
#include <TDataStd_Integer.hxx>
#include <TDataStd_Real.hxx>
#include <charconv>
#include <stdexcept>
#include <string>

// M10 (schema 32): a pattern keeps its spacing on the label, body (1), kind (2), count (3),
// result (4), reversed (5), angle (6), and its axis: a base axis or edge as a construction axis
// stores it (7) or a construction axis (8). A copy keeps its index on the label and its
// pattern (1).
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
    return nativeDatumAxisRecords(parameters.axis);
}

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
}

PatternParameters readNativePattern(const TDF_Label& label, std::size_t remainingRecords,
                                    int schemaVersion) {
    require(schemaVersion >= 32 && label.NbChildren() == 7);
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
