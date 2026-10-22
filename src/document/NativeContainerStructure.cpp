// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "NativeContainerStructure.hpp"
#include "DocumentSerializationDetail.hpp"
#include "NativeAttributeValues.hpp"
#include "NativeContainerHeader.hpp"
#include <bitset>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>

namespace voelund::document::detail {
namespace {
[[noreturn]] void invalidStructure() {
    throw std::invalid_argument("Invalid or unsupported native container structure");
}
class Structure final {
  public:
    Structure(std::string_view bytes, bool validateAttributes)
        : bytes_(bytes), offset_(validateNativeContainerHeader(bytes)) {
        if (validateAttributes)
            types_ = readNativeAttributeTypes(bytes);
    }
    void validate() {
        // Our parameter-only documents have no additional shape/application sections.
        if (integer(4, bytes_.size()) != 4 ||
            take(4, bytes_.size()) != std::string_view(":\0\0\0", 4))
            invalidStructure();
        for (int i = 0; i < 3; ++i)
            if (integer(8, bytes_.size()) != 0)
                invalidStructure();
        if (integer(4, bytes_.size()) != 0)
            invalidStructure(); // root label tag
        label(bytes_.size(), 1);
        if (offset_ != bytes_.size())
            invalidStructure();
    }

  private:
    std::string_view take(std::size_t count, std::size_t end) {
        if (offset_ > end || count > end - offset_)
            invalidStructure();
        const auto value = bytes_.substr(offset_, count);
        offset_ += count;
        return value;
    }
    std::uint64_t integer(unsigned width, std::size_t end) {
        const auto data = take(width, end);
        std::uint64_t value = 0;
        for (unsigned i = 0; i < width; ++i)
            value |= static_cast<std::uint64_t>(static_cast<unsigned char>(data[i])) << (8 * i);
        return value;
    }
    void label(std::size_t parentEnd, std::size_t depth) {
        if (depth > maximumFeatureDepth || ++labels_ > maximumDocumentRecords)
            invalidStructure();
        const auto start = offset_;
        const auto size = integer(8, parentEnd);
        // Size includes its own 8-byte word, end-attribute marker and end-label marker.
        if (size < 16 || size > parentEnd - start)
            invalidStructure();
        const auto end = start + static_cast<std::size_t>(size);
        attributes(end);
        for (;;) {
            const auto tag = integer(4, end);
            if (tag == 0xfffffffe)
                break;
            if (tag == 0 || tag > static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
                invalidStructure();
            label(end, depth + 1);
        }
        if (offset_ != end)
            invalidStructure();
    }
    void attributes(std::size_t end) {
        for (;;) {
            const auto type = integer(4, end);
            if (type == 0xffffffff)
                return;
            if (type == 0 || type > static_cast<std::uint64_t>(std::numeric_limits<int>::max()) ||
                ++attributes_ > maximumDocumentRecords)
                invalidStructure();
            const auto id = integer(4, end);
            // Negative IDs announce direct streamed attributes, not used by our parameter schema.
            if (id == 0 || id > static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
                invalidStructure();
            const auto size = integer(4, end);
            if (size > end - offset_)
                invalidStructure();
            const auto value = take(static_cast<std::size_t>(size), end);
            if (types_) {
                if (type > types_->count || id > maximumDocumentRecords ||
                    ids_.test(static_cast<std::size_t>(id)))
                    invalidStructure();
                ids_.set(static_cast<std::size_t>(id));
                referenceTags_ += validateNativeAttributeValue(
                    types_->values[static_cast<std::size_t>(type - 1)], value);
                if (referenceTags_ > maximumDocumentRecords)
                    invalidStructure();
            }
        }
    }
    std::string_view bytes_;
    std::size_t offset_;
    std::size_t labels_{};
    std::size_t attributes_{};
    std::optional<NativeAttributeTypes> types_;
    std::bitset<maximumDocumentRecords + 1> ids_;
    std::size_t referenceTags_{};
};
} // namespace
void validateNativeContainerStructure(std::string_view bytes, bool validateAttributes) {
    Structure(bytes, validateAttributes).validate();
}
} // namespace voelund::document::detail
