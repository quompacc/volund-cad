#pragma once

#include <voelund/document/ModelDocument.hpp>

#include <TopoDS_Shape.hxx>
#include <gp_Pnt.hxx>

#include <span>
#include <optional>
#include <vector>

namespace voelund::features {

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
};

[[nodiscard]] std::vector<EvaluatedSketchElement>
evaluateSketchElements(const document::SketchParameters& sketch);

[[nodiscard]] TopoDS_Shape evaluateSketchProfileFaces(const document::SketchParameters& sketch);
[[nodiscard]] TopoDS_Shape
evaluateSketchProfileFaces(const document::SketchParameters& sketch,
                           std::span<const document::SketchProfileId> selectedProfiles);

} // namespace voelund::features
