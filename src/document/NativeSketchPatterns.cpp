#include "NativeSketchPatterns.hpp"
#include "DocumentSerializationDetail.hpp"
#include <TDataStd_AsciiString.hxx>
#include <TDataStd_Integer.hxx>
#include <TDataStd_RealArray.hxx>
#include <charconv>
#include <stdexcept>
#include <string>
#include <voelund/document/IdCapacity.hpp>

namespace voelund::document::detail {
namespace {
void require(bool value) {
    if (!value)
        throw std::invalid_argument("Invalid native sketch pattern");
}
TDF_Label child(const TDF_Label& parent, int tag) {
    auto result = parent.FindChild(tag, false);
    require(!result.IsNull());
    return result;
}
int integer(const TDF_Label& label) {
    Handle(TDataStd_Integer) value;
    require(label.FindAttribute(TDataStd_Integer::GetID(), value));
    return value->Get();
}
// A canonical decimal; 0 only where `zero` allows it.
std::uint64_t number(const TDF_Label& label, bool zero) {
    Handle(TDataStd_AsciiString) value;
    require(label.FindAttribute(TDataStd_AsciiString::GetID(), value) &&
            value->Get().Length() <= 32 && label.NbChildren() == 0);
    const std::string encoded = value->Get().ToCString();
    std::uint64_t result{};
    const auto parsed = std::from_chars(encoded.data(), encoded.data() + encoded.size(), result);
    require(parsed.ec == std::errc{} && parsed.ptr == encoded.data() + encoded.size() &&
            encoded == std::to_string(result) &&
            (result == 0 ? zero : hasIdCapacity(result, 0)));
    return result;
}
void writeNumber(const TDF_Label& label, std::uint64_t value) {
    TDataStd_AsciiString::Set(label, std::to_string(value).c_str());
}
void writeIds(const TDF_Label& label, const std::vector<SketchElementId>& ids) {
    TDataStd_Integer::Set(label, static_cast<int>(ids.size()));
    for (std::size_t index = 0; index < ids.size(); ++index)
        writeNumber(label.FindChild(static_cast<int>(index + 1)), ids[index].value());
}
std::vector<SketchElementId> readIds(const TDF_Label& label, std::size_t& remaining) {
    const int count = integer(label);
    require(count > 0 && static_cast<std::size_t>(count) <= maximumSketchElementCount &&
            static_cast<std::size_t>(count) <= remaining && label.NbChildren() == count);
    remaining -= static_cast<std::size_t>(count);
    std::vector<SketchElementId> ids;
    for (int index = 1; index <= count; ++index)
        ids.emplace_back(number(child(label, index), false));
    return ids;
}
} // namespace

std::size_t nativeSketchPatternRecords(const SketchPatterns& patterns) noexcept {
    std::size_t records = 0;
    for (const auto& pattern : patterns.values)
        records += 1 + pattern.sources.size() + pattern.instances.size();
    return records;
}

void writeNativeSketchPatterns(const TDF_Label& label, const SketchPatterns& patterns) {
    TDataStd_Integer::Set(label, static_cast<int>(patterns.values.size()));
    writeNumber(label.FindChild(1), patterns.nextId);
    for (std::size_t index = 0; index < patterns.values.size(); ++index) {
        const auto& pattern = patterns.values[index];
        const auto entry = label.FindChild(static_cast<int>(index + 2));
        auto values = TDataStd_RealArray::Set(entry, 1, 5);
        values->SetValue(1, pattern.step.x);
        values->SetValue(2, pattern.step.y);
        values->SetValue(3, pattern.center.x);
        values->SetValue(4, pattern.center.y);
        values->SetValue(5, pattern.angle);
        writeNumber(entry.FindChild(1), pattern.id);
        TDataStd_Integer::Set(entry.FindChild(2), static_cast<int>(pattern.kind));
        TDataStd_Integer::Set(entry.FindChild(3), static_cast<int>(pattern.count));
        writeNumber(entry.FindChild(4), pattern.axis ? pattern.axis->value() : 0);
        writeIds(entry.FindChild(5), pattern.sources);
        writeIds(entry.FindChild(6), pattern.instances);
    }
}

SketchPatterns readNativeSketchPatterns(const TDF_Label& label, std::size_t remainingRecords) {
    const int count = integer(label);
    // S14: an empty list keeps the counter of dissolved patterns (written only then).
    require(count >= 0 && static_cast<std::size_t>(count) <= maximumSketchElementCount &&
            static_cast<std::size_t>(count) <= remainingRecords &&
            label.NbChildren() == count + 1 && label.NbAttributes() == 1);
    SketchPatterns patterns;
    patterns.nextId = number(child(label, 1), true);
    require(count > 0 || patterns.nextId > 1);
    for (int index = 2; index <= count + 1; ++index) {
        const auto entry = child(label, index);
        require(entry.NbChildren() == 6 && remainingRecords > 0);
        --remainingRecords;
        Handle(TDataStd_RealArray) values;
        require(entry.FindAttribute(TDataStd_RealArray::GetID(), values) &&
                values->Lower() == 1 && values->Upper() == 5);
        const int kind = integer(child(entry, 2));
        const int positions = integer(child(entry, 3));
        require(kind >= 1 && kind <= 3 && positions >= 2 &&
                positions <= static_cast<int>(maximumSketchPatternCount));
        SketchPattern pattern{number(child(entry, 1), false), static_cast<SketchPatternKind>(kind),
                              {}, static_cast<std::uint32_t>(positions),
                              {values->Value(1), values->Value(2)},
                              {values->Value(3), values->Value(4)}, values->Value(5),
                              std::nullopt, {}};
        if (const auto axis = number(child(entry, 4), true); axis != 0)
            pattern.axis = SketchElementId{axis};
        pattern.sources = readIds(child(entry, 5), remainingRecords);
        pattern.instances = readIds(child(entry, 6), remainingRecords);
        patterns.values.push_back(std::move(pattern));
    }
    return patterns;
}

} // namespace voelund::document::detail
