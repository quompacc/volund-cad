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

// M09 (user decision 2026-10-15): mirrors the selected body. The panel offers the base planes,
// the construction planes and "Angeklickte Fläche"; a click on a planar face of the model
// makes that face the plane. "Neuer Körper" keeps the original and adds the image as a body,
// "Vereinigen" fuses it into the original. Preview and OK run the same action; Bearbeiten and
// the timeline reopen a mirror.
class MirrorToolController final {
  public:
    MirrorToolController(application::CadSession& session, ToolPanelHost& panel,
                         std::function<void()> sceneChanged,
                         std::function<void(const QString&)> statusChanged);

    // `support` is the body's current result; a picked planar face becomes the plane.
    void begin(document::FeatureId support,
               const std::optional<viewport::PlanarFaceSelection>& face);
    void beginEdit(document::FeatureId mirror);
    [[nodiscard]] bool isActive() const noexcept { return active_; }
    // A planar face picked while the panel is open: the mirror plane.
    void picked(const viewport::PlanarFaceSelection& face);
    void cancel();
    void setPreviewHandler(std::function<void(const std::vector<features::EvaluatedBody>*)> handler);

  private:
    struct PickedFace final {
        document::FeatureId support;
        document::BodyFaceReference reference;
    };
    [[nodiscard]] std::optional<PickedFace> faceOf(const viewport::PlanarFaceSelection& face) const;
    void open();
    void preview();
    void showModel();
    [[nodiscard]] int faceChoice() const noexcept;
    [[nodiscard]] std::optional<document::MirrorPlane> plane() const;
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
    // Base planes first, then these construction planes, then the picked face.
    std::vector<document::FeatureDescriptor> constructionPlanes_;
    int planeIndex_{0};
    std::optional<PickedFace> face_;
    document::MirrorMode mode_{document::MirrorMode::newBody};
    std::optional<document::FeatureId> editing_;
    std::optional<document::MirrorParameters> stored_;
    std::optional<application::DocumentStamp> stamp_;
    bool active_{false};
    bool previewShown_{false};
    std::shared_ptr<const application::CadSession> previewSession_;
    std::optional<document::FeatureId> previewFeature_;
};

} // namespace voelund::desktop
