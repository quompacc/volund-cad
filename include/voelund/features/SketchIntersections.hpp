#pragma once
#include <voelund/features/SketchContourGraph.hpp>
namespace voelund::features {
enum class SketchIntersectionKind { crossing, tangent, endpoint };
struct SketchIntersection final {
    std::size_t firstPiece;
    std::size_t secondPiece;
    double firstParameter;
    double secondParameter;
    document::SketchPoint point;
    SketchIntersectionKind kind;
};
struct SketchBoundaryInterval final {
    std::size_t piece;
    double firstParameter;
    double lastParameter;
    document::SketchPoint start;
    document::SketchPoint end;
};
enum class SketchIntersectionIssue { overlap, calculationFailed, unresolvedInterval };
struct SketchIntersectionDiagnostic final {
    SketchIntersectionIssue issue;
    std::vector<document::SketchElementId> elements;
};
struct SketchIntersectionResult final {
    std::vector<ContourPiece> pieces;
    std::vector<SketchIntersection> intersections;
    std::vector<SketchBoundaryInterval> intervals;
    std::vector<SketchIntersectionDiagnostic> diagnostics;
    [[nodiscard]] bool valid() const noexcept { return diagnostics.empty(); }
};
// Analytic source geometry is retained. Line parameters are lengths in mm;
// circle/arc/ellipse parameters are radians. Periodic seams remain explicit.
// An invalid result contains diagnostics but no consumable intervals.
// Input must satisfy the document geometry contract. No regions or faces yet.
[[nodiscard]] SketchIntersectionResult
splitSketchIntersections(std::span<const document::SketchElement> elements);
} // namespace voelund::features
