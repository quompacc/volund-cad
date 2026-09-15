#pragma once
#include <voelund/document/DocumentId.hpp>
#include <optional>

namespace voelund::document::detail {
// Private seam for deterministic entropy-failure and collision tests.
template<class NextBytes>
DocumentId generateDocumentIdUsing(NextBytes nextBytes,
                                   std::optional<DocumentId> excluded = std::nullopt) {
    for (unsigned attempt = 0; attempt < 16; ++attempt) {
        const auto bytes = nextBytes();
        std::uint64_t high = 0;
        std::uint64_t low = 0;
        for (unsigned index = 0; index < 8; ++index) {
            high = (high << 8) | bytes[index];
            low = (low << 8) | bytes[index + 8];
        }
        if (high == 0 && low == 0)
            continue;
        const DocumentId id{high, low};
        if (!excluded || id != *excluded)
            return id;
    }
    throw std::runtime_error("Document identity generation exhausted valid candidates");
}
} // namespace voelund::document::detail
