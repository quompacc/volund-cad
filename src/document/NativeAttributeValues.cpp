// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "NativeAttributeValues.hpp"
#include "DocumentSerializationDetail.hpp"
#include "NativeContainerHeader.hpp"
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace voelund::document::detail {
namespace {
[[noreturn]] void invalidAttribute() {
    throw std::invalid_argument("Invalid or unsupported native attribute data");
}
std::uint64_t integer(std::string_view bytes, std::size_t at, unsigned width = 4) {
    if (at > bytes.size() || width > bytes.size() - at)
        invalidAttribute();
    std::uint64_t value = 0;
    for (unsigned i = 0; i < width; ++i)
        value |= static_cast<std::uint64_t>(static_cast<unsigned char>(bytes[at + i])) << (8 * i);
    return value;
}
std::string_view text(std::string_view bytes, std::size_t& at, std::size_t width = 1) {
    const auto count = integer(bytes, at);
    at += 4;
    if (count > (bytes.size() - at) / width)
        invalidAttribute();
    const auto result = bytes.substr(at, static_cast<std::size_t>(count) * width);
    at += result.size();
    return result;
}
void finiteReal(std::string_view bytes, std::size_t at) {
    if (!std::isfinite(std::bit_cast<double>(integer(bytes, at, 8))))
        invalidAttribute();
}
} // namespace
NativeAttributeTypes readNativeAttributeTypes(std::string_view bytes) {
    (void)validateNativeContainerHeader(bytes);
    std::size_t at = 63; // fixed header and object count, already validated
    for (const auto width : {1U, 1U, 1U, 1U, 2U, 1U, 2U})
        (void)text(bytes, at, width);
    const auto count = integer(bytes, at);
    at += 4;
    if (count < 6 || count > 11 || text(bytes, at) != "FILE_FORMAT: BinOcaf" ||
        text(bytes, at) != "REFERENCE_COUNTER: 0" || text(bytes, at) != "MODIFICATION_COUNTER: 1" ||
        text(bytes, at) != "START_TYPES")
        invalidAttribute();
    constexpr std::array<std::string_view, 6> names{"TDataStd_AsciiString", "TDataStd_Integer",
                                                    "TDataStd_RealArray",   "TDF_Reference",
                                                    "TDataStd_Real",        "TDataStd_ByteArray"};
    NativeAttributeTypes result;
    unsigned seen = 0;
    for (std::uint64_t i = 0; i < count - 5; ++i) {
        const auto name = text(bytes, at);
        std::size_t index = 0;
        while (index < names.size() && names[index] != name)
            ++index;
        if (index == names.size() || (seen & (1U << index)))
            invalidAttribute();
        seen |= 1U << index;
        result.values[result.count++] = static_cast<NativeAttributeKind>(index);
    }
    if (text(bytes, at) != "END_TYPES" || at != integer(bytes, 15))
        invalidAttribute();
    return result;
}
std::size_t validateNativeAttributeValue(NativeAttributeKind kind, std::string_view bytes) {
    switch (kind) {
    case NativeAttributeKind::ascii:
        if (bytes.empty() || bytes.size() > maximumNameBytes + 1 || bytes.back() != '\0' ||
            bytes.find('\0') != bytes.size() - 1)
            invalidAttribute();
        break;
    case NativeAttributeKind::integer:
        if (bytes.size() != 4)
            invalidAttribute();
        break;
    case NativeAttributeKind::real:
        if (bytes.size() != 8)
            invalidAttribute();
        finiteReal(bytes, 0);
        break;
    case NativeAttributeKind::realArray: {
        const auto lower = integer(bytes, 0), upper = integer(bytes, 4);
        // Our codec always writes arrays starting at 1, with the default GUID and no delta mode.
        if (lower != 1 || upper == 0 || upper > maximumSketchElementCount ||
            upper > (bytes.size() - 8) / 8 || bytes.size() != 9 + upper * 8 || bytes.back() != '\0')
            invalidAttribute();
        for (std::uint64_t i = 0; i < upper; ++i)
            finiteReal(bytes, 8 + static_cast<std::size_t>(i) * 8);
        break;
    }
    case NativeAttributeKind::byteArray: {
        // M17: first and last index (always 1..n), the bytes, no delta mode, default GUID.
        const auto lower = integer(bytes, 0), upper = integer(bytes, 4);
        if (lower != 1 || upper == 0 || upper > maximumImportedBytes || bytes.size() != 9 + upper ||
            bytes.back() != '\0')
            invalidAttribute();
        break;
    }
    case NativeAttributeKind::reference: {
        const auto count = integer(bytes, 0);
        if (count == 0 || count > maximumFeatureDepth || bytes.size() != 4 + count * 4 ||
            integer(bytes, 4) != 0)
            invalidAttribute();
        for (std::uint64_t i = 1; i < count; ++i) {
            const auto tag = integer(bytes, 4 + static_cast<std::size_t>(i) * 4);
            if (tag == 0 || tag > static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
                invalidAttribute();
        }
        return static_cast<std::size_t>(count);
    }
    default:
        invalidAttribute();
    }
    return 0;
}
} // namespace voelund::document::detail
