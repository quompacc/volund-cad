#pragma once

#include <voelund/document/BodyId.hpp>
#include <voelund/document/ModelDocument.hpp>

namespace voelund::document {

struct FeatureBodyIdentity final {
    FeatureId feature;
    // Sketches have no solid output, even when they have a support body.
    std::optional<BodyId> body;
    [[nodiscard]] bool operator==(const FeatureBodyIdentity&) const = default;
};

// Read-only migration projection. Corresponding descriptors/parameters may be reordered
// together. Body seeds come from creating feature IDs, never names or row positions.
// Throws invalid_argument for invalid IDs, dependencies, cycles or parameter kinds.
[[nodiscard]] std::vector<FeatureBodyIdentity>
legacyBodyIdentities(std::span<const FeatureDescriptor> features,
                     std::span<const FeatureParameters> parameters);

} // namespace voelund::document
