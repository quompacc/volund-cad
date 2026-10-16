#pragma once

#include <voelund/application/Actions.hpp>
#include <voelund/application/SceneStatus.hpp>
#include <voelund/document/ModelDocument.hpp>

#include <QString>

#include <functional>
#include <memory>
#include <optional>

namespace voelund::application {
class CadSession;
}

namespace voelund::desktop {

class ToolPanelHost;

// M19 (user decision 2026-10-14): a construction plane on the picked base plane or planar
// face (XY without a pick), moved by an offset and optionally turned about the reference's
// x or y axis; and a construction axis on a base axis (a picked edge creates it at once).
// The preview shows the plane or axis; Bearbeiten and the timeline reopen it.
class DatumToolController final {
  public:
    // A preview session to show the candidate's construction features; null shows the model.
    using PreviewHandler = std::function<void(const application::CadSession*)>;

    DatumToolController(application::CadSession& session, ToolPanelHost& panel,
                        std::function<void()> sceneChanged,
                        std::function<void(const QString&)> statusChanged);

    void beginPlane(std::optional<document::DatumPlane> basePlane,
                    std::optional<document::FeatureId> support,
                    std::optional<document::SubshapeOrigin> face);
    void beginAxis();
    void beginEdit(document::FeatureId datum);
    [[nodiscard]] bool isActive() const noexcept { return active_; }
    void cancel();
    void setPreviewHandler(PreviewHandler handler);

  private:
    void open();
    void preview();
    void showModel();
    [[nodiscard]] application::Action action() const;
    [[nodiscard]] bool unchanged() const;
    void accept();
    void finish();

    application::CadSession& session_;
    ToolPanelHost& panel_;
    std::function<void()> sceneChanged_;
    std::function<void(const QString&)> statusChanged_;
    PreviewHandler previewChanged_;
    bool axis_{false};
    // Plane: the reference and its placement.
    std::optional<document::DatumPlane> basePlane_;
    std::optional<document::FeatureId> support_;
    std::optional<document::SubshapeOrigin> face_;
    std::optional<document::BodyId> faceBody_;
    double offset_{10.0};
    document::DatumRotationAxis rotationAxis_{document::DatumRotationAxis::none};
    double angleDegrees_{0.0};
    // Axis: the base axis.
    document::BaseAxis baseAxis_{document::BaseAxis::z};
    std::optional<document::FeatureId> editing_;
    // M13: set while an edit is kept with errors at later features.
    std::optional<application::ErrorDisposition> keep_;
    std::optional<document::DatumPlaneParameters> storedPlane_;
    std::optional<document::DatumAxisParameters> storedAxis_;
    std::optional<application::DocumentStamp> stamp_;
    bool active_{false};
    bool previewShown_{false};
    std::shared_ptr<const application::CadSession> previewSession_;
};

} // namespace voelund::desktop
