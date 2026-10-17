#pragma once
#include <string>
#include <string_view>
#include <voelund/document/IdentifiedDocumentSnapshot.hpp>

namespace voelund::document::detail {
// Private BinOcaf payload adapter. Product dispatch and atomic replacement are separate.
// Container version 12 is independent of the application's native schema version.
class NativeDocumentStore final {
  public:
    [[nodiscard]] static std::string encode(const IdentifiedDocumentSnapshot& snapshot);
    [[nodiscard]] static IdentifiedDocumentSnapshot decode(std::string_view bytes);
};
} // namespace voelund::document::detail
