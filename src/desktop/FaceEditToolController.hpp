// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

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

// N13a (agent decision 2026-10-29, as Fusion's "Offset Face" and "Delete"): the picked planar
// face, and further faces of the body clicked while the panel is open, are moved by "Abstand"
// along their normals or removed with the gap healed ("Art"). A click on a chosen face takes it
// out again. Bearbeiten and the timeline reopen it.
class FaceEditToolController final {
  public:
    FaceEditToolController(application::CadSession& session, ToolPanelHost& panel,
                           std::function<void()> sceneChanged,
                           std::function<void(const QString&)> statusChanged);

    void begin(const viewport::PlanarFaceSelection& face, document::FaceEditMode mode);
    void beginEdit(document::FeatureId edit);
    [[nodiscard]] bool isActive() const noexcept { return active_; }
    // A planar face picked while the panel is open: chosen, or taken out again.
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
    [[nodiscard]] document::FaceEditParameters parameters() const;
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
    std::vector<document::SubshapeOrigin> faces_;
    document::FaceEditMode mode_{document::FaceEditMode::offset};
    double distance_{1.0};
    std::optional<document::FeatureId> editing_;
    std::optional<document::FaceEditParameters> stored_;
    std::optional<application::DocumentStamp> stamp_;
    bool active_{false};
    bool previewShown_{false};
    std::shared_ptr<const application::CadSession> previewSession_;
    std::optional<document::FeatureId> previewFeature_;
};

} // namespace voelund::desktop
