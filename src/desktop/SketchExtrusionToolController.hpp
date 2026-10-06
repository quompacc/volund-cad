#pragma once

#include <voelund/application/SceneStatus.hpp>
#include <voelund/document/ModelDocument.hpp>

#include <QString>

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace voelund::application {
class CadSession;
}

namespace voelund::desktop {

class ToolPanelHost;

class SketchExtrusionToolController final {
  public:
    SketchExtrusionToolController(application::CadSession& session, ToolPanelHost& panel,
                                  std::function<void()> sceneChanged,
                                  std::function<void(const QString&)> statusChanged);

    void beginSelectedProfiles();
    [[nodiscard]] bool isActive() const noexcept;
    void cancel();

  private:
    void setDistance(double distance);
    void choose(const std::string& choice, int index);
    void accept();

    application::CadSession& session_;
    ToolPanelHost& panel_;
    std::function<void()> sceneChanged_;
    std::function<void(const QString&)> statusChanged_;
    std::optional<document::FeatureId> profileSketch_;
    std::optional<application::DocumentStamp> supportStamp_;
    std::vector<document::SketchRegionId> profileIds_;
    double distance_{1.0};
    // T02: result, direction and, for a base-plane sketch, the body a join or cut works on.
    document::ExtrusionOperation operation_{document::ExtrusionOperation::newBody};
    bool reversed_{false};
    bool baseSketch_{false};
    std::vector<document::BodyId> targets_;
    std::optional<document::BodyId> target_;
    bool active_{false};
};

} // namespace voelund::desktop
