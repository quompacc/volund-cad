#pragma once

#include <voelund/document/ModelDocument.hpp>
#include <voelund/features/EdgeProvenance.hpp>

#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Pnt.hxx>

#include <optional>
#include <span>
#include <vector>

namespace voelund::features {

struct EvaluatedContourEdge final {
    document::SketchElementId element;
    ContourRole role;
    TopoDS_Edge edge;
    double startParameter;
    double endParameter;
};
struct EvaluatedProfileFace final {
    std::vector<document::SketchElementId> boundary;
    TopoDS_Face face;
    std::vector<EvaluatedContourEdge> edges{};
};
[[nodiscard]] std::vector<EvaluatedProfileFace>
evaluateNamedProfileFaces(const document::SketchParameters& sketch,
                          std::span<const document::SketchProfileId> selectedProfiles);
// R10 (V01 scope): each selected region whose outer and hole boundaries are whole closed
// loops (nested, non-crossing contours) becomes one valid face with inner wires; a selected
// inner region fills its parent's hole. Unresolved RegionIds and arrangements of crossing
// boundaries (R10b) are rejected with the elements involved, never substituted or empty.
[[nodiscard]] std::vector<EvaluatedProfileFace>
evaluateRegionFaces(const document::SketchParameters& sketch,
                    std::span<const document::SketchRegionId> regions);
// Regions when selected, otherwise the legacy profile selection (empty: all profiles).
[[nodiscard]] std::vector<EvaluatedProfileFace>
evaluateExtrusionFaces(const document::SketchParameters& sketch,
                       const document::ExtrusionParameters& parameters);

struct EvaluatedSketchPoint final {
    document::SketchPointPosition kind;
    document::SketchPoint local;
    gp_Pnt world;
};

struct EvaluatedSketchElement final {
    document::SketchElementId id;
    TopoDS_Shape shape;
    bool construction;
    std::vector<EvaluatedSketchPoint> points;
    double radialRadius{0.0};
    std::optional<gp_Pnt> radialAnchor;
    std::vector<EvaluatedContourEdge> edges{};
};

[[nodiscard]] std::vector<EvaluatedSketchElement>
evaluateSketchElements(const document::SketchParameters& sketch);

[[nodiscard]] TopoDS_Shape evaluateSketchProfileFaces(const document::SketchParameters& sketch);
[[nodiscard]] TopoDS_Shape
evaluateSketchProfileFaces(const document::SketchParameters& sketch,
                           std::span<const document::SketchProfileId> selectedProfiles);

} // namespace voelund::features
