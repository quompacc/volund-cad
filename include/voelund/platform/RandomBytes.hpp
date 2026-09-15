#pragma once
#include <array>
#include <cstdint>

namespace voelund::platform {
// Operating-system entropy. Failure throws; no clock/counter/PRNG fallback.
[[nodiscard]] std::array<std::uint8_t, 16> randomBytes128();
} // namespace voelund::platform
