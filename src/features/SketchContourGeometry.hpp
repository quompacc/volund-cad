#pragma once
#include <voelund/features/SketchContourGraph.hpp>
namespace voelund::features::detail {
[[nodiscard]] std::vector<ContourPiece> contourPieces(std::span<const document::SketchElement>);
[[nodiscard]] bool contourOverlap(const ContourPiece&, const ContourPiece&);
[[nodiscard]] bool contourInterior(const ContourPiece&, document::SketchPoint);
[[nodiscard]] document::SketchPoint contourMiddle(const ContourPiece&);
} // namespace voelund::features::detail
