#include <stdexcept>
#include <voelund/platform/RandomBytes.hpp>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// bcrypt.h needs the Windows types declared first.
#include <bcrypt.h>
#elif defined(__FreeBSD__)
#include <cstdlib>
#elif defined(__linux__)
#include <cerrno>
#include <sys/random.h>
#include <system_error>
#else
#error Unsupported operating system for document identity entropy
#endif

namespace voelund::platform {
std::array<std::uint8_t, 16> randomBytes128() {
    std::array<std::uint8_t, 16> bytes{};
#ifdef _WIN32
    const auto status = BCryptGenRandom(nullptr, bytes.data(), static_cast<ULONG>(bytes.size()),
                                        BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (status < 0)
        throw std::runtime_error("Operating-system random generator failed");
#elif defined(__FreeBSD__)
    arc4random_buf(bytes.data(), bytes.size());
#elif defined(__linux__)
    if (getentropy(bytes.data(), bytes.size()) != 0)
        throw std::system_error(errno, std::generic_category(),
                                "Operating-system random generator failed");
#endif
    return bytes;
}
} // namespace voelund::platform
