#pragma once

#include <iosfwd>
#include <voelund/document/LegacyDocumentImporter.hpp>

namespace voelund::document {

class ModelDocument;

class DocumentSerializer final {
public:
    static void write(const ModelDocument& document, std::ostream& output);
    [[nodiscard]] static ModelDocument read(std::istream& input);
    [[nodiscard]] static ImportedDocument readWithReport(std::istream& input);
};

} // namespace voelund::document
