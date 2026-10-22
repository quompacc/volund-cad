// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "ZipStore.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace voelund::kernel::detail {
namespace {

std::uint32_t crc32(std::string_view data) {
    static const auto table = [] {
        std::array<std::uint32_t, 256> values{};
        for (std::uint32_t index = 0; index < 256; ++index) {
            std::uint32_t value = index;
            for (int bit = 0; bit < 8; ++bit)
                value = (value & 1U) != 0 ? 0xEDB88320U ^ (value >> 1U) : value >> 1U;
            values[index] = value;
        }
        return values;
    }();
    std::uint32_t crc = 0xFFFFFFFFU;
    for (const char c : data)
        crc = table[(crc ^ static_cast<unsigned char>(c)) & 0xFFU] ^ (crc >> 8U);
    return crc ^ 0xFFFFFFFFU;
}

void put16(std::string& out, std::uint32_t value) {
    out.push_back(static_cast<char>(value & 0xFFU));
    out.push_back(static_cast<char>((value >> 8U) & 0xFFU));
}
void put32(std::string& out, std::uint32_t value) {
    put16(out, value & 0xFFFFU);
    put16(out, value >> 16U);
}
std::uint32_t size32(std::size_t size) {
    if (size > std::numeric_limits<std::uint32_t>::max() - 1)
        throw std::length_error("The archive is too large");
    return static_cast<std::uint32_t>(size);
}

// The fields a local header and a central directory entry share, from "version needed" on.
void common(std::string& out, const ZipEntry& entry, std::uint32_t crc) {
    put16(out, 20);     // version needed: 2.0
    put16(out, 0);      // flags
    put16(out, 0);      // method: stored
    put16(out, 0);      // time 00:00
    put16(out, 0x0021); // date 1980-01-01
    put32(out, crc);
    put32(out, size32(entry.data.size())); // compressed
    put32(out, size32(entry.data.size())); // uncompressed
    put16(out, size32(entry.name.size()));
    put16(out, 0); // extra field
}

} // namespace

std::string zipStore(std::span<const ZipEntry> entries) {
    std::string out;
    std::string directory;
    for (const auto& entry : entries) {
        const auto crc = crc32(entry.data);
        const auto offset = size32(out.size());
        put32(out, 0x04034B50U);
        common(out, entry, crc);
        out.append(entry.name);
        out.append(entry.data);
        put32(directory, 0x02014B50U);
        put16(directory, 20); // made by: 2.0
        common(directory, entry, crc);
        put16(directory, 0); // comment
        put16(directory, 0); // disk
        put16(directory, 0); // internal attributes
        put32(directory, 0); // external attributes
        put32(directory, offset);
        directory.append(entry.name);
    }
    const auto start = size32(out.size());
    out.append(directory);
    put32(out, 0x06054B50U);
    put16(out, 0); // this disk
    put16(out, 0); // disk with the directory
    put16(out, size32(entries.size()));
    put16(out, size32(entries.size()));
    put32(out, size32(directory.size()));
    put32(out, start);
    put16(out, 0); // comment
    return out;
}

} // namespace voelund::kernel::detail
