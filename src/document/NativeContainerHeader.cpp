// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "NativeContainerHeader.hpp"
#include "DocumentSerializationDetail.hpp"
#include <array>
#include <cstdint>
#include <stdexcept>

namespace voelund::document::detail {
namespace {
constexpr std::size_t maximumMetadataBytes = 1024 * 1024;
[[noreturn]] void invalidHeader() {
    throw std::invalid_argument("Invalid or unsupported native container header");
}
class Cursor final {
  public:
    explicit Cursor(std::string_view bytes) : bytes_(bytes) {}
    std::uint32_t integer() {
        const auto data = take(4);
        std::uint32_t value = 0;
        for (unsigned i = 0; i < 4; ++i)
            value |= static_cast<std::uint32_t>(static_cast<unsigned char>(data[i])) << (8 * i);
        return value;
    }
    std::string_view text(std::size_t width = 1) {
        const auto count = integer();
        // Unsigned interpretation rejects negative on-disk lengths as well.
        if (count > maximumLineBytes / width || count > remaining() / width)
            invalidHeader();
        return take(static_cast<std::size_t>(count) * width);
    }
    void strings(std::size_t width) {
        const auto count = integer();
        if (count > maximumDocumentRecords || count > remaining() / 4)
            invalidHeader();
        for (std::uint32_t i = 0; i < count; ++i)
            (void)text(width);
    }
    void end() const {
        if (remaining() != 0)
            invalidHeader();
    }

  private:
    std::size_t remaining() const { return bytes_.size() - offset_; }
    std::string_view take(std::size_t count) {
        if (count > remaining())
            invalidHeader();
        const auto value = bytes_.substr(offset_, count);
        offset_ += count;
        return value;
    }
    std::string_view bytes_;
    std::size_t offset_{};
};
} // namespace
std::size_t validateNativeContainerHeader(std::string_view bytes) {
    constexpr std::string_view magic = "BINFILE";
    constexpr std::size_t fixedEnd = magic.size() + 13 * 4;
    if (bytes.size() < fixedEnd || bytes.size() > maximumDocumentBytes || !bytes.starts_with(magic))
        invalidHeader();
    Cursor fixed(bytes.substr(magic.size(), 13 * 4));
    std::array<std::uint32_t, 13> fields{};
    for (auto& field : fields)
        field = fixed.integer();
    const auto infoEnd = fields[2], commentEnd = fields[4];
    if (fields[0] != 0x04030201 || fields[1] != magic.size() || infoEnd < fixedEnd ||
        fields[3] != infoEnd || commentEnd < infoEnd || fields[12] != commentEnd ||
        commentEnd > maximumMetadataBytes || commentEnd >= bytes.size())
        invalidHeader();
    // BinOcaf uses user-info for its type table, not FSD's persistent-object sections.
    for (std::size_t i = 5; i <= 11; ++i)
        if (fields[i] != UINT32_MAX)
            invalidHeader();
    Cursor info(bytes.substr(fixedEnd, infoEnd - fixedEnd));
    if (info.integer() != 1 || info.text() != "12")
        invalidHeader();
    (void)info.text();  // date
    (void)info.text();  // FSD schema name; distinct from our application schema
    (void)info.text();  // FSD schema version
    (void)info.text(2); // application name (UTF-16)
    (void)info.text();  // application version
    (void)info.text(2); // data type
    info.strings(1);    // user-info, including driver names
    info.end();
    Cursor comments(bytes.substr(infoEnd, commentEnd - infoEnd));
    comments.strings(2);
    comments.end();
    return commentEnd;
}
} // namespace voelund::document::detail
