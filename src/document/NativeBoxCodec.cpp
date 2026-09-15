#include "NativeBoxCodec.hpp"
#include "NativeDocumentMetadata.hpp"
#include "DocumentSerializationDetail.hpp"
#include <voelund/document/IdCapacity.hpp>
#include <voelund/kernel/NumericPolicy.hpp>
#include <TDataStd_AsciiString.hxx>
#include <TDataStd_Integer.hxx>
#include <TDataStd_RealArray.hxx>
#include <array>
#include <charconv>
#include <set>

namespace voelund::document::detail {
namespace {
void require(bool condition) {
    if (!condition) throw std::invalid_argument("Invalid or unsupported native box data");
}
TDF_Label child(const TDF_Label& parent, int tag) {
    require(!parent.IsNull());
    const auto label = parent.FindChild(tag, false);
    require(!label.IsNull());
    return label;
}
std::string text(const TDF_Label& label) {
    Handle(TDataStd_AsciiString) value;
    require(label.FindAttribute(TDataStd_AsciiString::GetID(), value));
    const auto& string = value->Get();
    require(string.Length() <= static_cast<int>(maximumNameBytes));
    return {string.ToCString(), static_cast<std::size_t>(string.Length())};
}
std::uint64_t id(const TDF_Label& label) {
    const auto encoded = text(label);
    std::uint64_t value = 0;
    const auto parsed = std::from_chars(encoded.data(), encoded.data() + encoded.size(), value);
    require(parsed.ec == std::errc{} && parsed.ptr == encoded.data() + encoded.size() &&
            encoded == std::to_string(value) && hasIdCapacity(value, 1));
    return value;
}
std::array<double, 9> values(const BoxParameters& box) {
    return {box.dimensions.width, box.dimensions.depth, box.dimensions.height,
            box.placement.translationX, box.placement.translationY, box.placement.translationZ,
            box.placement.rotationX.value(), box.placement.rotationY.value(),
            box.placement.rotationZ.value()};
}
void validate(const FeatureDescriptor& feature, const FeatureParameters& parameters,
              std::uint64_t next, std::set<std::uint64_t>& used) {
    require(feature.kind == FeatureKind::box && !feature.inputFeature &&
            std::holds_alternative<BoxParameters>(parameters) &&
            hasIdCapacity(feature.id.value(), 1) && feature.id.value() < next &&
            used.insert(feature.id.value()).second && feature.name.size() <= maximumNameBytes &&
            feature.name.find('\0') == std::string::npos);
    const auto data = values(std::get<BoxParameters>(parameters));
    for (unsigned index = 0; index < data.size(); ++index) {
        require(std::isfinite(data[index]));
        if (index < 3) require(kernel::modelLength(data[index]));
        else if (index < 6) require(kernel::modelCoordinate(data[index]));
    }
}
} // namespace
void NativeBoxCodec::write(const Handle(TDocStd_Document)& document,
                           const IdentifiedDocumentSnapshot& identified) {
    const auto& snapshot = identified.snapshot();
    require(snapshot.features().size() == snapshot.parameters().size() &&
            snapshot.features().size() <= maximumFeatureCount);
    std::set<std::uint64_t> used;
    for (std::size_t index = 0; index < snapshot.features().size(); ++index)
        validate(snapshot.features()[index], snapshot.parameters()[index], snapshot.nextFeatureId(), used);
    // Reject all unsupported feature families before writing even the header.
    initializeNativeMetadata(document, {identified.id(), snapshot.nextFeatureId()});
    const auto bodies = document->Main().FindChild(10);
    TDataStd_AsciiString::Set(bodies, "Boxes.v1");
    TDataStd_Integer::Set(bodies, static_cast<int>(snapshot.features().size()));
    for (std::size_t index = 0; index < snapshot.features().size(); ++index) {
        const auto& feature = snapshot.features()[index];
        const auto body = bodies.FindChild(static_cast<int>(index + 1));
        const auto nativeFeature = body.FindChild(10);
        // D11c box-origin identity, stored explicitly and checked on read.
        const auto encoded = std::to_string(feature.id.value());
        TDataStd_AsciiString::Set(body.FindChild(1), encoded.c_str());
        TDataStd_AsciiString::Set(nativeFeature.FindChild(1), "box");
        TDataStd_AsciiString::Set(nativeFeature.FindChild(2), encoded.c_str());
        TDataStd_AsciiString::Set(nativeFeature.FindChild(3), feature.name.c_str());
        const auto data = values(std::get<BoxParameters>(snapshot.parameters()[index]));
        auto array = TDataStd_RealArray::Set(nativeFeature.FindChild(4), 1, 9);
        for (int item = 1; item <= 9; ++item)
            array->SetValue(item, data[static_cast<std::size_t>(item - 1)]);
    }
}
IdentifiedDocumentSnapshot NativeBoxCodec::read(const Handle(TDocStd_Document)& document) {
    const auto metadata = readNativeMetadata(document);
    // Header children 1..4 plus body area 10; no silently skipped feature areas.
    require(document->Main().NbChildren() == 5);
    const auto bodies = child(document->Main(), 10);
    require(text(bodies) == "Boxes.v1");
    Handle(TDataStd_Integer) count;
    require(bodies.FindAttribute(TDataStd_Integer::GetID(), count));
    require(count->Get() >= 0 && count->Get() <= static_cast<int>(maximumFeatureCount) &&
            bodies.NbChildren() == count->Get());
    std::vector<FeatureDescriptor> features;
    std::vector<FeatureParameters> parameters;
    std::set<std::uint64_t> used;
    for (int index = 1; index <= count->Get(); ++index) {
        const auto body = child(bodies, index);
        require(body.NbChildren() == 2);
        const auto nativeFeature = child(body, 10);
        require(nativeFeature.NbChildren() == 4 && text(child(nativeFeature, 1)) == "box");
        const auto featureId = id(child(nativeFeature, 2));
        require(id(child(body, 1)) == featureId);
        FeatureDescriptor feature{FeatureId{featureId}, FeatureKind::box,
                                  text(child(nativeFeature, 3)), std::nullopt};
        Handle(TDataStd_RealArray) data;
        require(child(nativeFeature, 4).FindAttribute(TDataStd_RealArray::GetID(), data));
        require(data->Lower() == 1 && data->Upper() == 9);
        const BoxParameters box{{data->Value(1), data->Value(2), data->Value(3)},
            {data->Value(4), data->Value(5), data->Value(6), kernel::Radians{data->Value(7)},
             kernel::Radians{data->Value(8)}, kernel::Radians{data->Value(9)}}};
        validate(feature, box, metadata.nextFeatureId, used);
        features.push_back(std::move(feature));
        parameters.emplace_back(box);
    }
    return IdentifiedDocumentSnapshot::restore(metadata.id,
        DocumentSnapshot{std::move(features), std::move(parameters), metadata.nextFeatureId});
}
} // namespace voelund::document::detail
