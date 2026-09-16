#pragma once

#include <iosfwd>
#include <optional>
#include <voelund/document/LegacyDocumentImporter.hpp>

namespace voelund::document {

class ModelDocument;
// Native files retain their persisted identity. Legacy imports receive a fresh ID.
// Only legacy input has a migration report; native schema validation is not migration.
struct DocumentReadResult final {
    DocumentSnapshot snapshot;
    DocumentId id;
    std::optional<MigrationReport> report;
    [[nodiscard]] ModelDocument toDocument() const { return snapshot.toDocument(id); }
};

class DocumentSerializer final {
public:
    static void write(const ModelDocument& document, std::ostream& output);
    [[nodiscard]] static ModelDocument read(std::istream& input);
    [[nodiscard]] static DocumentReadResult readWithReport(std::istream& input);
};

} // namespace voelund::document
