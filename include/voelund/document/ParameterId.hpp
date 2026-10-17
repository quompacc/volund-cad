#pragma once

#include <cstdint>

namespace voelund::document {

// PAR01: stable identity of a document parameter, independent of its name.
class ParameterId final {
  public:
    constexpr explicit ParameterId(std::uint64_t value) noexcept : value_(value) {}

    [[nodiscard]] constexpr std::uint64_t value() const noexcept { return value_; }
    [[nodiscard]] constexpr bool operator==(const ParameterId&) const noexcept = default;

  private:
    std::uint64_t value_;
};

} // namespace voelund::document
