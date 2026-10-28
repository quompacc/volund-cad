// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/application/Actions.hpp>
#include <voelund/application/SceneStatus.hpp>
#include <voelund/document/ModelDocument.hpp>
#include <voelund/features/FeatureGraph.hpp>

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

// N12b (agent decision 2026-10-28, as Fusion's "Rib"): a rib from the selected sketch's one
// straight line. The panel has "Dicke" (symmetric about the sketch plane), "Richtung" (the side
// of the line it grows to until it meets faces) and "Körper" (the body it joins). Preview and
// OK run the same action; Bearbeiten and the timeline reopen a rib.
class RibToolController final {
  public:
    RibToolController(application::CadSession& session, ToolPanelHost& panel,
                      std::function<void()> sceneChanged,
                      std::function<void(const QString&)> statusChanged);

    void begin(document::FeatureId sketch);
    void beginEdit(document::FeatureId rib);
    [[nodiscard]] bool isActive() const noexcept { return active_; }
    void cancel();
    void
    setPreviewHandler(std::function<void(const std::vector<features::EvaluatedBody>*)> handler);

  private:
    void collectBodies();
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
    std::function<void(const std::vector<features::EvaluatedBody>*)> previewChanged_;
    document::FeatureId sketch_{1};
    std::vector<std::pair<QString, document::BodyId>> bodies_;
    int bodyIndex_{0};
    document::RibParameters rib_{};
    std::optional<document::FeatureId> editing_;
    std::optional<document::ExtrusionParameters> stored_;
    std::optional<application::DocumentStamp> stamp_;
    bool active_{false};
    bool previewShown_{false};
};

} // namespace voelund::desktop
