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
#include <memory>
#include <optional>
#include <vector>

namespace voelund::application {
class CadSession;
}

namespace voelund::desktop {

class ToolPanelHost;

// M07: the same panel rounds (fillet) or chamfers the edges.
enum class EdgeTreatment { fillet, chamfer };

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

    // On the current result of `support`'s body, with edges picked before (user request
    // 2026-10-13).
    void begin(document::FeatureId support, EdgeTreatment treatment = EdgeTreatment::fillet,
               std::vector<document::FilletEdge> edges = {});
    // An edge clicked while the panel is open (body 0: a chosen edge over the preview).
    void picked(std::uint64_t body, const TopoDS_Shape& edge);
    // Reopens a fillet or chamfer with its edges and value; OK is one undo step.
    void beginEdit(document::FeatureId feature);
    [[nodiscard]] bool isActive() const noexcept { return active_; }
    void cancel();
    // The candidate bodies while the preview is valid; nullptr asks for the model again.
    void
    setPreviewHandler(std::function<void(const std::vector<features::EvaluatedBody>*)> handler);

  private:
    void open(document::FeatureId support, std::vector<document::FilletEdge> edges, double value);
    void preview();
    void highlight(const std::vector<std::uint64_t>& failing);
    void showModel();
    // User request 2026-10-13: a grip on the first edge, dragged into the corner, sets the
    // radius or distance.
    void placeGrip();
    void dragged(double value, bool finished);
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
    // The values of the feature being edited.
    struct Stored final {
        document::BodyId body;
        std::vector<document::FilletEdge> edges;
        double value;
    };
    EdgeTreatment treatment_{EdgeTreatment::fillet};
    std::optional<document::FeatureId> editing_;
    // M13: set while an edit is kept with errors at later features.
    std::optional<application::ErrorDisposition> keep_;
    std::optional<Stored> stored_;
    std::vector<document::FilletEdge> edges_;
    double value_{1.0}; // radius or distance
    std::optional<application::DocumentStamp> stamp_;
    bool active_{false};
    bool previewShown_{false};
    // The shown preview and its feature: an edge picked on the previewed body is named by
    // the faces there (user report 2026-10-12: a second edge was refused).
    std::shared_ptr<const application::CadSession> previewSession_;
    std::optional<document::FeatureId> previewFeature_;
    std::optional<document::FilletEdge> gripEdge_;
    bool previewPending_{false};
};

} // namespace voelund::desktop
