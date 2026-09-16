#include <voelund/application/CadSession.hpp>
#include <limits>
#include <type_traits>

namespace voelund::application {
document::IdentifiedDocumentSnapshot CadSession::identifiedSnapshot() const {
    return document::IdentifiedDocumentSnapshot::restore(documentId_,
        document::DocumentSnapshot::capture(document_));
}
void CadSession::duplicateDocument() {
    if (generation_ == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("Document generation exhausted");
    const auto snapshot = identifiedSnapshot().duplicate();
    auto candidate = snapshot.snapshot().toDocument();
    const bool geometryCurrent = geometryStamp_ == std::optional<DocumentStamp>{sceneStatus().document};
    static_assert(std::is_nothrow_move_assignable_v<document::ModelDocument>);
    // All throwing preparation precedes publication. Derived geometry and selection
    // still describe the same content; only its namespace and transient history change.
    document_ = std::move(candidate);
    documentId_ = snapshot.id();
    ++generation_;
    geometryStamp_ = geometryCurrent ? std::optional<DocumentStamp>{sceneStatus().document} : std::nullopt;
    currentFile_.clear();
    cleanDocumentState_.reset();
    importReport_.reset();
}
} // namespace voelund::application
