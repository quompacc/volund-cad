#pragma once

#include <voelund/application/SceneStatus.hpp>
#include <voelund/document/ModelDocument.hpp>
#include <voelund/viewport/CadViewport.hpp>

#include <functional>
#include <optional>
#include <vector>

namespace voelund::application {
class CadSession;
}

namespace voelund::desktop {

class ToolPanelHost;

class SketchExtrusionToolController final {
  public:
    SketchExtrusionToolController(application::CadSession& session, viewport::CadViewport& viewport,
                                  ToolPanelHost& panel, std::function<void()> sceneChanged,
                                  std::function<void(const QString&)> statusChanged);

    void begin(const viewport::PlanarFaceSelection& support);
    void beginSelectedProfiles();
    void updateDistancePreview(double distance);
    void setResult(const viewport::SketchRectangleResult& result);
    [[nodiscard]] bool isActive() const noexcept;
    void cancel();

  private:
    void setDistance(double distance);
    void accept();

    application::CadSession& session_;
    viewport::CadViewport& viewport_;
    ToolPanelHost& panel_;
    std::function<void()> sceneChanged_;
    std::function<void(const QString&)> statusChanged_;
    std::optional<viewport::SketchRectangleResult> result_;
    std::optional<document::FeatureId> profileSketch_;
    std::optional<document::SketchSupportBinding> supportBinding_;
    std::optional<application::DocumentStamp> supportStamp_;
    std::vector<document::SketchRegionId> profileIds_;
    double distance_{1.0};
    bool active_{false};
};

} // namespace voelund::desktop
