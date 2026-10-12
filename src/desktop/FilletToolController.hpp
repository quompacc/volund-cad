#pragma once

#include <voelund/application/Actions.hpp>
#include <voelund/application/SceneStatus.hpp>
#include <voelund/document/ModelDocument.hpp>
#include <voelund/features/FeatureGraph.hpp>
#include <voelund/viewport/CadViewport.hpp>

#include <QString>
#include <TopoDS_Shape.hxx>

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace voelund::application {
class CadSession;
}

namespace voelund::desktop {

class ToolPanelHost;

// M06: rounds edges of one body. The panel holds the radius; while it is open the view offers
// edges, and a click on an edge adds it or (on a chosen one) removes it, as regions in the
// extrusion panel (user decision 2026-10-12). The preview uses the same evaluation as OK; a
// refused preview keeps the panel open, names the reason and marks the edges at fault red.
// OK creates or changes the one fillet feature, Cancel leaves the model as it was.
class FilletToolController final {
  public:
    FilletToolController(application::CadSession& session, viewport::CadViewport& viewport,
                         ToolPanelHost& panel, std::function<void()> sceneChanged,
                         std::function<void(const QString&)> statusChanged);

    // On the current result of `support`'s body.
    void begin(document::FeatureId support);
    // Reopens a fillet with its edges and radius; OK is one undo step.
    void beginEdit(document::FeatureId fillet);
    [[nodiscard]] bool isActive() const noexcept { return active_; }
    void cancel();
    // The candidate bodies while the preview is valid; nullptr asks for the model again.
    void setPreviewHandler(std::function<void(const std::vector<features::EvaluatedBody>*)> handler);

  private:
    void open(document::FeatureId support, std::vector<document::FilletEdge> edges,
              double radius);
    void picked(std::uint64_t body, const TopoDS_Shape& edge);
    void preview();
    void highlight(const std::vector<std::uint64_t>& failing);
    void showModel();
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
    std::optional<document::FeatureId> editing_;
    std::optional<document::FilletParameters> stored_;
    std::vector<document::FilletEdge> edges_;
    double radius_{1.0};
    std::optional<application::DocumentStamp> stamp_;
    bool active_{false};
    bool previewShown_{false};
};

} // namespace voelund::desktop
