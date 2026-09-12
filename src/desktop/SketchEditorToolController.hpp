#pragma once

#include <voelund/application/SketchEditorSession.hpp>
#include <voelund/viewport/CadViewport.hpp>

#include <functional>
#include <memory>
#include <optional>

namespace voelund::application { class CadSession; }

namespace voelund::desktop {

class ToolPanelHost;

class SketchEditorToolController final {
public:
    SketchEditorToolController(application::CadSession& session,
        viewport::CadViewport& viewport, ToolPanelHost& panel,
        std::function<void()> sceneChanged,
        std::function<void(const QString&)> statusChanged);

    void begin(const viewport::PlanarFaceSelection& support);
    void updatePreview(const std::optional<viewport::SketchElementIntent>& intent);
    void confirmElement(const viewport::SketchElementIntent& intent);
    [[nodiscard]] bool selectElement(std::uint64_t rawId);
    [[nodiscard]] bool isActive() const noexcept;
    void cancel();

private:
    void setTool(int index);
    void refreshPresentation();
    void accept();

    application::CadSession& session_;
    viewport::CadViewport& viewport_;
    ToolPanelHost& panel_;
    std::function<void()> sceneChanged_;
    std::function<void(const QString&)> statusChanged_;
    std::unique_ptr<application::SketchEditorSession> editor_;
};

} // namespace voelund::desktop
