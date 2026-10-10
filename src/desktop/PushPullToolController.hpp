#pragma once

#include <voelund/application/SceneStatus.hpp>
#include <voelund/document/SubshapeReference.hpp>
#include <voelund/viewport/CadViewport.hpp>

#include <QString>

#include <functional>
#include <optional>

namespace voelund::application {
class CadSession;
}

namespace voelund::desktop {

class ToolPanelHost;

// T04c (user decision 2026-10-07: "Pfeil ziehen + Maßfeld"): push/pull of a picked planar
// face. The arrow in the viewport and the length field set the same distance; positive pulls
// (joins), negative pushes (cuts). After a drag or a typed value the result is previewed in
// the viewport; OK creates the one extrusion feature, Cancel leaves the model as it was.
class PushPullToolController final {
  public:
    PushPullToolController(application::CadSession& session, viewport::CadViewport& viewport,
                           ToolPanelHost& panel, std::function<void()> sceneChanged,
                           std::function<void(const QString&)> statusChanged);

    void begin(const viewport::PlanarFaceSelection& face);
    [[nodiscard]] bool isActive() const noexcept { return active_; }
    void cancel();

  private:
    void dragged(double distance, bool finished);
    void typed(double distance);
    void preview();
    void accept();
    void finish();

    application::CadSession& session_;
    viewport::CadViewport& viewport_;
    ToolPanelHost& panel_;
    std::function<void()> sceneChanged_;
    std::function<void(const QString&)> statusChanged_;
    document::FeatureId support_{1};
    std::optional<document::SubshapeOrigin> face_;
    std::optional<application::DocumentStamp> stamp_;
    double distance_{10.0};
    bool active_{false};
    // S14: a drag shows the body live; previews are bundled to one per 40 ms.
    bool previewPending_{false};
};

} // namespace voelund::desktop
