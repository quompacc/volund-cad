#include "DocumentFileInput.hpp"
#include "NativeDocumentStore.hpp"
#include <sstream>
#include <voelund/document/DocumentSerializer.hpp>

namespace voelund::document {
DocumentReadResult DocumentSerializer::readWithReport(std::istream& source) {
    auto input = detail::DocumentFileInput::read(source);
    if (input.kind == detail::DocumentFileKind::nativeBinary) {
        const auto native = detail::NativeDocumentStore::decode(input.bytes);
        return {native.snapshot(), native.id(), std::nullopt};
    }
    std::istringstream legacy(input.bytes);
    auto imported = LegacyDocumentImporter::read(legacy);
    return {std::move(imported.snapshot), generateDocumentId(), std::move(imported.report)};
}
ModelDocument DocumentSerializer::read(std::istream& input) {
    return readWithReport(input).toDocument();
}
} // namespace voelund::document
