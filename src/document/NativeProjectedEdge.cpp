#include "NativeProjectedEdge.hpp"
#include <TDataStd_AsciiString.hxx>
#include <TDataStd_Integer.hxx>
#include <TDataStd_Real.hxx>
#include <charconv>
#include <stdexcept>
namespace voelund::document::detail {
namespace {
void require(bool ok) {
    if (!ok)
        throw std::invalid_argument("Invalid native projected edge");
}
TDF_Label child(const TDF_Label& parent, int tag) {
    auto label = parent.FindChild(tag, false);
    require(!label.IsNull() && label.NbChildren() == 0);
    return label;
}
std::string text(const TDF_Label& label) {
    Handle(TDataStd_AsciiString) value;
    require(label.FindAttribute(TDataStd_AsciiString::GetID(), value) &&
            value->Get().Length() <= 32);
    return value->Get().ToCString();
}
void number(const TDF_Label& label, std::uint64_t value) {
    TDataStd_AsciiString::Set(label, std::to_string(value).c_str());
}
std::uint64_t number(const TDF_Label& label) {
    const auto encoded = text(label);
    std::uint64_t value{};
    const auto result = std::from_chars(encoded.data(), encoded.data() + encoded.size(), value);
    require(result.ec == std::errc{} && result.ptr == encoded.data() + encoded.size() &&
            encoded == std::to_string(value));
    return value;
}
int integer(const TDF_Label& label) {
    Handle(TDataStd_Integer) value;
    require(label.FindAttribute(TDataStd_Integer::GetID(), value));
    return value->Get();
}
} // namespace
void writeNativeProjectedEdge(const TDF_Label& label, const ProjectedEdgeReference& ref) {
    validateProjectedEdgeReference(ref);
    TDataStd_AsciiString::Set(label, documentIdToHex(ref.document).c_str());
    number(label.FindChild(1), ref.body.value());
    if (const auto* primitive = std::get_if<PrimitiveEdgeOrigin>(&ref.origin.value)) {
        TDataStd_Integer::Set(label.FindChild(2), 1);
        number(label.FindChild(3), primitive->feature.value());
        TDataStd_Integer::Set(label.FindChild(4), static_cast<int>(primitive->first));
        TDataStd_Integer::Set(label.FindChild(5), static_cast<int>(primitive->second));
    } else {
        const auto& contour = std::get<ContourEdgeOrigin>(ref.origin.value);
        TDataStd_Integer::Set(label.FindChild(2), 2);
        number(label.FindChild(3), contour.sketch.value());
        number(label.FindChild(4), contour.element.value());
        TDataStd_Integer::Set(label.FindChild(5), static_cast<int>(contour.role));
        TDataStd_Integer::Set(label.FindChild(6), static_cast<int>(contour.stage));
        number(label.FindChild(7), contour.extrusion ? contour.extrusion->value() : 0);
    }
    TDataStd_Integer::Set(label.FindChild(8), static_cast<int>(ref.selection.kind));
    TDataStd_Real::Set(label.FindChild(9), ref.selection.parameter);
}
ProjectedEdgeReference readNativeProjectedEdge(const TDF_Label& label) {
    const int kind = integer(child(label, 2));
    require(kind == 1 || kind == 2);
    require(label.NbChildren() == (kind == 1 ? 7 : 9));
    const auto feature = FeatureId{number(child(label, 3))};
    EdgeOrigin origin{feature, kernel::PrimitiveRole::boxNegativeX,
                      kernel::PrimitiveRole::boxNegativeY};
    if (kind == 1) {
        origin = PrimitiveEdgeOrigin{feature,
                                     static_cast<kernel::PrimitiveRole>(integer(child(label, 4))),
                                     static_cast<kernel::PrimitiveRole>(integer(child(label, 5)))};
    } else {
        const auto extrusion = number(child(label, 7));
        origin = ContourEdgeOrigin{feature, SketchElementId{number(child(label, 4))},
                                   static_cast<ContourRole>(integer(child(label, 5))),
                                   extrusion ? std::optional{FeatureId{extrusion}} : std::nullopt,
                                   static_cast<ContourStage>(integer(child(label, 6)))};
    }
    Handle(TDataStd_Real) parameter;
    require(child(label, 9).FindAttribute(TDataStd_Real::GetID(), parameter));
    ProjectedEdgeReference result{
        documentIdFromHex(text(label)),
        BodyId{number(child(label, 1))},
        origin,
        {static_cast<EdgePointKind>(integer(child(label, 8))), parameter->Get()}};
    validateProjectedEdgeReference(result);
    return result;
}
} // namespace voelund::document::detail
