#pragma once
#include <array>
#include <cstddef>
#include <string_view>

namespace voelund::document::detail {
// M17: byteArray holds the BRep of an imported solid.
enum class NativeAttributeKind { ascii, integer, realArray, reference, real, byteArray };
struct NativeAttributeTypes final {
    std::array<NativeAttributeKind, 6> values{};
    std::size_t count{};
};
[[nodiscard]] NativeAttributeTypes readNativeAttributeTypes(std::string_view bytes);
// Returns the number of reference tags, for the document-wide allocation budget.
[[nodiscard]] std::size_t validateNativeAttributeValue(NativeAttributeKind kind, std::string_view bytes);
} // namespace voelund::document::detail
