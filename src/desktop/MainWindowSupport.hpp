#pragma once

#include <voelund/document/ModelDocument.hpp>
#include <voelund/viewport/CadViewport.hpp>

#include <QString>

#include <optional>
#include <vector>

namespace voelund::application { class CadSession; }

namespace voelund::desktop {

[[nodiscard]] std::vector<viewport::SceneBody> makeSceneBodies(
    const application::CadSession& session);
[[nodiscard]] QString featureErrorText(
    const application::CadSession& session, document::FeatureId featureId);
[[nodiscard]] int resultModeIndex(document::CylinderResultMode mode);
[[nodiscard]] document::CylinderResultMode resultModeFromIndex(int index);
[[nodiscard]] std::optional<document::FeatureId> latestCylinderId(
    const application::CadSession& session);
[[nodiscard]] bool selectTransformFeatureForBody(
    application::CadSession& session, document::FeatureId bodyId);

} // namespace voelund::desktop
