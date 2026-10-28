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

// M08 (user decision 2026-10-13): a hole at a clicked point of a planar body face. The panel
// holds X/Y in the face's frame, the diameter, and the depth or "Durchgehend"; a click on a
// planar face while it is open moves the hole there. Preview and OK run the same action;
// Bearbeiten and the timeline reopen a hole. N09: kind (simple, counterbore, countersink), a
// 118° drill point and an ISO metric thread (the hole then takes the thread's core drill).
class HoleToolController final {
  public:
    HoleToolController(application::CadSession& session, ToolPanelHost& panel,
                       std::function<void()> sceneChanged,
                       std::function<void(const QString&)> statusChanged);

    void begin(const viewport::PlanarFaceSelection& face);
    void beginEdit(document::FeatureId hole);
    // N09b: holes at all points of a sketch on a body face; the panel has no X/Y then.
    void beginAtSketch(document::FeatureId sketch);
    [[nodiscard]] bool isActive() const noexcept { return active_; }
    // A planar face picked while the panel is open moves the hole to the click point.
    void place(const viewport::PlanarFaceSelection& face);
    void cancel();
    void
    setPreviewHandler(std::function<void(const std::vector<features::EvaluatedBody>*)> handler);

  private:
    // The face's frame coordinates of a model point on it; none for a face not offered.
    [[nodiscard]] std::optional<std::array<double, 2>>
    frameCoordinates(document::FeatureId support, const document::SubshapeOrigin& face,
                     const std::array<double, 3>& point) const;
    void open();
    void changed(const std::string& id, double value);
    void chosen(const std::string& id, int index);
    void showVariantFields();
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
    document::HoleVariant variant_{};           // N09
    std::optional<document::FeatureId> sketch_; // N09b: holes at its points
    std::optional<document::FeatureId> editing_;
    // M13: set while an edit is kept with errors at later features.
    std::optional<application::ErrorDisposition> keep_;
    std::optional<document::HoleParameters> stored_;
    std::optional<application::DocumentStamp> stamp_;
    bool active_{false};
    bool previewShown_{false};
    // The shown preview and its feature: a face clicked on the previewed body is named there.
    std::shared_ptr<const application::CadSession> previewSession_;
    std::optional<document::FeatureId> previewFeature_;
};

} // namespace voelund::desktop
