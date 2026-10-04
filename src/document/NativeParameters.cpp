#include "NativeParameters.hpp"
#include "DocumentSerializationDetail.hpp"
#include <TDataStd_AsciiString.hxx>
#include <TDataStd_Integer.hxx>
#include <TDataStd_Real.hxx>
#include <charconv>
#include <stdexcept>
#include <string>
#include <voelund/document/IdCapacity.hpp>

namespace voelund::document::detail {
namespace {
void require(bool condition) {
    if (!condition)
        throw std::invalid_argument("Invalid native parameter data");
}
TDF_Label child(const TDF_Label& parent, int tag) {
    const auto label = parent.FindChild(tag, false);
    require(!label.IsNull());
    return label;
}
std::string text(const TDF_Label& label) {
    Handle(TDataStd_AsciiString) value;
    require(label.NbChildren() == 0 && label.FindAttribute(TDataStd_AsciiString::GetID(), value));
    const auto& string = value->Get();
    require(string.Length() <= static_cast<int>(maximumNameBytes));
    return {string.ToCString(), static_cast<std::size_t>(string.Length())};
}
std::uint64_t decimal(const TDF_Label& label, std::uint64_t capacity) {
    const auto encoded = text(label);
    std::uint64_t value = 0;
    const auto parsed = std::from_chars(encoded.data(), encoded.data() + encoded.size(), value);
    require(parsed.ec == std::errc{} && parsed.ptr == encoded.data() + encoded.size() &&
            encoded == std::to_string(value) && hasIdCapacity(value, capacity));
    return value;
}
int integer(const TDF_Label& label) {
    Handle(TDataStd_Integer) value;
    require(label.FindAttribute(TDataStd_Integer::GetID(), value));
    return value->Get();
}
} // namespace

std::size_t nativeParameterRecords(const ParameterTable& table) noexcept {
    return table.values.size();
}

void writeNativeParameterTable(const TDF_Label& label, const ParameterTable& table) {
    TDataStd_AsciiString::Set(label.FindChild(1), std::to_string(table.nextId).c_str());
    const auto values = label.FindChild(2);
    TDataStd_Integer::Set(values, static_cast<int>(table.values.size()));
    for (std::size_t index = 0; index < table.values.size(); ++index) {
        const auto& parameter = table.values[index];
        const auto native = values.FindChild(static_cast<int>(index + 1));
        TDataStd_AsciiString::Set(native.FindChild(1),
                                  std::to_string(parameter.id.value()).c_str());
        TDataStd_AsciiString::Set(native.FindChild(2), parameter.name.c_str());
        TDataStd_Integer::Set(native.FindChild(3), static_cast<int>(parameter.unit));
        TDataStd_Real::Set(native.FindChild(4), parameter.value);
        TDataStd_AsciiString::Set(native.FindChild(5), parameter.comment.c_str());
    }
}

ParameterTable readNativeParameterTable(const TDF_Label& label, std::size_t remainingRecords) {
    require(label.NbChildren() == 2 && label.NbAttributes() == 0);
    ParameterTable table;
    table.nextId = decimal(child(label, 1), 0);
    const auto values = child(label, 2);
    const int count = integer(values);
    require(count >= 0 && static_cast<std::size_t>(count) <= maximumParameterCount &&
            static_cast<std::size_t>(count) <= remainingRecords && values.NbChildren() == count);
    for (int index = 1; index <= count; ++index) {
        const auto native = child(values, index);
        require(native.NbChildren() == 5);
        const int unit = integer(child(native, 3));
        require(unit == static_cast<int>(ParameterUnit::length) ||
                unit == static_cast<int>(ParameterUnit::angle));
        Handle(TDataStd_Real) value;
        require(child(native, 4).FindAttribute(TDataStd_Real::GetID(), value));
        table.values.push_back({ParameterId{decimal(child(native, 1), 1)},
                                text(child(native, 2)), static_cast<ParameterUnit>(unit),
                                value->Get(), text(child(native, 5))});
    }
    // A stored table is never the default one; that is encoded by its absence.
    require(!(table == ParameterTable{}));
    validateParameterTable(table);
    return table;
}

void writeNativeParameterBinding(const TDF_Label& label, ParameterId id) {
    TDataStd_AsciiString::Set(label, std::to_string(id.value()).c_str());
}

ParameterId readNativeParameterBinding(const TDF_Label& label) {
    return ParameterId{decimal(label, 1)};
}

} // namespace voelund::document::detail
