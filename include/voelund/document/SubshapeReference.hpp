#pragma once
#include <voelund/document/BodyId.hpp>
#include <voelund/document/DocumentId.hpp>
#include <voelund/document/FeatureId.hpp>
#include <voelund/document/SketchGeometry.hpp>
#include <voelund/kernel/OperationHistory.hpp>

namespace voelund::document {
// Face-only subset of the R02 contract. Exactly one role representation is set.
// Extrusion end cap: canonical sorted boundary element IDs, never a ProfileId/index.
// T04 (schema 10): the end cap of an extrusion whose profile is a body face.
struct SubshapeOrigin final {
    FeatureId feature;
    std::optional<kernel::PrimitiveRole> role;
    std::vector<SketchElementId> extrusionBoundary{};
    bool faceExtrusionCap{false};
    bool operator==(const SubshapeOrigin&) const = default;
};
struct SubshapeRef final {
    DocumentId document;
    BodyId body;
    SubshapeOrigin origin;
    bool operator==(const SubshapeRef&) const = default;
};
void validateSubshapeOrigin(const SubshapeOrigin& origin);
} // namespace voelund::document
