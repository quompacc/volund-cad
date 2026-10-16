#pragma once
#include <cstddef>
#include <cstdint>
#include <voelund/document/BodyId.hpp>
#include <voelund/document/DocumentId.hpp>
#include <voelund/document/EdgeReference.hpp>
#include <voelund/document/FeatureId.hpp>
#include <voelund/document/SketchGeometry.hpp>
#include <voelund/kernel/OperationHistory.hpp>

namespace voelund::document {
// T04b: which face of an extrusion's prism (ExtrusionCapKey / ExtrusionSideKey of the R02
// contract): its end or start cap, or the side face swept from a named edge.
enum class ExtrusionFace { end, start, side };
// Face-only subset of the R02 contract. Exactly one representation is set: a primitive
// role; the canonical sorted boundary element IDs of a sketch extrusion's profile (never a
// ProfileId/index); or T04 (schema 10) a face extrusion (push/pull). Extrusion faces name
// their cap or the edge a side face was swept from: a sketch contour edge, or for a face
// extrusion the named edge of the body face.
// M09 (schema 31): one copy step of a face made by a mirror (later a pattern) feature; a
// mirror makes copy 1.
inline constexpr std::size_t maximumSubshapeInstances = 16;
struct SubshapeInstance final {
    FeatureId feature;
    std::uint32_t index;
    bool operator==(const SubshapeInstance&) const = default;
};
struct SubshapeOrigin final {
    FeatureId feature;
    std::optional<kernel::PrimitiveRole> role;
    std::vector<SketchElementId> extrusionBoundary{};
    bool faceExtrusion{false};
    ExtrusionFace extrusionFace{ExtrusionFace::end};
    std::optional<EdgeOrigin> sweptEdge{};
    // Schema 17 (user decision 2026-10-13): the face a fillet or chamfer `feature` generated
    // from its edge with this id (FilletEdge::id).
    std::optional<std::uint64_t> treatedEdge{};
    // M09: the copies this face went through, oldest first; empty for the face itself.
    std::vector<SubshapeInstance> instances{};
    // M17 (schema 35): face number (from 1) of an imported solid, as stored with the import.
    std::optional<std::uint32_t> importedFace{};
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
