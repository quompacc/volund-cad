#include "NativeSketchSupport.hpp"
#include "NativeSolidFeatures.hpp"
#include <TDataStd_AsciiString.hxx>
#include <TDataStd_Integer.hxx>
#include <charconv>
#include <stdexcept>
#include <string>

// M09 (schema 31): mode on the label, body (1), base plane code (2, 0 for none), and else the
// construction plane (3) or the face (4: its body on child 1 and its origin).
namespace voelund::document::detail {
namespace {
void require(bool value) {
    if (!value)
        throw std::invalid_argument("Invalid native mirror data");
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
void writeId(const TDF_Label& label, std::uint64_t id) {
    TDataStd_AsciiString::Set(label, std::to_string(id).c_str());
}
std::uint64_t readId(const TDF_Label& label) {
    Handle(TDataStd_AsciiString) value;
    require(label.FindAttribute(TDataStd_AsciiString::GetID(), value) &&
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

std::size_t nativeMirrorRecords(const MirrorParameters& parameters) {
    return parameters.plane.face ? parameters.plane.face->origin.extrusionBoundary.size() : 0;
}

void writeNativeMirror(const TDF_Label& label, const MirrorParameters& parameters) {
    validateMirrorParameters(parameters);
    TDataStd_Integer::Set(label, static_cast<int>(parameters.mode));
    writeId(label.FindChild(1), parameters.body.value());
    const auto& plane = parameters.plane;
    TDataStd_Integer::Set(label.FindChild(2),
                          plane.basePlane ? static_cast<int>(*plane.basePlane) : 0);
    if (plane.constructionPlane)
        writeId(label.FindChild(3), plane.constructionPlane->value());
    if (plane.face) {
        const auto face = label.FindChild(4);
        writeId(face.FindChild(1), plane.face->body.value());
        writeNativeSubshapeOrigin(face, plane.face->origin);
    }
}

MirrorParameters readNativeMirror(const TDF_Label& label, std::size_t remainingRecords,
                                  int schemaVersion) {
    require(schemaVersion >= 31);
    const int mode = integer(label);
    require(mode == static_cast<int>(MirrorMode::newBody) ||
            mode == static_cast<int>(MirrorMode::join));
    const auto body = child(label, 1);
    require(body.NbChildren() == 0);
    MirrorParameters result{BodyId{readId(body)}, {}, static_cast<MirrorMode>(mode)};
    const auto base = child(label, 2);
    const int code = integer(base);
    // A base plane needs no third child; a construction plane or a face is one.
    require(base.NbChildren() == 0 && code >= 0 && code <= 3 &&
            label.NbChildren() == (code != 0 ? 2 : 3));
    if (code != 0) {
        result.plane.basePlane = static_cast<DatumPlane>(code);
    } else if (const auto plane = label.FindChild(3, false); !plane.IsNull()) {
        require(plane.NbChildren() == 0);
        result.plane.constructionPlane = FeatureId{readId(plane)};
    } else {
        const auto face = child(label, 4);
        require(face.NbChildren() == 4 + nativeSubshapeOriginExtension(face));
        const auto faceBody = child(face, 1);
        require(faceBody.NbChildren() == 0);
        result.plane.face = BodyFaceReference{
            BodyId{readId(faceBody)}, readNativeSubshapeOrigin(face, remainingRecords, schemaVersion)};
    }
    validateMirrorParameters(result);
    return result;
}

} // namespace voelund::document::detail
