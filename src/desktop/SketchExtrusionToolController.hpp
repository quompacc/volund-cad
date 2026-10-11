#pragma once

#include <voelund/application/Actions.hpp>
#include <voelund/application/SceneStatus.hpp>
#include <voelund/document/ModelDocument.hpp>

#include <QString>
#include <TopoDS_Shape.hxx>

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
    // T03: with "Bis Fläche" chosen, a face picked in the viewport becomes the end face.
    [[nodiscard]] bool awaitsEndFace() const noexcept;
    void pickEndFace(document::FeatureId support, const TopoDS_Shape& face);
    // U11 (A-UI-02): the candidate bodies of the current values, shown while the panel is open;
    // nullptr asks for the model itself again.
    void setPreviewHandler(
        std::function<void(const std::vector<features::EvaluatedBody>*)> handler);

  private:
    void setDistance(double distance);
    [[nodiscard]] application::Action extrusion() const;
    void preview();
    void choose(const std::string& choice, int index);
    void accept();

    application::CadSession& session_;
    ToolPanelHost& panel_;
    std::function<void()> sceneChanged_;
    std::function<void(const QString&)> statusChanged_;
    std::function<void(const std::vector<features::EvaluatedBody>*)> previewChanged_;
    bool previewShown_{false};
    std::optional<document::FeatureId> profileSketch_;
    std::optional<application::DocumentStamp> supportStamp_;
    std::vector<document::SketchRegionId> profileIds_;
    double distance_{1.0};
    // T02: result, direction and, for a base-plane sketch, the body a join or cut works on.
    document::ExtrusionOperation operation_{document::ExtrusionOperation::newBody};
    bool reversed_{false};
    bool symmetric_{false}; // M02
    bool baseSketch_{false};
    std::vector<document::BodyId> targets_;
    std::optional<document::BodyId> target_;
    double offset_{0.0};
    bool upToFace_{false};
    std::optional<document::BodyFaceReference> endFace_;
    bool active_{false};
};

} // namespace voelund::desktop
