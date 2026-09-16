#pragma once
#include <iosfwd>
#include <string>

namespace voelund::document::detail {
enum class DocumentFileKind { legacyText, nativeBinary };
struct DocumentFileInput final {
    DocumentFileKind kind;
    std::string bytes;
    // Capture from the current position without seeking, then select one format.
    // Classification is not schema/container validation and invokes no decoder.
    [[nodiscard]] static DocumentFileInput read(std::istream& source);
};
} // namespace voelund::document::detail
