#pragma once

#include <voelund/document/ModelDocument.hpp>

#include <TopoDS_Shape.hxx>

#include <vector>

namespace voelund::features {

struct EvaluatedSketchElement final {
    document::SketchElementId id;
    TopoDS_Shape shape;
};

[[nodiscard]] std::vector<EvaluatedSketchElement> evaluateSketchElements(
    const document::SketchParameters& sketch);

[[nodiscard]] TopoDS_Shape evaluateSketchProfileFaces(
    const document::SketchParameters& sketch);

} // namespace voelund::features
