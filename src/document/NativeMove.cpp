#include "NativeSolidFeatures.hpp"
#include <TDataStd_AsciiString.hxx>
#include <TDataStd_RealArray.hxx>
#include <charconv>
#include <stdexcept>
#include <string>

// M11 (schema 33): body (1); translation, rotation about X, Y, Z (radians) and pivot as nine
// reals (2).
namespace voelund::document::detail {
namespace {
void require(bool value) {
    if (!value)
        throw std::invalid_argument("Invalid native move data");
}
TDF_Label child(const TDF_Label& label, int tag) {
    const auto result = label.FindChild(tag, false);
    require(!result.IsNull());
    return result;
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

void writeNativeMove(const TDF_Label& label, const MoveParameters& parameters) {
    validateMoveParameters(parameters);
    TDataStd_AsciiString::Set(label.FindChild(1), std::to_string(parameters.body.value()).c_str());
    const auto& t = parameters.transform;
    const double values[] = {t.translationX,      t.translationY,      t.translationZ,
                             t.rotationX.value(), t.rotationY.value(), t.rotationZ.value(),
                             parameters.pivot[0], parameters.pivot[1], parameters.pivot[2]};
    const auto data = TDataStd_RealArray::Set(label.FindChild(2), 1, 9);
    for (int i = 1; i <= 9; ++i)
        data->SetValue(i, values[i - 1]);
}

MoveParameters readNativeMove(const TDF_Label& label, int schemaVersion) {
    require(schemaVersion >= 33 && label.NbChildren() == 2);
    Handle(TDataStd_RealArray) data;
    const auto values = child(label, 2);
    require(values.NbChildren() == 0 && values.FindAttribute(TDataStd_RealArray::GetID(), data) &&
            data->Lower() == 1 && data->Upper() == 9);
    MoveParameters result{BodyId{readId(child(label, 1))},
                          {data->Value(1), data->Value(2), data->Value(3),
                           kernel::Radians{data->Value(4)}, kernel::Radians{data->Value(5)},
                           kernel::Radians{data->Value(6)}},
                          {data->Value(7), data->Value(8), data->Value(9)}};
    validateMoveParameters(result);
    return result;
}

} // namespace voelund::document::detail
