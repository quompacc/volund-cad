#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace voelund::document {

// Persistent document namespace. Generation and encoding belong to the store boundary.
class DocumentId final {
  public:
    constexpr DocumentId(std::uint64_t high, std::uint64_t low) : high_(high), low_(low) {
        if (high == 0 && low == 0)
            throw std::invalid_argument("Document ID must be non-zero");
    }
    [[nodiscard]] constexpr std::uint64_t high() const noexcept { return high_; }
    [[nodiscard]] constexpr std::uint64_t low() const noexcept { return low_; }
    [[nodiscard]] constexpr bool operator==(const DocumentId&) const noexcept = default;

  private:
    std::uint64_t high_;
    std::uint64_t low_;
};

// Lossless, canonical field encoding: 16 high-word then 16 low-word hex digits.
[[nodiscard]] std::string documentIdToHex(DocumentId id);
[[nodiscard]] DocumentId documentIdFromHex(std::string_view text);
// Random non-zero 128-bit namespace; not a UUID or a sequential revision number.
[[nodiscard]] DocumentId generateDocumentId();

} // namespace voelund::document
