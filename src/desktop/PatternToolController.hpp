// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/application/Actions.hpp>
#include <voelund/application/SceneStatus.hpp>
#include <voelund/document/ModelDocument.hpp>
#include <voelund/features/FeatureGraph.hpp>
#include <voelund/viewport/CadViewport.hpp>

#include <QString>

#include <TopoDS_Shape.hxx>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace voelund::application {
class CadSession;
}

namespace voelund::desktop {

class ToolPanelHost;

// M10 (user decision 2026-10-15): copies of the selected body, linear (count and spacing) or
// circular (count and total angle). The axis is a base axis, a construction axis or a clicked
// edge (straight: its direction, circular: its axis); a picked edge before the tool is taken
// over. "Neue Körper" makes every copy a body of its own, "Vereinigen" fuses them into the
// original. While the panel is open bodies offer only their edges; the chosen one is
// highlighted. Preview and OK run the same action; Bearbeiten and the timeline reopen a pattern.
class PatternToolController final {
  public:
    PatternToolController(application::CadSession& session, viewport::CadViewport& viewport,
                          ToolPanelHost& panel, std::function<void()> sceneChanged,
                          std::function<void(const QString&)> statusChanged);

    // `support` is the body's current result; `edge` an edge picked on `edgeSupport` before.
    void begin(document::FeatureId support, document::PatternKind kind,
               std::optional<document::FeatureId> edgeSupport,
               std::optional<document::FilletEdge> edge);
    void beginEdit(document::FeatureId pattern);
    [[nodiscard]] bool isActive() const noexcept { return active_; }
    // An edge clicked while the panel is open: the direction or axis.
    void picked(std::uint64_t body, const TopoDS_Shape& edge);
    void cancel();
    void
    setPreviewHandler(std::function<void(const std::vector<features::EvaluatedBody>*)> handler);

  private:
    struct PickedEdge final {
        document::FeatureId support;
        document::BodyId body;
        document::FilletEdge edge;
    };
    void open();
    void syncFields();
    void preview();
    void showModel();
    void highlightEdge();
    [[nodiscard]] int edgeChoice() const noexcept;
    [[nodiscard]] bool hasAxis() const noexcept;
    [[nodiscard]] document::PatternParameters parameters(document::BodyId body) const;
    [[nodiscard]] application::Action action() const;
    [[nodiscard]] bool unchanged() const;
    void accept();
    void finish();
    [[nodiscard]] QString guidance() const;

    application::CadSession& session_;
    viewport::CadViewport& viewport_;
    ToolPanelHost& panel_;
    std::function<void()> sceneChanged_;
    std::function<void(const QString&)> statusChanged_;
    std::function<void(const std::vector<features::EvaluatedBody>*)> previewChanged_;
    document::FeatureId support_{1};
    document::PatternKind kind_{document::PatternKind::linear};
    // Base axes first, then these construction axes, then the clicked edge.
    std::vector<document::FeatureDescriptor> datumAxes_;
    int axisIndex_{0};
    std::optional<PickedEdge> edge_;
    bool reversed_{false};
    double count_{3};
    double spacing_{20.0};
    double angleDegrees_{360.0};
    document::PatternMode mode_{document::PatternMode::newBodies};
    std::optional<document::FeatureId> editing_;
    // M13: set while an edit is kept with errors at later features.
    std::optional<application::ErrorDisposition> keep_;
    std::optional<document::PatternParameters> stored_;
    std::optional<application::DocumentStamp> stamp_;
    bool active_{false};
    bool previewShown_{false};
    std::shared_ptr<const application::CadSession> previewSession_;
    std::optional<document::FeatureId> previewFeature_;
};

} // namespace voelund::desktop
