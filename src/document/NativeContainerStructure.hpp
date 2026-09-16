#pragma once
#include <string_view>

namespace voelund::document::detail {
// Checks the supported BinOcaf-12 section table, label/attribute framing and EOF.
// validateAttributes additionally checks driver mapping and attribute payload values.
// The store always enables it; framing-only mode supports isolated structure tests.
void validateNativeContainerStructure(std::string_view bytes, bool validateAttributes = false);
} // namespace voelund::document::detail
