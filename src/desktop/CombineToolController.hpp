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
#include <utility>
#include <vector>

namespace voelund::application {
class CadSession;
}

namespace voelund::desktop {

class ToolPanelHost;

// M21 (user decision 2026-10-14): combines whole bodies. The selected body is the target; a
// click on another body adds it as a tool, a second click takes it out again. The panel holds
// the operation and whether the tools are kept. Preview and OK run the same action;
// Bearbeiten and the timeline reopen a combination.
class CombineToolController final {
  public:
    CombineToolController(application::CadSession& session, ToolPanelHost& panel,
                          std::function<void()> sceneChanged,
                          std::function<void(const QString&)> statusChanged);

    // `target` is the current result of the target body.
    void begin(document::FeatureId target);
    void beginEdit(document::FeatureId combine);
    [[nodiscard]] bool isActive() const noexcept { return active_; }
    // A body clicked while the panel is open (its current result, possibly the preview's).
    void picked(document::FeatureId body);
    void cancel();
    void
    setPreviewHandler(std::function<void(const std::vector<features::EvaluatedBody>*)> handler);

  private:
    void open();
    void preview();
    void showModel();
    [[nodiscard]] application::Action action() const;
    [[nodiscard]] document::CombineParameters parameters() const;
    [[nodiscard]] bool unchanged() const;
    void accept();
    void finish();
    [[nodiscard]] QString guidance() const;

    application::CadSession& session_;
    ToolPanelHost& panel_;
    std::function<void()> sceneChanged_;
    std::function<void(const QString&)> statusChanged_;
    std::function<void(const std::vector<features::EvaluatedBody>*)> previewChanged_;
    std::optional<document::FeatureId> target_;
    document::BodyId targetBody_{1};
    // The tool bodies and, for a new combination, the result each was picked on.
    std::vector<std::pair<document::BodyId, document::FeatureId>> tools_;
    document::CombineOperation operation_{document::CombineOperation::join};
    bool keepTools_{false};
    std::optional<document::FeatureId> editing_;
    // M13: set while an edit is kept with errors at later features.
    std::optional<application::ErrorDisposition> keep_;
    std::optional<document::CombineParameters> stored_;
    std::optional<application::DocumentStamp> stamp_;
    bool active_{false};
    bool previewShown_{false};
    std::optional<document::FeatureId> previewFeature_;
};

} // namespace voelund::desktop
