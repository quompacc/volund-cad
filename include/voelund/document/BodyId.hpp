#pragma once

#include <stdexcept>
#include <voelund/document/IdCapacity.hpp>

namespace voelund::document {

// Local to one DocumentId; never implicitly interchangeable with FeatureId.
class BodyId final {
  public:
    constexpr explicit BodyId(std::uint64_t value) : value_(value) {
        if (!hasIdCapacity(value, 1))
            throw std::invalid_argument("Body ID is outside the allocatable range");
    }
    [[nodiscard]] constexpr std::uint64_t value() const noexcept { return value_; }
    [[nodiscard]] constexpr bool operator==(const BodyId&) const noexcept = default;

  private:
    std::uint64_t value_;
};

} // namespace voelund::document
