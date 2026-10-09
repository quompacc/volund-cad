#pragma once

#include <voelund/kernel/OcctCompatibility.hpp>
#include <voelund/viewport/CadViewport.hpp>

#include <QRect>
#include <V3d_View.hxx>

#include <cstdint>
#include <span>
#include <vector>

namespace voelund::viewport {

// S13: whole elements in a selection box. A window takes elements whose curve lies fully
// inside, a crossing box elements whose curve it touches. Curves are sampled on screen, so
// arcs, circles and ellipses count by their curve, not by their center or end points.
[[nodiscard]] std::vector<std::uint64_t>
boxSelection(std::span<const SceneSketchElement> elements, const occ::handle<V3d_View>& view,
             const QRect& rectangle, bool crossing);

} // namespace voelund::viewport
