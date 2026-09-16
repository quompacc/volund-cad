#include "NativeExtrusion.hpp"
#include "DocumentSerializationDetail.hpp"
#include <voelund/kernel/NumericPolicy.hpp>
#include <TDataStd_AsciiString.hxx>
#include <TDataStd_Integer.hxx>
#include <TDataStd_Real.hxx>
#include <charconv>
#include <set>

namespace voelund::document::detail {
namespace {
void require(bool value) {
    if (!value) throw std::invalid_argument("Invalid native extrusion data");
}
TDF_Label child(const TDF_Label& label, int tag) {
    const auto result = label.FindChild(tag, false);
    require(!result.IsNull());
    return result;
}
}
void validateNativeExtrusion(const ExtrusionParameters& parameters) {
    require(kernel::modelLength(parameters.distance) &&
        parameters.profiles.size() <= maximumExtrusionProfileCount);
    std::set<std::uint64_t> ids;
    for (const auto id : parameters.profiles)
        require(id.value() != 0 && ids.insert(id.value()).second);
}
void writeNativeExtrusion(const TDF_Label& label, const ExtrusionParameters& parameters) {
    TDataStd_Real::Set(label, parameters.distance);
    const auto profiles = label.FindChild(1);
    TDataStd_Integer::Set(profiles, static_cast<int>(parameters.profiles.size()));
    for (std::size_t index = 0; index < parameters.profiles.size(); ++index)
        TDataStd_AsciiString::Set(profiles.FindChild(static_cast<int>(index + 1)),
            std::to_string(parameters.profiles[index].value()).c_str());
}
ExtrusionParameters readNativeExtrusion(const TDF_Label& label, std::size_t remainingRecords) {
    require(label.NbChildren() == 1);
    Handle(TDataStd_Real) distance;
    require(label.FindAttribute(TDataStd_Real::GetID(), distance));
    const auto profiles = child(label, 1);
    Handle(TDataStd_Integer) size;
    require(profiles.FindAttribute(TDataStd_Integer::GetID(), size));
    const int count = size->Get();
    require(count >= 0 && count <= static_cast<int>(maximumExtrusionProfileCount) &&
        static_cast<std::size_t>(count) <= remainingRecords && profiles.NbChildren() == count);
    ExtrusionParameters result{distance->Get(), {}};
    for (int index = 1; index <= count; ++index) {
        const auto profile = child(profiles, index);
        require(profile.NbChildren() == 0);
        Handle(TDataStd_AsciiString) value;
        require(profile.FindAttribute(TDataStd_AsciiString::GetID(), value) && value->Get().Length() <= 20);
        const std::string text{value->Get().ToCString(), static_cast<std::size_t>(value->Get().Length())};
        std::uint64_t id = 0;
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), id);
        require(parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() &&
            id != 0 && text == std::to_string(id));
        // Profile IDs are historical selections, not allocation counters or element references.
        result.profiles.emplace_back(id);
    }
    validateNativeExtrusion(result);
    return result;
}
}
