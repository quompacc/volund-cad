#include "BoundedDocumentInput.hpp"
#include "DocumentFormat.hpp"
#include <locale>
#include <voelund/document/DocumentSerializer.hpp>

namespace voelund::document {
ImportedDocument DocumentSerializer::readWithReport(std::istream& source) {
    detail::BoundedDocumentInput input(source);
    input.imbue(std::locale::classic());
    const auto format = detail::readSourceFormat(input);
    switch (format.encoding) {
    case DocumentEncoding::legacyText:
        return LegacyDocumentImporter::readPayload(input, format);
    }
    detail::invalidDocument("unsupported document encoding");
}
ModelDocument DocumentSerializer::read(std::istream& input) {
    return readWithReport(input).snapshot.toDocument();
}
} // namespace voelund::document
