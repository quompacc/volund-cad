#include "NativeSolidFeatures.hpp"
#include <TDataStd_AsciiString.hxx>
#include <TDataStd_Integer.hxx>
#include <charconv>
#include <stdexcept>
#include <string>

// Native encodings of the features added on 2026-10-14 (M19-M27).
namespace voelund::document::detail {
namespace {
void require(bool value) {
    if (!value)
        throw std::invalid_argument("Invalid native solid feature data");
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
void writeBody(const TDF_Label& label, BodyId body) {
    TDataStd_AsciiString::Set(label, std::to_string(body.value()).c_str());
}
BodyId readBody(const TDF_Label& label) {
    Handle(TDataStd_AsciiString) value;
    require(label.NbChildren() == 0 && label.FindAttribute(TDataStd_AsciiString::GetID(), value) &&
            value->Get().Length() <= 20);
    const std::string text{value->Get().ToCString(),
                           static_cast<std::size_t>(value->Get().Length())};
    std::uint64_t id = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), id);
    require(parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() && id != 0 &&
            text == std::to_string(id));
    return BodyId{id};
}
} // namespace

std::size_t nativeCombineRecords(const CombineParameters& parameters) {
    return parameters.tools.size();
}

void writeNativeCombine(const TDF_Label& label, const CombineParameters& parameters) {
    validateCombineParameters(parameters);
    TDataStd_Integer::Set(label, static_cast<int>(parameters.operation));
    writeBody(label.FindChild(1), parameters.target);
    const auto tools = label.FindChild(2);
    TDataStd_Integer::Set(tools, static_cast<int>(parameters.tools.size()));
    for (std::size_t index = 0; index < parameters.tools.size(); ++index)
        writeBody(tools.FindChild(static_cast<int>(index + 1)), parameters.tools[index]);
    TDataStd_Integer::Set(label.FindChild(3), parameters.keepTools ? 1 : 0);
}

CombineParameters readNativeCombine(const TDF_Label& label, std::size_t remainingRecords,
                                    int schemaVersion) {
    require(schemaVersion >= 18 && label.NbChildren() == 3);
    const int operation = integer(label);
    require(operation >= 1 && operation <= 3);
    CombineParameters result{readBody(child(label, 1)), {},
                             static_cast<CombineOperation>(operation), false};
    const auto tools = child(label, 2);
    const int count = integer(tools);
    require(count > 0 && static_cast<std::size_t>(count) <= maximumCombineTools &&
            static_cast<std::size_t>(count) <= remainingRecords && tools.NbChildren() == count);
    for (int index = 1; index <= count; ++index)
        result.tools.push_back(readBody(child(tools, index)));
    const auto keep = child(label, 3);
    const int kept = integer(keep);
    require(keep.NbChildren() == 0 && (kept == 0 || kept == 1));
    result.keepTools = kept == 1;
    validateCombineParameters(result);
    return result;
}

} // namespace voelund::document::detail
