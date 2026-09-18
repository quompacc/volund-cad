#include "DocumentIdGeneration.hpp"
#include <voelund/document/IdentifiedDocumentSnapshot.hpp>
#include <voelund/platform/RandomBytes.hpp>

namespace voelund::document {
IdentifiedDocumentSnapshot IdentifiedDocumentSnapshot::create(DocumentSnapshot snapshot) {
    return {generateDocumentId(), std::move(snapshot)};
}
IdentifiedDocumentSnapshot IdentifiedDocumentSnapshot::restore(DocumentId id,
                                                               DocumentSnapshot snapshot) {
    return {id, std::move(snapshot)};
}
IdentifiedDocumentSnapshot IdentifiedDocumentSnapshot::revised(DocumentSnapshot snapshot) const {
    if (snapshot.nextFeatureId() < snapshot_.nextFeatureId())
        throw std::invalid_argument("Document revision cannot lower its feature ID watermark");
    return {id_, std::move(snapshot)};
}
IdentifiedDocumentSnapshot IdentifiedDocumentSnapshot::duplicate() const {
    const auto next = detail::generateDocumentIdUsing(platform::randomBytes128, id_);
    return {next, snapshot_.remapSupportNamespace(id_, next)};
}
} // namespace voelund::document
