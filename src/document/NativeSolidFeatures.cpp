// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "NativeSolidFeatures.hpp"
#include "NativeSketchSupport.hpp"
#include <TDataStd_AsciiString.hxx>
#include <TDataStd_Integer.hxx>
#include <TDataStd_Real.hxx>
#include <charconv>
#include <stdexcept>
#include <string>
#include <variant>

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
double real(const TDF_Label& label) {
    Handle(TDataStd_Real) value;
    require(label.FindAttribute(TDataStd_Real::GetID(), value));
    return value->Get();
}
SubshapeOrigin readOrigin(const TDF_Label& label, std::size_t& remainingRecords,
                          int schemaVersion) {
    require(label.NbChildren() == 3 + nativeSubshapeOriginExtension(label));
    return readNativeSubshapeOrigin(label, remainingRecords, schemaVersion);
}
} // namespace

std::size_t nativeDatumPlaneRecords(const DatumPlaneParameters& parameters) {
    return parameters.face ? parameters.face->origin.extrusionBoundary.size() : 0;
}

void writeNativeDatumPlane(const TDF_Label& label, const DatumPlaneParameters& parameters) {
    validateDatumPlaneParameters(parameters);
    TDataStd_Real::Set(label, parameters.offset);
    TDataStd_Integer::Set(label.FindChild(1),
                          parameters.basePlane ? static_cast<int>(*parameters.basePlane) : 0);
    if (parameters.face) {
        writeBody(label.FindChild(2), parameters.face->body);
        writeNativeSubshapeOrigin(label.FindChild(3), parameters.face->origin);
    }
    TDataStd_Integer::Set(label.FindChild(4), static_cast<int>(parameters.rotationAxis));
    TDataStd_Real::Set(label.FindChild(5), parameters.angle.value());
}

DatumPlaneParameters readNativeDatumPlane(const TDF_Label& label, std::size_t remainingRecords,
                                          int schemaVersion) {
    const int base = integer(child(label, 1));
    require(schemaVersion >= 19 && base >= 0 && base <= 3 &&
            label.NbChildren() == (base == 0 ? 5 : 3));
    DatumPlaneParameters result;
    result.offset = real(label);
    if (base != 0)
        result.basePlane = static_cast<DatumPlane>(base);
    else
        result.face =
            BodyFaceReference{readBody(child(label, 2)),
                              readOrigin(child(label, 3), remainingRecords, schemaVersion)};
    const int axis = integer(child(label, 4));
    require(axis >= 0 && axis <= 2);
    result.rotationAxis = static_cast<DatumRotationAxis>(axis);
    result.angle = kernel::Radians{real(child(label, 5))};
    validateDatumPlaneParameters(result);
    return result;
}

std::size_t nativeDatumAxisRecords(const DatumAxisParameters& parameters) {
    return parameters.edge ? parameters.edge->first.extrusionBoundary.size() +
                                 parameters.edge->second.extrusionBoundary.size()
                           : 0;
}

void writeNativeDatumAxis(const TDF_Label& label, const DatumAxisParameters& parameters) {
    validateDatumAxisParameters(parameters);
    TDataStd_Integer::Set(label, parameters.baseAxis ? static_cast<int>(*parameters.baseAxis) : 0);
    if (!parameters.edge)
        return;
    writeBody(label.FindChild(1), *parameters.body);
    writeNativeSubshapeOrigin(label.FindChild(2), parameters.edge->first);
    writeNativeSubshapeOrigin(label.FindChild(3), parameters.edge->second);
    TDataStd_AsciiString::Set(label.FindChild(4), std::to_string(parameters.edge->id).c_str());
}

DatumAxisParameters readNativeDatumAxis(const TDF_Label& label, std::size_t remainingRecords,
                                        int schemaVersion) {
    const int base = integer(label);
    require(schemaVersion >= 19 && base >= 0 && base <= 3 &&
            label.NbChildren() == (base == 0 ? 4 : 0));
    DatumAxisParameters result;
    if (base != 0) {
        result.baseAxis = static_cast<BaseAxis>(base);
    } else {
        result.body = readBody(child(label, 1));
        auto first = readOrigin(child(label, 2), remainingRecords, schemaVersion);
        auto second = readOrigin(child(label, 3), remainingRecords, schemaVersion);
        result.edge =
            FilletEdge{std::move(first), std::move(second), readBody(child(label, 4)).value()};
    }
    validateDatumAxisParameters(result);
    return result;
}

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
    CombineParameters result{
        readBody(child(label, 1)), {}, static_cast<CombineOperation>(operation), false};
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

// M22 (schema 21): thickness on the label, body (1), open faces (2), outward (3).
std::size_t nativeShellRecords(const ShellParameters& parameters) {
    std::size_t records = parameters.openFaces.size();
    for (const auto& face : parameters.openFaces)
        records += face.extrusionBoundary.size();
    return records;
}

