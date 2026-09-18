#include "NativeSketchSupport.hpp"
#include <TDataStd_AsciiString.hxx>
#include <TDataStd_Integer.hxx>
#include <TDataStd_RealArray.hxx>
#include <charconv>
#include <voelund/document/IdCapacity.hpp>
#include <voelund/document/SketchPlaneValidation.hpp>

namespace voelund::document::detail {
namespace {
void require(bool value) {
    if (!value)
        throw std::invalid_argument("Invalid native sketch support binding");
}
TDF_Label child(const TDF_Label& parent, int tag) {
    auto result = parent.FindChild(tag, false);
    require(!result.IsNull());
    return result;
}
std::string text(const TDF_Label& label) {
    Handle(TDataStd_AsciiString) value;
    require(label.FindAttribute(TDataStd_AsciiString::GetID(), value) &&
            value->Get().Length() <= 32);
    return value->Get().ToCString();
}
std::uint64_t number(const TDF_Label& label) {
    const auto encoded = text(label);
    std::uint64_t value{};
    const auto parsed = std::from_chars(encoded.data(), encoded.data() + encoded.size(), value);
    require(parsed.ec == std::errc{} && parsed.ptr == encoded.data() + encoded.size() &&
            encoded == std::to_string(value) && hasIdCapacity(value, 1));
    return value;
}
int integer(const TDF_Label& label) {
    Handle(TDataStd_Integer) value;
    require(label.FindAttribute(TDataStd_Integer::GetID(), value));
    return value->Get();
}
} // namespace
void writeNativeSketchSupport(const TDF_Label& label, const SketchSupportBinding& binding) {
    const auto& ref = binding.reference;
    validateSubshapeOrigin(ref.origin);
    validateCanonicalSketchPlane(binding.localPlane);
    TDataStd_AsciiString::Set(label, documentIdToHex(ref.document).c_str());
    TDataStd_AsciiString::Set(label.FindChild(1), std::to_string(ref.body.value()).c_str());
    TDataStd_AsciiString::Set(label.FindChild(2),
                              std::to_string(ref.origin.feature.value()).c_str());
    TDataStd_Integer::Set(label.FindChild(3),
                          ref.origin.role ? static_cast<int>(*ref.origin.role) : -1);
    const auto boundary = label.FindChild(4);
    TDataStd_Integer::Set(boundary, static_cast<int>(ref.origin.extrusionBoundary.size()));
    int index = 0;
    for (const auto id : ref.origin.extrusionBoundary)
        TDataStd_AsciiString::Set(boundary.FindChild(++index), std::to_string(id.value()).c_str());
    const auto& p = binding.localPlane;
    const double values[] = {p.originX, p.originY,     p.originZ,     p.normalX,    p.normalY,
                             p.normalZ, p.xDirectionX, p.xDirectionY, p.xDirectionZ};
    const auto data = TDataStd_RealArray::Set(label.FindChild(5), 1, 9);
    for (int i = 1; i <= 9; ++i)
        data->SetValue(i, values[i - 1]);
}
SketchSupportBinding readNativeSketchSupport(const TDF_Label& label,
                                             std::size_t remainingElements) {
    require(label.NbChildren() == 5);
    SubshapeOrigin origin{FeatureId{number(child(label, 2))}, std::nullopt};
    const int role = integer(child(label, 3));
    require(role >= -1 && role <= static_cast<int>(kernel::PrimitiveRole::cylinderSide));
    if (role >= 0)
        origin.role = static_cast<kernel::PrimitiveRole>(role);
    const auto boundary = child(label, 4);
    const int count = integer(boundary);
    require(count >= 0 && static_cast<std::size_t>(count) <= remainingElements &&
            boundary.NbChildren() == count);
    for (int i = 1; i <= count; ++i)
        origin.extrusionBoundary.emplace_back(number(child(boundary, i)));
    validateSubshapeOrigin(origin);
    Handle(TDataStd_RealArray) data;
    require(child(label, 5).FindAttribute(TDataStd_RealArray::GetID(), data) &&
            data->Lower() == 1 && data->Upper() == 9);
    SketchSupportBinding result{
        {documentIdFromHex(text(label)), BodyId{number(child(label, 1))}, origin},
        {data->Value(1), data->Value(2), data->Value(3), data->Value(4), data->Value(5),
         data->Value(6), data->Value(7), data->Value(8), data->Value(9)}};
    validateCanonicalSketchPlane(result.localPlane);
    return result;
}
} // namespace voelund::document::detail
