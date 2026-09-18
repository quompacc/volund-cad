#pragma once
#include <cstdint>
namespace voelund::document {
class SketchElementId final {
  public:
    constexpr explicit SketchElementId(std::uint64_t value) noexcept : value_(value) {}

    [[nodiscard]] constexpr std::uint64_t value() const noexcept { return value_; }
    [[nodiscard]] constexpr bool operator==(const SketchElementId&) const noexcept = default;

  private:
    std::uint64_t value_;
};

} // namespace voelund::document
