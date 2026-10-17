#pragma once
#include <cstdint>
#include <limits>

namespace voelund::document {
// Zero is invalid; UINT64_MAX is reserved by the legacy reader. The resulting
// next counter must remain serializable, even after the last permitted allocation.
[[nodiscard]] constexpr bool hasIdCapacity(std::uint64_t next, std::uint64_t count) noexcept {
    constexpr auto limit = std::numeric_limits<std::uint64_t>::max();
    return next != 0 && next < limit && count < limit - next;
}
[[nodiscard]] constexpr bool hasRepeatedIdCapacity(std::uint64_t next, std::uint64_t perCopy,
                                                   std::uint64_t copies) noexcept {
    constexpr auto limit = std::numeric_limits<std::uint64_t>::max();
    return hasIdCapacity(next, 0) && (perCopy == 0 || copies <= (limit - 1 - next) / perCopy);
}
} // namespace voelund::document
