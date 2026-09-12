#pragma once

#include <voelund/document/ModelDocument.hpp>

#include <TopoDS_Shape.hxx>
#include <gp_Pnt.hxx>

#include <span>
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
};

[[nodiscard]] std::vector<EvaluatedSketchElement>
evaluateSketchElements(const document::SketchParameters& sketch);

[[nodiscard]] TopoDS_Shape evaluateSketchProfileFaces(const document::SketchParameters& sketch);
[[nodiscard]] TopoDS_Shape
evaluateSketchProfileFaces(const document::SketchParameters& sketch,
                           std::span<const document::SketchProfileId> selectedProfiles);

} // namespace voelund::features
