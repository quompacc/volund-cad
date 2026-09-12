#pragma once

#include <voelund/kernel/OcctCompatibility.hpp>
#include <voelund/viewport/CadViewport.hpp>

#include <QRect>
#include <V3d_View.hxx>

namespace voelund::viewport {

[[nodiscard]] std::vector<SketchSelectionIntent>
boxSelectionIntents(std::span<const SceneSketchElement> elements,
                    const occ::handle<V3d_View>& view, const QRect& rectangle);

} // namespace voelund::viewport
