#pragma once
#include <string_view>

namespace voelund::document::detail {
// Checks the supported BinOcaf-12 section table, label/attribute framing and EOF.
// Attribute payload values and driver/type mapping require separate validation.
void validateNativeContainerStructure(std::string_view bytes);
} // namespace voelund::document::detail
