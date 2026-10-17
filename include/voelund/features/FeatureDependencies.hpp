#pragma once
#include <optional>
#include <span>
#include <vector>
#include <voelund/document/BodyId.hpp>
#include <voelund/document/FeatureId.hpp>
namespace voelund::document {
class DocumentSnapshot;
}
namespace voelund::features {
// T02/T03: the latest feature before `feature` (document order) that produced `body`'s
// solid; nullopt before the body's first solid.
[[nodiscard]] std::optional<document::FeatureId>
bodyProducerBefore(const document::DocumentSnapshot& snapshot, document::FeatureId feature,
                   document::BodyId body);
// T02: a join or cut of a base-plane sketch works on its target body as the latest earlier
// feature left it; nullopt without a target or before the body's first solid.
[[nodiscard]] std::optional<document::FeatureId>
targetProducer(const document::DocumentSnapshot& snapshot, document::FeatureId extrusion);
// M14: the input a feature evaluates on. Past a suppressed input the body is taken as the latest
// earlier feature that is not suppressed left it; an input without a body (a sketch) stays.
[[nodiscard]] std::optional<document::FeatureId>
effectiveInput(const document::DocumentSnapshot& snapshot, document::FeatureId feature);
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
} // namespace voelund::features
