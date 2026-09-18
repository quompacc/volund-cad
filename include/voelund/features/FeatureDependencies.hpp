#pragma once
#include <voelund/document/FeatureId.hpp>
#include <span>
#include <vector>
namespace voelund::document { class DocumentSnapshot; }
namespace voelund::features {
struct FeatureInputs final {
    document::FeatureId feature;
    std::vector<document::FeatureId> inputs;
    bool operator==(const FeatureInputs&) const = default;
};
// Validates IDs and edges before returning a deterministic, stable topological order.
class FeatureDependencies final {
public:
    explicit FeatureDependencies(std::vector<FeatureInputs> nodes);
    [[nodiscard]] static FeatureDependencies from(const document::DocumentSnapshot& snapshot);
    [[nodiscard]] std::span<const FeatureInputs> nodes() const noexcept { return nodes_; }
    [[nodiscard]] std::span<const std::size_t> order() const noexcept { return order_; }
    [[nodiscard]] std::vector<bool> affected(std::span<const document::FeatureId> changed) const;
private:
    std::vector<FeatureInputs> nodes_;
    std::vector<std::size_t> order_;
};
}
