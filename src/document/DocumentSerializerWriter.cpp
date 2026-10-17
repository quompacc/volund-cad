#include "NativeDocumentStore.hpp"
#include <ostream>
#include <stdexcept>
#include <voelund/document/DocumentSerializer.hpp>
#include <voelund/document/IdentifiedDocumentSnapshot.hpp>

namespace voelund::document {
void DocumentSerializer::write(const ModelDocument& document, std::ostream& output) {
    const auto snapshot = IdentifiedDocumentSnapshot::restore(document.documentId(),
                                                              DocumentSnapshot::capture(document));
    const auto bytes = detail::NativeDocumentStore::encode(snapshot);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!output)
        throw std::runtime_error("Failed to write Völund document");
}
} // namespace voelund::document
