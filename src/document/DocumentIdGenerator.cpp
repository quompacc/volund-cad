#include "DocumentIdGeneration.hpp"
#include <voelund/platform/RandomBytes.hpp>

namespace voelund::document {
DocumentId generateDocumentId() {
    return detail::generateDocumentIdUsing(platform::randomBytes128);
}
} // namespace voelund::document
