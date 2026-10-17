#pragma once

#include <voelund/application/Actions.hpp>
#include <voelund/application/SceneStatus.hpp>
#include <voelund/document/ModelDocument.hpp>
#include <voelund/features/FeatureGraph.hpp>
#include <voelund/viewport/CadViewport.hpp>

#include <QString>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace voelund::application {
class CadSession;
}

namespace voelund::desktop {

class ToolPanelHost;

// M25 (user decision 2026-10-14): a draft ("Formschräge"). The picked planar face is the
// neutral face; clicks on further planar faces of the body, also on the preview, tilt them
// (or not again). "Neutrale Fläche neu wählen" makes the next click the neutral face. The
// panel holds the angle; Bearbeiten and the timeline reopen it.
class DraftToolController final {
  public:
    DraftToolController(application::CadSession& session, ToolPanelHost& panel,
                        std::function<void()> sceneChanged,
                        std::function<void(const QString&)> statusChanged);

    void begin(const viewport::PlanarFaceSelection& face);
    void beginEdit(document::FeatureId draft);
    [[nodiscard]] bool isActive() const noexcept { return active_; }
    // A planar face picked while the panel is open: tilted (or not again), or the neutral face.
    void picked(const viewport::PlanarFaceSelection& face);
    void cancel();
    void
    setPreviewHandler(std::function<void(const std::vector<features::EvaluatedBody>*)> handler);

  private:
    [[nodiscard]] std::optional<document::SubshapeOrigin>
    faceOrigin(const viewport::PlanarFaceSelection& face) const;
    void open();
    void preview();
    void showModel();
    [[nodiscard]] application::Action action() const;
    [[nodiscard]] document::DraftParameters parameters(document::BodyId body) const;
    [[nodiscard]] bool unchanged() const;
    void accept();
    void finish();
    [[nodiscard]] QString guidance() const;

    application::CadSession& session_;
    ToolPanelHost& panel_;
    std::function<void()> sceneChanged_;
    std::function<void(const QString&)> statusChanged_;
    std::function<void(const std::vector<features::EvaluatedBody>*)> previewChanged_;
    document::FeatureId support_{1};
    std::vector<document::SubshapeOrigin> faces_;
    std::optional<document::SubshapeOrigin> neutral_;
    double angleDegrees_{5.0};
    bool pickNeutral_{false};
    std::optional<document::FeatureId> editing_;
    // M13: set while an edit is kept with errors at later features.
    std::optional<application::ErrorDisposition> keep_;
    std::optional<document::DraftParameters> stored_;
    std::optional<application::DocumentStamp> stamp_;
    bool active_{false};
    bool previewShown_{false};
    std::shared_ptr<const application::CadSession> previewSession_;
    std::optional<document::FeatureId> previewFeature_;
};

} // namespace voelund::desktop
