#pragma once
#include <cstdint>
#include <optional>

namespace voelund::application {
struct DocumentStamp final {
    std::uint64_t generation;
    std::uint64_t revision;
    bool operator==(const DocumentStamp&) const = default;
};
struct SceneStatus final {
    DocumentStamp document;
    std::optional<DocumentStamp> geometry;
    [[nodiscard]] bool isCurrent() const noexcept { return geometry == document; }
};
} // namespace voelund::application
