#include "NativeSketchSupport.hpp"
#include "NativeSolidFeatures.hpp"
#include <TDataStd_AsciiString.hxx>
#include <TDataStd_Integer.hxx>
#include <TDataStd_Real.hxx>
#include <charconv>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// M25 (schema 29): angle on the label, body (1), drafted faces (2: count and origins), neutral
// face (3).
namespace voelund::document::detail {
namespace {
void require(bool value) {
    if (!value)
        throw std::invalid_argument("Invalid native draft data");
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
SubshapeOrigin readOrigin(const TDF_Label& label, std::size_t& remainingRecords,
                          int schemaVersion) {
    require(label.NbChildren() == 3 + nativeSubshapeOriginExtension(label));
    return readNativeSubshapeOrigin(label, remainingRecords, schemaVersion);
}
} // namespace

std::size_t nativeDraftRecords(const DraftParameters& parameters) {
    std::size_t records = parameters.faces.size() + parameters.neutral.extrusionBoundary.size();
    for (const auto& face : parameters.faces)
        records += face.extrusionBoundary.size();
    return records;
}

void writeNativeDraft(const TDF_Label& label, const DraftParameters& parameters) {
    validateDraftParameters(parameters);
    TDataStd_Real::Set(label, parameters.angle.value());
    TDataStd_AsciiString::Set(label.FindChild(1), std::to_string(parameters.body.value()).c_str());
    const auto faces = label.FindChild(2);
    TDataStd_Integer::Set(faces, static_cast<int>(parameters.faces.size()));
    for (std::size_t index = 0; index < parameters.faces.size(); ++index)
        writeNativeSubshapeOrigin(faces.FindChild(static_cast<int>(index + 1)),
                                  parameters.faces[index]);
    writeNativeSubshapeOrigin(label.FindChild(3), parameters.neutral);
}

DraftParameters readNativeDraft(const TDF_Label& label, std::size_t remainingRecords,
                                int schemaVersion) {
    require(schemaVersion >= 29 && label.NbChildren() == 3);
    Handle(TDataStd_Real) angle;
    Handle(TDataStd_AsciiString) body;
    const auto bodyLabel = child(label, 1);
    require(label.FindAttribute(TDataStd_Real::GetID(), angle) && bodyLabel.NbChildren() == 0 &&
            bodyLabel.FindAttribute(TDataStd_AsciiString::GetID(), body) &&
            body->Get().Length() <= 20);
    const std::string text{body->Get().ToCString(), static_cast<std::size_t>(body->Get().Length())};
    std::uint64_t id = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), id);
    require(parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() && id != 0 &&
            text == std::to_string(id));
    const auto faces = child(label, 2);
    const int count = integer(faces);
    require(count > 0 && static_cast<std::size_t>(count) <= maximumDraftFaces &&
            static_cast<std::size_t>(count) <= remainingRecords && faces.NbChildren() == count);
    remainingRecords -= static_cast<std::size_t>(count);
    std::vector<SubshapeOrigin> drafted;
    for (int index = 1; index <= count; ++index)
        drafted.push_back(readOrigin(child(faces, index), remainingRecords, schemaVersion));
    DraftParameters result{BodyId{id}, std::move(drafted),
                           readOrigin(child(label, 3), remainingRecords, schemaVersion),
                           kernel::Radians{angle->Get()}};
    validateDraftParameters(result);
    return result;
}

} // namespace voelund::document::detail
