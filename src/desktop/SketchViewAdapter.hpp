#pragma once

#include <voelund/viewport/CadViewport.hpp>

#include <QString>

#include <vector>

namespace voelund::application {
class CadSession;
}

namespace voelund::desktop {

[[nodiscard]] std::vector<viewport::SceneSketchElement> makeSceneSketchElements(
    const application::CadSession& session);
[[nodiscard]] QString sketchSelectionSummary(const application::CadSession& session);

} // namespace voelund::desktop
