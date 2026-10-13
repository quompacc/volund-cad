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

// M08 (user decision 2026-10-13): a hole at a clicked point of a planar body face. The panel
// holds X/Y in the face's frame, the diameter, and the depth or "Durchgehend"; a click on a
// planar face while it is open moves the hole there. Preview and OK run the same action;
// Bearbeiten and the timeline reopen a hole.
class HoleToolController final {
  public:
    HoleToolController(application::CadSession& session, ToolPanelHost& panel,
                       std::function<void()> sceneChanged,
                       std::function<void(const QString&)> statusChanged);

    void begin(const viewport::PlanarFaceSelection& face);
    void beginEdit(document::FeatureId hole);
    [[nodiscard]] bool isActive() const noexcept { return active_; }
    // A planar face picked while the panel is open moves the hole to the click point.
    void place(const viewport::PlanarFaceSelection& face);
    void cancel();
    void setPreviewHandler(std::function<void(const std::vector<features::EvaluatedBody>*)> handler);

  private:
    // The face's frame coordinates of a model point on it; none for a face not offered.
    [[nodiscard]] std::optional<std::array<double, 2>>
    frameCoordinates(document::FeatureId support, const document::SubshapeOrigin& face,
                     const std::array<double, 3>& point) const;
    void open();
    void preview();
    void showModel();
    [[nodiscard]] application::Action action() const;
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
    std::optional<document::SubshapeOrigin> face_;
    double x_{0.0};
    double y_{0.0};
    double diameter_{6.0};
    double depth_{10.0};
    bool through_{false};
    std::optional<document::FeatureId> editing_;
    std::optional<document::HoleParameters> stored_;
    std::optional<application::DocumentStamp> stamp_;
    bool active_{false};
    bool previewShown_{false};
    // The shown preview and its feature: a face clicked on the previewed body is named there.
    std::shared_ptr<const application::CadSession> previewSession_;
    std::optional<document::FeatureId> previewFeature_;
};

} // namespace voelund::desktop
