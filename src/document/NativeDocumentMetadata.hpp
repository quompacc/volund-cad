#pragma once
#include <TDocStd_Document.hxx>
#include <voelund/document/DocumentId.hpp>

namespace voelund::document::detail {
// Candidate native application schema. Not the OCAF container version or VOLUND 17.
inline constexpr int nativeSchemaVersion = 21;
struct NativeDocumentMetadata final {
    DocumentId id;
    std::uint64_t nextFeatureId;
    int schemaVersion{nativeSchemaVersion};
    bool operator==(const NativeDocumentMetadata&) const = default;
};
// Initializes a fresh root only. Existing roots, including malformed ones, are rejected.
void initializeNativeMetadata(const Handle(TDocStd_Document) & document,
                              const NativeDocumentMetadata& metadata);
// Read-only validation; never creates labels, repairs data or generates an identity.
[[nodiscard]] NativeDocumentMetadata readNativeMetadata(const Handle(TDocStd_Document) & document);
} // namespace voelund::document::detail
