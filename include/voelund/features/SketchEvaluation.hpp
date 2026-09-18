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
