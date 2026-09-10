#pragma once

#include <voelund/viewport/CadViewport.hpp>

#include <functional>
#include <optional>

namespace voelund::application {
class CadSession;
}

namespace voelund::desktop {

class ToolPanelHost;

class SketchExtrusionToolController final {
public:
    SketchExtrusionToolController(
        application::CadSession& session,
        viewport::CadViewport& viewport,
        ToolPanelHost& panel,
        std::function<void()> sceneChanged,
        std::function<void(const QString&)> statusChanged);

    void begin(const viewport::PlanarFaceSelection& support);
    void updateDistancePreview(double distance);
    void setResult(const viewport::SketchRectangleResult& result);
    void cancel();

private:
    void setDistance(double distance);
    void accept();

    application::CadSession& session_;
    viewport::CadViewport& viewport_;
    ToolPanelHost& panel_;
    std::function<void()> sceneChanged_;
    std::function<void(const QString&)> statusChanged_;
    std::optional<viewport::SketchRectangleResult> result_;
    double distance_{1.0};
};

} // namespace voelund::desktop
