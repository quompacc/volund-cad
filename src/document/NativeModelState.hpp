#pragma once
#include "NativePrimitiveCodec.hpp"
#include <memory>
#include <vector>
#include <utility>

namespace voelund::document::detail {
// Private OCAF authority; immutable read views are derived before publication.
// No mutable native handle escapes. Prepared changes require the target to outlive them.
class NativeModelState final {
    friend struct NativeModelStateTestAccess;
    struct State {
        Handle(TDocStd_Document) current;
        std::shared_ptr<const IdentifiedDocumentSnapshot> view;
        std::vector<Handle(TDocStd_Document)> undo;
        std::vector<Handle(TDocStd_Document)> redo;
        std::uint64_t revision{0};
    };
  public:
    class Prepared final {
      public:
        Prepared(Prepared&& other) noexcept
            : owner_(other.owner_), revision_(other.revision_), state_(std::move(other.state_)),
              consumed_(std::exchange(other.consumed_, true)) {}
        Prepared& operator=(Prepared&&) = delete;
        Prepared(const Prepared&) = delete;
        Prepared& operator=(const Prepared&) = delete;
      private:
        friend class NativeModelState;
        Prepared(const NativeModelState* owner, std::uint64_t revision,
                 std::unique_ptr<State> state)
            : owner_(owner), revision_(revision), state_(std::move(state)) {}
        const NativeModelState* owner_;
        std::uint64_t revision_;
        std::unique_ptr<State> state_;
        bool consumed_{false};
    };
    explicit NativeModelState(const IdentifiedDocumentSnapshot& initial);
    NativeModelState(const NativeModelState& other);
    NativeModelState& operator=(const NativeModelState&) = delete;
    [[nodiscard]] IdentifiedDocumentSnapshot snapshot() const;
    [[nodiscard]] const IdentifiedDocumentSnapshot& view() const noexcept { return *state_->view; }
    [[nodiscard]] std::uint64_t revision() const noexcept;
    [[nodiscard]] bool canUndo() const noexcept;
    [[nodiscard]] bool canRedo() const noexcept;
    [[nodiscard]] Prepared prepare(const IdentifiedDocumentSnapshot& replacement, bool recordHistory = true) const;
    [[nodiscard]] Prepared prepareUndo() const;
    [[nodiscard]] Prepared prepareRedo() const;
    // A successful prepared commit only swaps owners; no allocation/native mutation.
    [[nodiscard]] bool commit(Prepared&& prepared);
  private:
    [[nodiscard]] Prepared prepareHistory(bool undo) const;
    static Handle(TDocStd_Document) encode(const IdentifiedDocumentSnapshot& value);
    void checkRevisionAvailable() const;
    std::unique_ptr<State> state_;
};
} // namespace voelund::document::detail
