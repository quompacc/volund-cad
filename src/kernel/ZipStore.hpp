#pragma once

#include <span>
#include <string>
#include <string_view>

namespace voelund::kernel::detail {

struct ZipEntry final {
    std::string_view name; // ASCII path inside the archive
    std::string_view data;
};

// N03: a ZIP archive with the entries stored uncompressed (method 0), as a 3MF package may be.
// Throws std::length_error beyond the 4 GiB limits of the format without ZIP64.
[[nodiscard]] std::string zipStore(std::span<const ZipEntry> entries);

} // namespace voelund::kernel::detail
