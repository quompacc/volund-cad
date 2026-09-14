#pragma once
#include <voelund/document/SketchPlaneValidation.hpp>
#include <voelund/features/GeometryEvaluationError.hpp>

namespace voelund::features::detail {
inline void requireCanonicalPlane(const document::SketchPlane& plane) {
    try {
        document::validateCanonicalSketchPlane(plane);
    } catch (const std::invalid_argument& error) {
        throw GeometryEvaluationError(error.what());
    }
}
} // namespace voelund::features::detail