void writeNativeShell(const TDF_Label& label, const ShellParameters& parameters) {
    validateShellParameters(parameters);
    TDataStd_Real::Set(label, parameters.thickness);
    writeBody(label.FindChild(1), parameters.body);
    const auto faces = label.FindChild(2);
    TDataStd_Integer::Set(faces, static_cast<int>(parameters.openFaces.size()));
    for (std::size_t index = 0; index < parameters.openFaces.size(); ++index)
        writeNativeSubshapeOrigin(faces.FindChild(static_cast<int>(index + 1)),
                                  parameters.openFaces[index]);
    TDataStd_Integer::Set(label.FindChild(3), parameters.outward ? 1 : 0);
}

ShellParameters readNativeShell(const TDF_Label& label, std::size_t remainingRecords,
                                int schemaVersion) {
    require(schemaVersion >= 21 && label.NbChildren() == 3);
    ShellParameters result{readBody(child(label, 1)), {}, real(label), false};
    const auto faces = child(label, 2);
    const int count = integer(faces);
    require(count > 0 && static_cast<std::size_t>(count) <= maximumShellFaces &&
            static_cast<std::size_t>(count) <= remainingRecords && faces.NbChildren() == count);
    remainingRecords -= static_cast<std::size_t>(count);
    for (int index = 1; index <= count; ++index)
        result.openFaces.push_back(
            readOrigin(child(faces, index), remainingRecords, schemaVersion));
    const int outward = integer(child(label, 3));
    require(outward == 0 || outward == 1);
    result.outward = outward == 1;
    validateShellParameters(result);
    return result;
}

namespace {
struct SolidKind final {
    FeatureKind kind;
    const char* name;
    int schema;
};
constexpr SolidKind solidKinds[]{{FeatureKind::combine, "combine", 18},
                                 {FeatureKind::datumPlane, "datum_plane", 19},
                                 {FeatureKind::datumAxis, "datum_axis", 19},
                                 {FeatureKind::shell, "shell", 21},
                                 {FeatureKind::primitive, "primitive", 25},
                                 {FeatureKind::draft, "draft", 29},
                                 {FeatureKind::mirror, "mirror", 31},
                                 {FeatureKind::pattern, "pattern", 32},
                                 {FeatureKind::patternCopy, "pattern_copy", 32},
                                 {FeatureKind::move, "move", 33},
                                 {FeatureKind::stepImport, "step_import", 35},
                                 {FeatureKind::importPart, "import_part", 35},
                                 {FeatureKind::split, "split", 50},
                                 {FeatureKind::splitPart, "split_part", 50}};
} // namespace

const char* nativeSolidKindName(FeatureKind kind) noexcept {
    for (const auto& item : solidKinds)
        if (item.kind == kind)
            return item.name;
    return nullptr;
}

std::optional<FeatureKind> nativeSolidKind(std::string_view name, int schemaVersion) {
    for (const auto& item : solidKinds)
        if (name == item.name && schemaVersion >= item.schema)
            return item.kind;
    return std::nullopt;
}

