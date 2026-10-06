#pragma once

#include <voelund/features/SketchEvaluation.hpp>
#include <voelund/viewport/CadViewport.hpp>

#include <QString>

#include <vector>

namespace voelund::application {
class CadSession;
struct SketchSupportProjection;
} // namespace voelund::application

namespace voelund::desktop {
void setSupportProjection(viewport::PlanarFaceSelection& support,
                          const application::SketchSupportProjection& projection);

[[nodiscard]] viewport::SceneSketchElement
makeSceneSketchElement(const features::EvaluatedSketchElement& element, bool conflicting = false,
                       bool determined = false);
[[nodiscard]] std::vector<viewport::SceneSketchElement>
makeSceneSketchElements(const application::CadSession& session);
[[nodiscard]] QString sketchSelectionSummary(const application::CadSession& session);

} // namespace voelund::desktop
