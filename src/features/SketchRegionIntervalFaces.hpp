#pragma once
#include <voelund/features/SketchEvaluation.hpp>
#include <voelund/features/SketchRegions.hpp>
namespace voelund::features::detail {
// True when every outer and hole boundary of the cells consists of unsplit contour pieces,
// so the R10 whole-loop path applies unchanged.
[[nodiscard]] bool wholeLoopCells(const SketchRegionResult& regions,
                                  std::span<const std::size_t> cells);
// R10b: faces of the union of the selected cells, built directly from the R08b boundary
// intervals as trimmed source curves. Intervals shared by two selected cells cancel, so
// adjacent cells become one face; cells touching only at a point stay separate faces.
[[nodiscard]] std::vector<EvaluatedProfileFace>
intervalRegionFaces(const document::SketchParameters& sketch, const SketchRegionResult& regions,
                    std::span<const std::size_t> cells);
} // namespace voelund::features::detail
