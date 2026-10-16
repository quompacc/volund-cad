#pragma once

#include <voelund/application/Actions.hpp>
#include <voelund/application/SceneStatus.hpp>
#include <voelund/document/ModelDocument.hpp>
#include <voelund/viewport/CadViewport.hpp>

#include <QString>
#include <TopoDS_Shape.hxx>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace voelund::application {
class CadSession;
}

namespace voelund::desktop {

class ToolPanelHost;

class SketchExtrusionToolController final {
  public:
    SketchExtrusionToolController(application::CadSession& session, ToolPanelHost& panel,
                                  std::function<void()> sceneChanged,
                                  std::function<void(const QString&)> statusChanged);

    // M20: with `revolve` the regions turn about an axis ("Drehen") instead.
    // M26: with `helix` as well, a helix ("Wendel") about that axis: pitch 5, height 20.
    void beginSelectedProfiles(bool revolve = false, bool helix = false);
    // M23: the regions swept along a path, all drawn elements of an earlier sketch ("Folgen").
    void beginSelectedSweep();
    // M24: the one picked region lofted to the first region of another sketch ("Ausformung").
    void beginSelectedLoft();
    // M04 (user decision 2026-10-12): reopens an existing sketch extrusion with its values;
    // OK changes it as one undo step, Cancel changes nothing.
    void beginEdit(document::FeatureId extrusion);
    [[nodiscard]] bool isActive() const noexcept;
    void cancel();
    // T03: with "Bis Fläche" chosen, a face picked in the viewport becomes the end face.
    [[nodiscard]] bool awaitsEndFace() const noexcept;
    void pickEndFace(document::FeatureId support, const TopoDS_Shape& face);
    // M04 (user decision 2026-10-12): while the panel is open, a click on a region of its
    // sketch toggles that region; the session's region selection mirrors the panel's.
    [[nodiscard]] std::optional<document::FeatureId> regionSketch() const noexcept;
    [[nodiscard]] bool toggleRegionAt(const viewport::ViewRay& ray);
    [[nodiscard]] bool toggleRegionByElement(document::SketchElementId element);
    // M04: elements of the contour an invalid region fails on, marked red in the sketch.
    [[nodiscard]] const std::vector<std::uint64_t>& problemElements() const noexcept;
    // U11 (A-UI-02): the candidate bodies of the current values, shown while the panel is open;
    // nullptr asks for the model itself again.
    void setPreviewHandler(
        std::function<void(const std::vector<features::EvaluatedBody>*)> handler);

  private:
    void setDistance(double distance);
    void regionsToggled();
    void open(document::FeatureId sketch, std::vector<document::SketchRegionId> regions,
              const document::ExtrusionParameters& values);
    [[nodiscard]] application::Action extrusion() const;
    void preview();
    void choose(const std::string& choice, int index);
    // M04: fields that do not apply to the current choices are disabled or hidden.
    void syncFields();
    // M04: what the extrusion does and on which body, or what it waits for.
    [[nodiscard]] QString guidance() const;
    void accept();
    // M20: the axes the panel offers; the first is a line picked in the sketch.
    void collectAxes();
    void collectPaths(); // M23: earlier sketches with drawn elements
    void collectSections(); // M24: other sketches with a region (SketchExtrusionModes.cpp)
    [[nodiscard]] int axisIndex() const;
    [[nodiscard]] bool pickAxisLine(document::SketchElementId element);

    application::CadSession& session_;
    ToolPanelHost& panel_;
    std::function<void()> sceneChanged_;
    std::function<void(const QString&)> statusChanged_;
    std::function<void(const std::vector<features::EvaluatedBody>*)> previewChanged_;
    bool previewShown_{false};
    std::optional<document::FeatureId> profileSketch_;
    std::optional<application::DocumentStamp> supportStamp_;
    std::vector<document::SketchRegionId> profileIds_;
    double distance_{1.0};
    // T02: result, direction and, for a base-plane sketch, the body a join or cut works on.
    document::ExtrusionOperation operation_{document::ExtrusionOperation::newBody};
    bool reversed_{false};
    bool symmetric_{false}; // M02
    // M04: the direction was turned for a cut by the panel, not by the user.
    bool autoReversed_{false};
    // M04: the extrusion being edited, with its stored values; none for a new one.
    std::optional<document::FeatureId> editing_;
    // M13: set while an edit is kept with errors at later features.
    std::optional<application::ErrorDisposition> keep_;
    std::optional<document::ExtrusionParameters> stored_;
    bool baseSketch_{false};
    std::vector<document::BodyId> targets_;
    std::optional<document::BodyId> target_;
    double offset_{0.0};
    bool upToFace_{false};
    std::optional<document::BodyFaceReference> endFace_;
    std::vector<std::uint64_t> problemElements_;
    bool active_{false};
    // M20: the revolve's axis and angle; none for a prism. While `axisPick_` is set, a click
    // on a line of the sketch makes it the axis.
    std::optional<document::RevolveParameters> revolve_;
    std::vector<std::pair<QString, document::RevolveAxis>> axes_;
    std::optional<document::SweepPath> sweep_; // M23
    std::vector<std::pair<QString, document::SweepPath>> paths_;
    std::optional<document::LoftParameters> loft_; // M24
    std::vector<std::pair<QString, document::LoftSection>> sections_;
    bool axisPick_{false};
    double angleDegrees_{360.0};
};

} // namespace voelund::desktop