namespace {
// Records beyond the feature, after checking its parameters (N12a: by type, not by a switch).
std::size_t solidRecords(const CombineParameters& values) {
    validateCombineParameters(values);
    return nativeCombineRecords(values);
}
std::size_t solidRecords(const DatumPlaneParameters& values) {
    validateDatumPlaneParameters(values);
    return nativeDatumPlaneRecords(values);
}
std::size_t solidRecords(const DatumAxisParameters& values) {
    validateDatumAxisParameters(values);
    return nativeDatumAxisRecords(values);
}
std::size_t solidRecords(const ShellParameters& values) {
    validateShellParameters(values);
    return nativeShellRecords(values);
}
std::size_t solidRecords(const DraftParameters& values) { // M25
    validateDraftParameters(values);
    return nativeDraftRecords(values);
}
std::size_t solidRecords(const MirrorParameters& values) { // M09
    validateMirrorParameters(values);
    return nativeMirrorRecords(values);
}
std::size_t solidRecords(const PatternParameters& values) { // M10
    validatePatternParameters(values);
    return nativePatternRecords(values);
}
std::size_t solidRecords(const ImportParameters& values) { // M17
    validateImportParameters(values);
    return nativeImportRecords(values);
}
std::size_t solidRecords(const SplitParameters& values) { // N12a
    validateSplitParameters(values);
    return nativeSplitRecords(values);
}
std::size_t solidRecords(const MoveParameters& values) { // M11: no records beyond the feature
    validateMoveParameters(values);
    return 0;
}
std::size_t solidRecords(const PrimitiveParameters& values) { // M27: none either
    validatePrimitiveParameters(values);
    return 0;
}
std::size_t solidRecords(const PatternCopyParameters&) { return 0; }
std::size_t solidRecords(const ImportPartParameters&) { return 0; }
std::size_t solidRecords(const SplitPartParameters&) { return 0; }
template <class Other> std::size_t solidRecords(const Other&) {
    throw std::invalid_argument("Not a natively stored solid feature");
}

void writeSolid(const TDF_Label& label, const CombineParameters& values) {
    writeNativeCombine(label, values);
}
void writeSolid(const TDF_Label& label, const DatumPlaneParameters& values) {
    writeNativeDatumPlane(label, values);
}
void writeSolid(const TDF_Label& label, const DatumAxisParameters& values) {
    writeNativeDatumAxis(label, values);
}
void writeSolid(const TDF_Label& label, const ShellParameters& values) {
    writeNativeShell(label, values);
}
void writeSolid(const TDF_Label& label, const PrimitiveParameters& values) {
    writeNativePrimitive(label, values);
}
void writeSolid(const TDF_Label& label, const DraftParameters& values) {
    writeNativeDraft(label, values);
}
void writeSolid(const TDF_Label& label, const MirrorParameters& values) {
    writeNativeMirror(label, values);
}
void writeSolid(const TDF_Label& label, const PatternParameters& values) {
    writeNativePattern(label, values);
}
void writeSolid(const TDF_Label& label, const PatternCopyParameters& values) {
    writeNativePatternCopy(label, values);
}
void writeSolid(const TDF_Label& label, const MoveParameters& values) {
    writeNativeMove(label, values);
}
void writeSolid(const TDF_Label& label, const ImportParameters& values) {
    writeNativeImport(label, values);
}
void writeSolid(const TDF_Label& label, const ImportPartParameters& values) {
    writeNativeImportPart(label, values);
}
void writeSolid(const TDF_Label& label, const SplitParameters& values) {
    writeNativeSplit(label, values);
}
void writeSolid(const TDF_Label& label, const SplitPartParameters& values) {
    writeNativeSplitPart(label, values);
}
template <class Other> void writeSolid(const TDF_Label&, const Other&) {
    throw std::invalid_argument("Not a natively stored solid feature");
}

using Reader = FeatureParameters (*)(const TDF_Label&, std::size_t, int);
struct SolidReader final {
    FeatureKind kind;
    Reader read;
};
constexpr SolidReader readers[]{
    {FeatureKind::combine,
     [](const TDF_Label& label, std::size_t records, int schema) -> FeatureParameters {
         return readNativeCombine(label, records, schema);
     }},
    {FeatureKind::datumPlane,
     [](const TDF_Label& label, std::size_t records, int schema) -> FeatureParameters {
         return readNativeDatumPlane(label, records, schema);
     }},
    {FeatureKind::datumAxis,
     [](const TDF_Label& label, std::size_t records, int schema) -> FeatureParameters {
         return readNativeDatumAxis(label, records, schema);
     }},
    {FeatureKind::shell,
     [](const TDF_Label& label, std::size_t records, int schema) -> FeatureParameters {
         return readNativeShell(label, records, schema);
     }},
    {FeatureKind::primitive,
     [](const TDF_Label& label, std::size_t, int schema) -> FeatureParameters {
         return readNativePrimitive(label, schema);
     }},
    {FeatureKind::draft,
     [](const TDF_Label& label, std::size_t records, int schema) -> FeatureParameters {
         return readNativeDraft(label, records, schema);
     }},
    {FeatureKind::mirror,
     [](const TDF_Label& label, std::size_t records, int schema) -> FeatureParameters {
         return readNativeMirror(label, records, schema);
     }},
    {FeatureKind::pattern,
     [](const TDF_Label& label, std::size_t records, int schema) -> FeatureParameters {
         return readNativePattern(label, records, schema);
     }},
    {FeatureKind::patternCopy,
     [](const TDF_Label& label, std::size_t, int schema) -> FeatureParameters {
         return readNativePatternCopy(label, schema);
     }},
    {FeatureKind::move,
     [](const TDF_Label& label, std::size_t, int schema) -> FeatureParameters {
         return readNativeMove(label, schema);
     }},
    {FeatureKind::stepImport,
     [](const TDF_Label& label, std::size_t records, int schema) -> FeatureParameters {
         return readNativeImport(label, records, schema);
     }},
    {FeatureKind::importPart,
     [](const TDF_Label& label, std::size_t, int schema) -> FeatureParameters {
         return readNativeImportPart(label, schema);
     }},
    {FeatureKind::split,
     [](const TDF_Label& label, std::size_t records, int schema) -> FeatureParameters {
         return readNativeSplit(label, records, schema);
     }},
    {FeatureKind::splitPart,
     [](const TDF_Label& label, std::size_t, int schema) -> FeatureParameters {
         return readNativeSplitPart(label, schema);
     }},
};
} // namespace

std::size_t nativeSolidRecords(FeatureKind kind, const FeatureParameters& parameters) {
    if (nativeSolidKindName(kind) == nullptr)
        throw std::invalid_argument("Not a natively stored solid feature");
    require(parameters.index() == static_cast<std::size_t>(kind));
    return std::visit([](const auto& values) { return solidRecords(values); }, parameters);
}

void writeNativeSolid(const TDF_Label& label, const FeatureParameters& parameters) {
    std::visit([&](const auto& values) { writeSolid(label, values); }, parameters);
}

FeatureParameters readNativeSolid(const TDF_Label& label, FeatureKind kind,
                                  std::size_t remainingRecords, int schemaVersion) {
    for (const auto& reader : readers)
        if (reader.kind == kind)
            return reader.read(label, remainingRecords, schemaVersion);
    throw std::invalid_argument("Not a natively stored solid feature");
}

} // namespace voelund::document::detail
