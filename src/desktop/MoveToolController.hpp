#pragma once

#include <voelund/application/Actions.hpp>
#include <voelund/application/SceneStatus.hpp>
#include <voelund/document/ModelDocument.hpp>
#include <voelund/features/FeatureGraph.hpp>

#include <QString>

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace voelund::application {
class CadSession;
}

namespace voelund::desktop {

class ToolPanelHost;

// M11 (user decision 2026-10-15): a move ("Bewegen") is made by dragging a body; Bearbeiten and
// the timeline open its numbers: shift along X, Y and Z and turn about X, Y and Z (degrees,
// about the stored pivot). The same edit action previews and commits.
class MoveToolController final {
  public:
    MoveToolController(application::CadSession& session, ToolPanelHost& panel,
                       std::function<void()> sceneChanged,
                       std::function<void(const QString&)> statusChanged);

    void beginEdit(document::FeatureId move);
    [[nodiscard]] bool isActive() const noexcept { return active_; }
    void cancel();
    void setPreviewHandler(std::function<void(const std::vector<features::EvaluatedBody>*)> handler);

  private:
    [[nodiscard]] document::MoveParameters parameters() const;
    void preview();
    void showModel();
    void accept();
    void finish();

    application::CadSession& session_;
    ToolPanelHost& panel_;
    std::function<void()> sceneChanged_;
    std::function<void(const QString&)> statusChanged_;
    std::function<void(const std::vector<features::EvaluatedBody>*)> previewChanged_;
    // X, Y, Z in mm, then the turns about X, Y, Z in degrees.
    std::array<double, 6> values_{};
    std::optional<document::FeatureId> editing_;
    // M13: set while an edit is kept with errors at later features.
    std::optional<application::ErrorDisposition> keep_;
    std::optional<document::MoveParameters> stored_;
    std::optional<application::DocumentStamp> stamp_;
    bool active_{false};
    bool previewShown_{false};
};

} // namespace voelund::desktop
