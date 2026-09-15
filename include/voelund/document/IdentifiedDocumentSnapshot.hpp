#pragma once
#include <voelund/document/DocumentId.hpp>
#include <voelund/document/DocumentSnapshot.hpp>

namespace voelund::document {
// Immutable native-store boundary value, not a second mutable document backend.
// Copying this value preserves identity. Only duplicate() creates a new namespace.
class IdentifiedDocumentSnapshot final {
  public:
    [[nodiscard]] static IdentifiedDocumentSnapshot create(DocumentSnapshot snapshot);
    [[nodiscard]] static IdentifiedDocumentSnapshot restore(DocumentId id,
                                                            DocumentSnapshot snapshot);
    [[nodiscard]] DocumentId id() const noexcept { return id_; }
    [[nodiscard]] const DocumentSnapshot& snapshot() const noexcept { return snapshot_; }
    [[nodiscard]] IdentifiedDocumentSnapshot revised(DocumentSnapshot snapshot) const;
    [[nodiscard]] IdentifiedDocumentSnapshot duplicate() const;
    [[nodiscard]] bool operator==(const IdentifiedDocumentSnapshot&) const = default;

  private:
    IdentifiedDocumentSnapshot(DocumentId id, DocumentSnapshot snapshot)
        : id_(id), snapshot_(std::move(snapshot)) {}
    DocumentId id_;
    DocumentSnapshot snapshot_;
};
} // namespace voelund::document
