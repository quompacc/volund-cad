// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/application/Actions.hpp>
#include <voelund/application/SceneStatus.hpp>
#include <voelund/document/ModelDocument.hpp>
#include <voelund/viewport/CadViewport.hpp>

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
struct ToolPanelDefinition;

// N13b (agent decision 2026-10-29, as Fusion's construction tools): three more construction
// elements next to M19's plane and axis.
// - "Mittelebene": halfway between the picked planar face and a second parallel face clicked
//   while the panel is open.
// - "Ebene durch drei Punkte": through three earlier construction points chosen in the panel.
// - "Bezugspunkt": at X/Y/Z, or on the edge picked before (Anfang, Ende, Mitte, Mittelpunkt).
// The preview shows the element; Bearbeiten and the timeline reopen it.
class ConstructionToolController final {
  public:
    using PreviewHandler = std::function<void(const application::CadSession*)>;
    enum class Kind { midplane, planeThroughPoints, point };

    ConstructionToolController(application::CadSession& session, ToolPanelHost& panel,
                               std::function<void()> sceneChanged,
                               std::function<void(const QString&)> statusChanged);

    void beginMidplane(const viewport::PlanarFaceSelection& face);
    void beginPlaneThroughPoints();
    // `support` and `edge`: an edge picked before, else the point lies at coordinates.
    void beginPoint(std::optional<document::FeatureId> support,
                    std::optional<document::FilletEdge> edge);
    void beginEdit(document::FeatureId feature);
    [[nodiscard]] bool isActive() const noexcept { return active_; }
    // A planar face clicked while the panel is open: a midplane's second face.
    void picked(const viewport::PlanarFaceSelection& face);
    void cancel();
    void setPreviewHandler(PreviewHandler handler);

  private:
    struct Face final {
        document::FeatureId support;
        document::SubshapeOrigin origin;
    };
    [[nodiscard]] std::optional<Face> faceOf(const viewport::PlanarFaceSelection& face) const;
    void collectPoints();
    void start(Kind kind);
    void open();
    [[nodiscard]] ToolPanelDefinition definition();
    void preview();
    void showModel();
    [[nodiscard]] std::optional<application::Action> action() const;
    [[nodiscard]] std::optional<application::Action> editAction() const;
    void accept();
    void finish();
    [[nodiscard]] QString guidance() const;

    application::CadSession& session_;
    ToolPanelHost& panel_;
    std::function<void()> sceneChanged_;
    std::function<void(const QString&)> statusChanged_;
    PreviewHandler previewChanged_;
    Kind kind_{Kind::point};
    std::optional<Face> first_;
    std::optional<Face> second_;
    // Earlier construction points by name, and the three chosen (indices into it).
    std::vector<std::pair<QString, document::FeatureId>> points_;
    std::array<int, 3> chosen_{0, 1, 2};
    std::array<double, 3> position_{0.0, 0.0, 0.0};
    std::optional<document::FeatureId> edgeSupport_;
    std::optional<document::FilletEdge> edge_;
    document::EdgeSpot spot_{document::EdgeSpot::start};
    std::optional<document::FeatureId> editing_;
    std::optional<document::FeatureParameters> stored_;
    std::optional<application::DocumentStamp> stamp_;
    bool active_{false};
    bool previewShown_{false};
    std::shared_ptr<const application::CadSession> previewSession_;
};

} // namespace voelund::desktop
