#pragma once

#include <voelund/document/ModelDocument.hpp>

#include <gp_Pnt.hxx>

#include <vector>

namespace voelund::features {

struct SketchConstraintAnnotation final {
    document::SketchConstraintId id;
    document::SketchConstraintKind kind;
    gp_Pnt position;
    // Internal units: radians for angles, millimeters for lengths.
    double value;
};

[[nodiscard]] std::vector<SketchConstraintAnnotation>
evaluateSketchConstraintAnnotations(const document::SketchParameters& sketch);

} // namespace voelund::features
