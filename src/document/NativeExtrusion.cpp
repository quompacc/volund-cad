#include "NativeExtrusion.hpp"
#include "DocumentSerializationDetail.hpp"
#include "NativeParameters.hpp"
#include "NativeSketchSupport.hpp"
#include <voelund/kernel/NumericPolicy.hpp>
#include <TDataStd_AsciiString.hxx>
#include <TDataStd_Integer.hxx>
#include <TDataStd_Real.hxx>
#include <charconv>
#include <cmath>
#include <set>
#include <string>

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
// AsciiString attributes are bounded by maximumNameBytes; long signatures are chunked.
constexpr std::size_t regionChunkBytes = 4000;
std::size_t chunkCount(const SketchRegionId& region) {
    return (region.text().size() + regionChunkBytes - 1) / regionChunkBytes;
}
void writeRegions(const TDF_Label& label, const std::vector<SketchRegionId>& regions) {
    TDataStd_Integer::Set(label, static_cast<int>(regions.size()));
    for (std::size_t index = 0; index < regions.size(); ++index) {
        const auto region = label.FindChild(static_cast<int>(index + 1));
        const auto& text = regions[index].text();
        TDataStd_Integer::Set(region, static_cast<int>(chunkCount(regions[index])));
        for (std::size_t chunk = 0; chunk < chunkCount(regions[index]); ++chunk)
            TDataStd_AsciiString::Set(region.FindChild(static_cast<int>(chunk + 1)),
                text.substr(chunk * regionChunkBytes, regionChunkBytes).c_str());
    }
}
std::vector<SketchRegionId> readRegions(const TDF_Label& label, std::size_t& remainingRecords) {
    Handle(TDataStd_Integer) size;
    require(label.FindAttribute(TDataStd_Integer::GetID(), size));
    const int count = size->Get();
    require(count >= 0 && count <= static_cast<int>(maximumExtrusionProfileCount) &&
        static_cast<std::size_t>(count) <= remainingRecords && label.NbChildren() == count);
    remainingRecords -= static_cast<std::size_t>(count);
    std::vector<SketchRegionId> regions;
    for (int index = 1; index <= count; ++index) {
        const auto region = child(label, index);
        Handle(TDataStd_Integer) chunks;
        require(region.FindAttribute(TDataStd_Integer::GetID(), chunks));
        const int pieces = chunks->Get();
        require(pieces >= 1 && static_cast<std::size_t>(pieces) <= remainingRecords &&
            region.NbChildren() == pieces);
        remainingRecords -= static_cast<std::size_t>(pieces);
        std::string text;
        for (int chunk = 1; chunk <= pieces; ++chunk) {
            const auto part = child(region, chunk);
            Handle(TDataStd_AsciiString) value;
            require(part.NbChildren() == 0 &&
                part.FindAttribute(TDataStd_AsciiString::GetID(), value));
            const auto length = static_cast<std::size_t>(value->Get().Length());
            // Every chunk but the last is full, so the split is canonical as well.
            require(length >= 1 && length <= regionChunkBytes &&
                (chunk == pieces || length == regionChunkBytes));
            text.append(value->Get().ToCString(), length);
        }
        const auto id = SketchRegionId::parse(text);
        require(id.has_value());
        regions.push_back(*id);
    }
    return regions;
}
// Decimal text like profile IDs, so 64-bit body IDs fit.
std::uint64_t readDecimal(const TDF_Label& label) {
    Handle(TDataStd_AsciiString) value;
    require(label.NbChildren() == 0 && label.FindAttribute(TDataStd_AsciiString::GetID(), value) &&
        value->Get().Length() <= 20);
    const std::string text{value->Get().ToCString(), static_cast<std::size_t>(value->Get().Length())};
    std::uint64_t id = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), id);
    require(parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() && id != 0 &&
        text == std::to_string(id));
    return id;
}
int readInteger(const TDF_Label& label) {
    Handle(TDataStd_Integer) value;
    require(label.NbChildren() == 0 && label.FindAttribute(TDataStd_Integer::GetID(), value));
    return value->Get();
}
}
std::size_t nativeExtrusionRecords(const ExtrusionParameters& parameters) {
    std::size_t records = parameters.profiles.size() + parameters.regions.size() +
        (parameters.upToFace ? parameters.upToFace->origin.extrusionBoundary.size() : 0) +
        (parameters.faceProfile ? parameters.faceProfile->origin.extrusionBoundary.size() : 0);
    for (const auto& region : parameters.regions)
        records += chunkCount(region);
    if (parameters.sweep) // M23: path elements
        records += parameters.sweep->elements.size();
    if (parameters.loft) // M24: each section region and its chunks
        for (const auto& section : parameters.loft->sections)
            records += 1 + chunkCount(section.region);
    return records;
}
void validateNativeExtrusion(const ExtrusionParameters& parameters) {
    require(kernel::modelLength(parameters.distance) &&
        parameters.profiles.size() <= maximumExtrusionProfileCount);
    std::set<std::uint64_t> ids;
    for (const auto id : parameters.profiles)
        require(id.value() != 0 && ids.insert(id.value()).second);
    require(parameters.regions.size() <= maximumExtrusionProfileCount &&
        (parameters.profiles.empty() || parameters.regions.empty()));
    std::set<std::string> regions;
    for (const auto& region : parameters.regions)
        require(regions.insert(region.text()).second);
    require(!parameters.target || parameters.target->value() != 0);
    require(std::isfinite(parameters.offset) &&
        (!parameters.upToFace || parameters.upToFace->body.value() != 0) &&
        (!parameters.faceProfile || parameters.faceProfile->body.value() != 0));
}
void writeNativeExtrusion(const TDF_Label& label, const ExtrusionParameters& parameters) {
    TDataStd_Real::Set(label, parameters.distance);
    const auto profiles = label.FindChild(1);
    TDataStd_Integer::Set(profiles, static_cast<int>(parameters.profiles.size()));
    for (std::size_t index = 0; index < parameters.profiles.size(); ++index)
        TDataStd_AsciiString::Set(profiles.FindChild(static_cast<int>(index + 1)),
            std::to_string(parameters.profiles[index].value()).c_str());
    if (!parameters.regions.empty())
        writeRegions(label.FindChild(2), parameters.regions);
    if (parameters.distanceParameter)
        writeNativeParameterBinding(label.FindChild(3), *parameters.distanceParameter);
    if (parameters.operation)
        TDataStd_Integer::Set(label.FindChild(4), static_cast<int>(*parameters.operation));
    if (parameters.reversed)
        TDataStd_Integer::Set(label.FindChild(5), 1);
    if (parameters.target)
        TDataStd_AsciiString::Set(label.FindChild(6),
            std::to_string(parameters.target->value()).c_str());
    if (parameters.offset != 0.0)
        TDataStd_Real::Set(label.FindChild(7), parameters.offset);
    if (parameters.offsetParameter)
        writeNativeParameterBinding(label.FindChild(8), *parameters.offsetParameter);
    if (parameters.upToFace) {
        const auto face = label.FindChild(9);
        TDataStd_AsciiString::Set(face.FindChild(1),
            std::to_string(parameters.upToFace->body.value()).c_str());
        writeNativeSubshapeOrigin(face, parameters.upToFace->origin);
    }
    if (parameters.symmetric)
        TDataStd_Integer::Set(label.FindChild(11), 1);
    if (parameters.faceProfile) {
        const auto face = label.FindChild(10);
        TDataStd_AsciiString::Set(face.FindChild(1),
            std::to_string(parameters.faceProfile->body.value()).c_str());
        writeNativeSubshapeOrigin(face, parameters.faceProfile->origin);
    }
    // M20 (schema 20): angle on child 12, its axis kind (1 line, 2 base, 3 construction) and
    // value below.
    if (parameters.revolve) {
        const auto revolve = label.FindChild(12);
        const auto& axis = parameters.revolve->axis;
        TDataStd_Real::Set(revolve, parameters.revolve->angle.value());
        TDataStd_Integer::Set(revolve.FindChild(1), axis.sketchLine ? 1 : axis.baseAxis ? 2 : 3);
        const std::uint64_t value = axis.sketchLine   ? axis.sketchLine->value()
                                    : axis.baseAxis ? static_cast<std::uint64_t>(*axis.baseAxis)
                                                    : axis.datumAxis->value();
        TDataStd_AsciiString::Set(revolve.FindChild(2), std::to_string(value).c_str());
        if (parameters.revolve->pitch) // M26 (schema 28)
            TDataStd_Real::Set(revolve.FindChild(3), *parameters.revolve->pitch);
    }
    // M24 (schema 27): child 14 holds ruled (0/1) and each section (1..): its sketch (1) and
    // region (2, as the region list).
    if (parameters.loft) {
        const auto loft = label.FindChild(14);
        TDataStd_Integer::Set(loft, parameters.loft->ruled ? 1 : 0);
        for (std::size_t index = 0; index < parameters.loft->sections.size(); ++index) {
            const auto& section = parameters.loft->sections[index];
            const auto node = loft.FindChild(static_cast<int>(index + 1));
            TDataStd_AsciiString::Set(node.FindChild(1),
                                      std::to_string(section.sketch.value()).c_str());
            writeRegions(node.FindChild(2), {section.region});
        }
    }
    // M23 (schema 26): child 13 holds the path sketch (1) and the path elements (children of 2).
    if (parameters.sweep) {
        const auto sweep = label.FindChild(13);
        TDataStd_AsciiString::Set(sweep.FindChild(1),
                                  std::to_string(parameters.sweep->sketch.value()).c_str());
        const auto elements = sweep.FindChild(2);
        for (std::size_t index = 0; index < parameters.sweep->elements.size(); ++index)
            TDataStd_AsciiString::Set(
                elements.FindChild(static_cast<int>(index + 1)),
                std::to_string(parameters.sweep->elements[index].value()).c_str());
    }
}
ExtrusionParameters readNativeExtrusion(const TDF_Label& label, std::size_t remainingRecords,
                                        int schemaVersion) {
    // Schema 5 (R09) adds the region selection as optional child 2, present only when
    // regions are selected, so documents without regions keep the older encoding.
    // Schema 7 (PAR01) adds the distance parameter binding as optional child 3.
    // Schema 8 (T02) adds operation (4), reversed direction (5) and target body (6).
    const auto regions = label.FindChild(2, false);
    const auto binding = label.FindChild(3, false);
    const auto operation = label.FindChild(4, false);
    const auto reversed = label.FindChild(5, false);
    const auto target = label.FindChild(6, false);
    // Schema 9 (T03) adds start offset (7), its binding (8) and the end face (9).
    const auto offset = label.FindChild(7, false);
    const auto offsetBinding = label.FindChild(8, false);
    const auto endFace = label.FindChild(9, false);
    // Schema 10 (T04) adds the profile face (10) of a push/pull.
    const auto profileFace = label.FindChild(10, false);
    // Schema 13 (M02) adds the symmetric extent (11) and the intersect operation (4 = 4).
    const auto symmetric = label.FindChild(11, false);
    // Schema 20 (M20) adds the revolve (12).
    const auto revolve = label.FindChild(12, false);
    // Schema 26 (M23) adds the sweep path (13).
    const auto sweep = label.FindChild(13, false);
    // Schema 27 (M24) adds the loft (14).
    const auto loft = label.FindChild(14, false);
    const int t02 = (operation.IsNull() ? 0 : 1) + (reversed.IsNull() ? 0 : 1) +
        (target.IsNull() ? 0 : 1);
    const int t03 = (offset.IsNull() ? 0 : 1) + (offsetBinding.IsNull() ? 0 : 1) +
        (endFace.IsNull() ? 0 : 1);
    require((regions.IsNull() || schemaVersion >= 5) && (binding.IsNull() || schemaVersion >= 7) &&
        (t02 == 0 || schemaVersion >= 8) && (t03 == 0 || schemaVersion >= 9) &&
        (profileFace.IsNull() || schemaVersion >= 10) &&
        (symmetric.IsNull() || schemaVersion >= 13) && (revolve.IsNull() || schemaVersion >= 20) &&
        (sweep.IsNull() || schemaVersion >= 26) && (loft.IsNull() || schemaVersion >= 27) &&
        label.NbChildren() == 1 + (regions.IsNull() ? 0 : 1) + (binding.IsNull() ? 0 : 1) + t02 +
                                  t03 + (profileFace.IsNull() ? 0 : 1) +
                                  (symmetric.IsNull() ? 0 : 1) + (revolve.IsNull() ? 0 : 1) +
                                  (sweep.IsNull() ? 0 : 1) + (loft.IsNull() ? 0 : 1));
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
    if (!regions.IsNull()) {
        remainingRecords -= static_cast<std::size_t>(count);
        result.regions = readRegions(regions, remainingRecords);
        require(!result.regions.empty());
    }
    if (!binding.IsNull())
        result.distanceParameter = readNativeParameterBinding(binding);
    if (!operation.IsNull()) {
        const int value = readInteger(operation);
        require(value >= static_cast<int>(ExtrusionOperation::newBody) &&
            value <= static_cast<int>(schemaVersion >= 13 ? ExtrusionOperation::intersect
                                                          : ExtrusionOperation::cut));
        result.operation = static_cast<ExtrusionOperation>(value);
    }
    if (!reversed.IsNull()) {
        require(readInteger(reversed) == 1); // Written only when set.
        result.reversed = true;
    }
    if (!symmetric.IsNull()) {
        require(symmetric.NbChildren() == 0 && readInteger(symmetric) == 1);
        result.symmetric = true;
    }
    if (!target.IsNull())
        result.target = BodyId{readDecimal(target)};
    if (!offset.IsNull()) {
        Handle(TDataStd_Real) value;
        require(offset.NbChildren() == 0 && offset.FindAttribute(TDataStd_Real::GetID(), value) &&
            value->Get() != 0.0); // Written only when set.
        result.offset = value->Get();
    }
    if (!offsetBinding.IsNull())
        result.offsetParameter = readNativeParameterBinding(offsetBinding);
    if (!endFace.IsNull()) {
        require(endFace.NbChildren() == 4 + nativeSubshapeOriginExtension(endFace));
        const BodyId body{readDecimal(child(endFace, 1))};
        result.upToFace = BodyFaceReference{
            body, readNativeSubshapeOrigin(endFace, remainingRecords, schemaVersion)};
    }
    if (!profileFace.IsNull()) {
        require(profileFace.NbChildren() == 4 + nativeSubshapeOriginExtension(profileFace));
        const BodyId body{readDecimal(child(profileFace, 1))};
        result.faceProfile = BodyFaceReference{
            body, readNativeSubshapeOrigin(profileFace, remainingRecords, schemaVersion)};
    }
    if (!revolve.IsNull()) {
        Handle(TDataStd_Real) angle;
        const auto pitch = revolve.FindChild(3, false);
        require(revolve.NbChildren() == (pitch.IsNull() ? 2 : 3) &&
                (pitch.IsNull() || schemaVersion >= 28) &&
                revolve.FindAttribute(TDataStd_Real::GetID(), angle));
        const int kind = readInteger(child(revolve, 1));
        const std::uint64_t value = readDecimal(child(revolve, 2));
        require(kind >= 1 && kind <= 3 && (kind != 2 || value <= 3));
        RevolveAxis axis;
        if (kind == 1)
            axis.sketchLine = SketchElementId{value};
        else if (kind == 2)
            axis.baseAxis = static_cast<BaseAxis>(value);
        else
            axis.datumAxis = FeatureId{value};
        result.revolve = RevolveParameters{axis, kernel::Radians{angle->Get()}};
        if (!pitch.IsNull()) {
            Handle(TDataStd_Real) stored;
            require(pitch.NbChildren() == 0 &&
                    pitch.FindAttribute(TDataStd_Real::GetID(), stored));
            result.revolve->pitch = stored->Get();
        }
        validateRevolveParameters(*result.revolve);
    }
    if (!sweep.IsNull()) {
        require(sweep.NbChildren() == 2);
        const auto elements = child(sweep, 2);
        const int pathLength = elements.NbChildren();
        require(pathLength >= 1 &&
                static_cast<std::size_t>(pathLength) <= maximumSweepPathElements &&
                static_cast<std::size_t>(pathLength) <= remainingRecords);
        SweepPath path{FeatureId{readDecimal(child(sweep, 1))}, {}};
        for (int index = 1; index <= pathLength; ++index)
            path.elements.push_back(SketchElementId{readDecimal(child(elements, index))});
        validateSweepPath(path);
        result.sweep = std::move(path);
    }
    if (!loft.IsNull()) {
        const int ruled = [&] {
            Handle(TDataStd_Integer) value;
            require(loft.FindAttribute(TDataStd_Integer::GetID(), value));
            return value->Get();
        }();
        const int sections = loft.NbChildren();
        require((ruled == 0 || ruled == 1) && sections >= 1 &&
                static_cast<std::size_t>(sections) <= maximumLoftSections);
        LoftParameters parameters{{}, ruled == 1};
        for (int index = 1; index <= sections; ++index) {
            const auto node = child(loft, index);
            require(node.NbChildren() == 2);
            auto regionList = readRegions(child(node, 2), remainingRecords);
            require(regionList.size() == 1);
            parameters.sections.push_back(
                {FeatureId{readDecimal(child(node, 1))}, std::move(regionList.front())});
        }
        validateLoftParameters(parameters);
        result.loft = std::move(parameters);
    }
    validateNativeExtrusion(result);
    return result;
}
}
