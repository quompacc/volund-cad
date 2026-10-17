#pragma once

#include <voelund/features/SketchEvaluation.hpp>
#include <voelund/features/SketchProfiles.hpp>

namespace voelund::features::detail {

// L06: the face of one already detected closed profile, with its named contour edges. Callers
// detect the profiles once (detectClosedProfiles) and check the plane once; building a face
// per region must not detect every profile of the sketch again.
[[nodiscard]] EvaluatedProfileFace profileFaceOf(const document::SketchParameters& sketch,
                                                 const SketchProfile& profile);

} // namespace voelund::features::detail
