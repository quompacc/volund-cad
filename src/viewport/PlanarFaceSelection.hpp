#pragma once

#include <voelund/viewport/CadViewport.hpp>

#include <TopoDS_Shape.hxx>

#include <cstdint>
#include <optional>

namespace voelund::viewport {

// Sketch support from a picked planar face; reference edges in the face frame.
[[nodiscard]] std::optional<PlanarFaceSelection> planarFaceSelection(std::uint64_t bodyId,
                                                                    const TopoDS_Shape& detectedShape);

} // namespace voelund::viewport
