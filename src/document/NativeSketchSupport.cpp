#include "NativeSketchSupport.hpp"
#include "NativeProjectedEdge.hpp"
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
constexpr int faceExtrusionRole = -2;
constexpr int treatedEdgeRole = -3; // schema 17: a fillet or chamfer face

void writeNativeSubshapeOrigin(const TDF_Label& label, const SubshapeOrigin& origin) {
    validateSubshapeOrigin(origin);
    TDataStd_AsciiString::Set(label.FindChild(2), std::to_string(origin.feature.value()).c_str());
    TDataStd_Integer::Set(label.FindChild(3), origin.faceExtrusion ? faceExtrusionRole
                                              : origin.treatedEdge ? treatedEdgeRole
                                              : origin.role ? static_cast<int>(*origin.role)
                                                            : -1);
    const auto boundary = label.FindChild(4);
    TDataStd_Integer::Set(boundary, static_cast<int>(origin.extrusionBoundary.size()));
    int index = 0;
    for (const auto id : origin.extrusionBoundary)
        TDataStd_AsciiString::Set(boundary.FindChild(++index), std::to_string(id.value()).c_str());
    if (origin.treatedEdge)
        TDataStd_AsciiString::Set(label.FindChild(6).FindChild(1),
                                  std::to_string(*origin.treatedEdge).c_str());
    if (origin.extrusionFace != ExtrusionFace::end) {
        const auto extension = label.FindChild(6);
        TDataStd_Integer::Set(extension.FindChild(1), static_cast<int>(origin.extrusionFace));
        if (origin.sweptEdge)
            writeNativeEdgeOrigin(extension.FindChild(2), *origin.sweptEdge);
    }
    // M09 (schema 31): the copies of a mirrored face, each its feature and copy index.
    if (!origin.instances.empty()) {
        const auto instances = label.FindChild(7);
        TDataStd_Integer::Set(instances, static_cast<int>(origin.instances.size()));
        int position = 0;
        for (const auto& instance : origin.instances) {
            const auto item = instances.FindChild(++position);
            TDataStd_AsciiString::Set(item, std::to_string(instance.feature.value()).c_str());
            TDataStd_Integer::Set(item, static_cast<int>(instance.index));
        }
    }
}
int nativeSubshapeOriginExtension(const TDF_Label& label) {
    return (label.FindChild(6, false).IsNull() ? 0 : 1) +
           (label.FindChild(7, false).IsNull() ? 0 : 1);
}
SubshapeOrigin readNativeSubshapeOrigin(const TDF_Label& label, std::size_t& remainingElements,
                                        int schemaVersion) {
    SubshapeOrigin origin{FeatureId{number(child(label, 2))}, std::nullopt};
    const int role = integer(child(label, 3));
    require((role >= -1 || (role == faceExtrusionRole && schemaVersion >= 10) ||
             (role == treatedEdgeRole && schemaVersion >= 17)) &&
            (role <= static_cast<int>(kernel::PrimitiveRole::cylinderSide) ||
             (role <= static_cast<int>(kernel::PrimitiveRole::torusSurface) &&
              schemaVersion >= 25))); // M27: sphere and torus surfaces
    if (role >= 0)
        origin.role = static_cast<kernel::PrimitiveRole>(role);
    origin.faceExtrusion = role == faceExtrusionRole;
    const auto boundary = child(label, 4);
    const int count = integer(boundary);
    require(count >= 0 && static_cast<std::size_t>(count) <= remainingElements &&
            boundary.NbChildren() == count);
    remainingElements -= static_cast<std::size_t>(count);
    for (int i = 1; i <= count; ++i)
        origin.extrusionBoundary.emplace_back(number(child(boundary, i)));
    if (role == treatedEdgeRole) {
        const auto extension = label.FindChild(6, false);
        require(!extension.IsNull() && extension.NbChildren() == 1);
        origin.treatedEdge = number(child(extension, 1));
    } else if (const auto extension = label.FindChild(6, false); !extension.IsNull()) {
        require(schemaVersion >= 10);
        const int face = integer(child(extension, 1));
        require(face == static_cast<int>(ExtrusionFace::start) ||
                face == static_cast<int>(ExtrusionFace::side));
        origin.extrusionFace = static_cast<ExtrusionFace>(face);
        if (origin.extrusionFace == ExtrusionFace::side) {
            const auto edge = extension.FindChild(2, false);
            require(!edge.IsNull() && extension.NbChildren() == 2);
            origin.sweptEdge = readNativeEdgeOrigin(edge);
            require(edge.NbChildren() ==
                    (std::holds_alternative<PrimitiveEdgeOrigin>(origin.sweptEdge->value) ? 4 : 6));
        } else {
            require(extension.NbChildren() == 1);
        }
    }
    if (const auto instances = label.FindChild(7, false); !instances.IsNull()) {
        const int copies = integer(instances);
        require(schemaVersion >= 31 && copies > 0 &&
                static_cast<std::size_t>(copies) <= maximumSubshapeInstances &&
                instances.NbChildren() == copies);
        for (int i = 1; i <= copies; ++i) {
            const auto item = child(instances, i);
            const int index = integer(item);
            require(item.NbChildren() == 0 && index > 0);
            origin.instances.push_back(
                {FeatureId{number(item)}, static_cast<std::uint32_t>(index)});
        }
    }
    validateSubshapeOrigin(origin);
    return origin;
}
void writeNativeSketchSupport(const TDF_Label& label, const SketchSupportBinding& binding) {
    const auto& ref = binding.reference;
    validateCanonicalSketchPlane(binding.localPlane);
    TDataStd_AsciiString::Set(label, documentIdToHex(ref.document).c_str());
    TDataStd_AsciiString::Set(label.FindChild(1), std::to_string(ref.body.value()).c_str());
    writeNativeSubshapeOrigin(label, ref.origin);
    const auto& p = binding.localPlane;
    const double values[] = {p.originX, p.originY,     p.originZ,     p.normalX,    p.normalY,
                             p.normalZ, p.xDirectionX, p.xDirectionY, p.xDirectionZ};
    const auto data = TDataStd_RealArray::Set(label.FindChild(5), 1, 9);
    for (int i = 1; i <= 9; ++i)
        data->SetValue(i, values[i - 1]);
}
SketchSupportBinding readNativeSketchSupport(const TDF_Label& label,
                                             std::size_t remainingElements, int schemaVersion) {
    require(label.NbChildren() == 5 + nativeSubshapeOriginExtension(label));
    const auto origin = readNativeSubshapeOrigin(label, remainingElements, schemaVersion);
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
