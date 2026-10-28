// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "NativeSketchSupport.hpp"
#include "NativeSolidFeatures.hpp"
#include <TDataStd_AsciiString.hxx>
#include <TDataStd_Integer.hxx>
#include <charconv>
#include <stdexcept>
#include <string>

// M09 (schema 31): mode on the label, body (1), base plane code (2, 0 for none), and else the
// construction plane (3) or the face (4: its body on child 1 and its origin). N11b (schema 48):
// the repeated steps (5).
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
// N11b: the mirrored steps (5), since schema 48.
std::vector<FeatureId> readMirroredSteps(const TDF_Label& label, int schemaVersion) {
    const auto steps = label.FindChild(5, false);
    if (steps.IsNull())
        return {};
    require(schemaVersion >= 48);
    return readNativeSteps(steps);
}

// A construction plane (3) or a body face (4).
void readPlane(const TDF_Label& label, MirrorPlane& result, std::size_t remainingRecords,
               int schemaVersion) {
    if (const auto plane = label.FindChild(3, false); !plane.IsNull()) {
        require(plane.NbChildren() == 0);
        result.constructionPlane = FeatureId{readId(plane)};
        return;
    }
    const auto face = child(label, 4);
    require(face.NbChildren() == 4 + nativeSubshapeOriginExtension(face));
    const auto faceBody = child(face, 1);
    require(faceBody.NbChildren() == 0);
    result.face = BodyFaceReference{
        BodyId{readId(faceBody)}, readNativeSubshapeOrigin(face, remainingRecords, schemaVersion)};
}
// The plane of a mirror or split: base plane code (2, 0 for none), construction plane (3) or
// face (4: its body on child 1 and its origin).
void writePlane(const TDF_Label& label, const MirrorPlane& plane) {
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

// The plane; `children` counts the label's further children (body, steps).
MirrorPlane readMirrorPlane(const TDF_Label& label, int children, std::size_t remainingRecords,
                            int schemaVersion) {
    const auto base = child(label, 2);
    const int code = integer(base);
    // A base plane needs no third child; a construction plane or a face is one.
    require(base.NbChildren() == 0 && code >= 0 && code <= 3 &&
            label.NbChildren() == children + (code != 0 ? 1 : 2));
    MirrorPlane result;
    if (code != 0)
        result.basePlane = static_cast<DatumPlane>(code);
    else
        readPlane(label, result, remainingRecords, schemaVersion);
    return result;
}
} // namespace

std::size_t nativeMirrorRecords(const MirrorParameters& parameters) {
    return parameters.plane.face ? parameters.plane.face->origin.extrusionBoundary.size() : 0;
}

void writeNativeMirror(const TDF_Label& label, const MirrorParameters& parameters) {
    validateMirrorParameters(parameters);
    TDataStd_Integer::Set(label, static_cast<int>(parameters.mode));
    writeId(label.FindChild(1), parameters.body.value());
    writePlane(label, parameters.plane);
    if (!parameters.features.empty())
        writeNativeSteps(label.FindChild(5), parameters.features);
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
    const int extra = label.FindChild(5, false).IsNull() ? 0 : 1;
    result.plane = readMirrorPlane(label, 1 + extra, remainingRecords, schemaVersion);
    result.features = readMirroredSteps(label, schemaVersion);
    validateMirrorParameters(result);
    return result;
}

// N12a (schema 50): a split keeps its side on the label, its body (1) and its plane as a
// mirror's; a split part names its split (1).
std::size_t nativeSplitRecords(const SplitParameters& parameters) {
    return parameters.plane.face ? parameters.plane.face->origin.extrusionBoundary.size() : 0;
}

void writeNativeSplit(const TDF_Label& label, const SplitParameters& parameters) {
    validateSplitParameters(parameters);
    TDataStd_Integer::Set(label, static_cast<int>(parameters.keep));
    writeId(label.FindChild(1), parameters.body.value());
    writePlane(label, parameters.plane);
}

SplitParameters readNativeSplit(const TDF_Label& label, std::size_t remainingRecords,
                                int schemaVersion) {
    require(schemaVersion >= 50);
    const int keep = integer(label);
    require(keep >= static_cast<int>(SplitKeep::both) && keep <= static_cast<int>(SplitKeep::back));
    const auto body = child(label, 1);
    require(body.NbChildren() == 0);
    SplitParameters result{BodyId{readId(body)},
                           readMirrorPlane(label, 1, remainingRecords, schemaVersion),
                           static_cast<SplitKeep>(keep)};
    validateSplitParameters(result);
    return result;
}

void writeNativeSplitPart(const TDF_Label& label, const SplitPartParameters& parameters) {
    writeId(label.FindChild(1), parameters.split.value());
}

SplitPartParameters readNativeSplitPart(const TDF_Label& label, int schemaVersion) {
    require(schemaVersion >= 50 && label.NbChildren() == 1);
    const auto split = child(label, 1);
    require(split.NbChildren() == 0);
    return {FeatureId{readId(split)}};
}

} // namespace voelund::document::detail
