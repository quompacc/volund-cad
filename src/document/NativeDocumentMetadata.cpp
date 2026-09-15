#include "NativeDocumentMetadata.hpp"
#include <voelund/document/IdCapacity.hpp>
#include <TDataStd_AsciiString.hxx>
#include <TDataStd_Integer.hxx>
#include <charconv>
#include <limits>

namespace voelund::document::detail {
namespace {
constexpr const char* marker = "VoelundCAD.Native";
// Private Main() children: marker, application schema, document ID, allocation watermark.
enum Slot { formatSlot = 1, schemaSlot = 2, identitySlot = 3, watermarkSlot = 4 };
TDF_Label root(const Handle(TDocStd_Document)& document) {
    if (document.IsNull())
        throw std::invalid_argument("Missing native document");
    return document->Main();
}
std::string text(const TDF_Label& main, int slot) {
    const auto label = main.FindChild(slot, false);
    Handle(TDataStd_AsciiString) value;
    if (label.IsNull() || !label.FindAttribute(TDataStd_AsciiString::GetID(), value))
        throw std::invalid_argument("Missing native metadata string");
    const auto& native = value->Get();
    return {native.ToCString(), static_cast<std::size_t>(native.Length())};
}
void validateWatermark(std::uint64_t value) {
    if (!hasIdCapacity(value, 0))
        throw std::invalid_argument("Invalid native feature ID watermark");
}
} // namespace
void initializeNativeMetadata(const Handle(TDocStd_Document)& document,
                              const NativeDocumentMetadata& metadata) {
    validateWatermark(metadata.nextFeatureId);
    const auto main = root(document);
    if (main.HasChild() || main.NbAttributes() != 0)
        throw std::invalid_argument("Native metadata requires a fresh document root");
    const auto id = documentIdToHex(metadata.id);
    const auto next = std::to_string(metadata.nextFeatureId);
    TDataStd_AsciiString::Set(main.FindChild(formatSlot), marker);
    TDataStd_Integer::Set(main.FindChild(schemaSlot), nativeSchemaVersion);
    TDataStd_AsciiString::Set(main.FindChild(identitySlot), id.c_str());
    TDataStd_AsciiString::Set(main.FindChild(watermarkSlot), next.c_str());
}
NativeDocumentMetadata readNativeMetadata(const Handle(TDocStd_Document)& document) {
    const auto main = root(document);
    if (text(main, formatSlot) != marker)
        throw std::invalid_argument("Unknown native document marker");
    const auto schemaLabel = main.FindChild(schemaSlot, false);
    Handle(TDataStd_Integer) schema;
    if (schemaLabel.IsNull() ||
        !schemaLabel.FindAttribute(TDataStd_Integer::GetID(), schema) ||
        schema->Get() != nativeSchemaVersion)
        throw std::invalid_argument("Unsupported native application schema");
    const auto id = documentIdFromHex(text(main, identitySlot));
    const auto encoded = text(main, watermarkSlot);
    std::uint64_t next = 0;
    const auto result = std::from_chars(encoded.data(), encoded.data() + encoded.size(), next);
    if (result.ec != std::errc{} || result.ptr != encoded.data() + encoded.size() ||
        encoded != std::to_string(next))
        throw std::invalid_argument("Invalid native feature ID watermark encoding");
    validateWatermark(next);
    return {id, next};
}
} // namespace voelund::document::detail
