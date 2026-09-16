#pragma once
#include <memory>
#include <voelund/document/ModelDocument.hpp>

namespace voelund::document {
// Single-threaded isolated editing. The target must outlive this object and must
// not be moved/reassigned while a transaction exists. Concurrent commits are rejected.
class DocumentTransaction final {
  public:
    explicit DocumentTransaction(ModelDocument& target);
    ~DocumentTransaction();
    DocumentTransaction(const DocumentTransaction&) = delete;
    DocumentTransaction& operator=(const DocumentTransaction&) = delete;
    DocumentTransaction(DocumentTransaction&&) = delete;
    DocumentTransaction& operator=(DocumentTransaction&&) = delete;
    [[nodiscard]] ModelDocument& edit();
    [[nodiscard]] const ModelDocument& candidate() const noexcept { return candidate_; }
    // All allocation precedes publication. Returns false for an unchanged candidate.
    [[nodiscard]] bool prepare();
    [[nodiscard]] bool commit();
    void rollback() noexcept;

  private:
    void checkOpenAndCurrent() const;
    ModelDocument& target_;
    ModelDocument candidate_;
    std::uint64_t baseRevision_;
    std::uint64_t preparedRevision_{0};
    std::unique_ptr<ModelDocument::State> before_;
    std::unique_ptr<ModelDocument::State> after_;
    bool prepared_{false};
    bool finished_{false};
};
} // namespace voelund::document