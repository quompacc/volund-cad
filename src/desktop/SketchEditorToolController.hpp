#pragma once

#include <voelund/application/SketchEditorSession.hpp>
#include <voelund/viewport/CadViewport.hpp>

#include <functional>
#include <memory>
#include <optional>

namespace voelund::application {
class CadSession;
}

namespace voelund::desktop {

class ToolPanelHost;

class SketchEditorToolController final {
  public:
    SketchEditorToolController(application::CadSession& session, viewport::CadViewport& viewport,
                               ToolPanelHost& panel, std::function<void()> sceneChanged,
                               std::function<void(const QString&)> statusChanged);

    void begin(const viewport::PlanarFaceSelection& support);
    [[nodiscard]] bool beginSelectedSketch();
    void updatePreview(const std::optional<viewport::SketchElementIntent>& intent);
    void confirmElement(const viewport::SketchElementIntent& intent);
    void translateFromViewport(double deltaX, double deltaY);
    [[nodiscard]] bool selectElement(const viewport::SketchSelectionIntent& intent);
    void editConstraint(std::uint64_t constraintId);
    [[nodiscard]] bool isActive() const noexcept;
    void cancel();

  private:
    void setTool(int index);
    void applyTranslation(const std::string& id, double value);
    void applyElementDimension(const std::string& id, double value);
    void applyLineConstraint(document::SketchLineConstraint constraint);
    void applyRelation(document::SketchConstraintKind kind);
    void toggleConstruction();
    void lockSelectedReference();
    void unlockSelectedReference();
    void removeSelected();
    void removeLastConstraint();
    void resetTranslationInputs();
    void syncSelectedDimensionInputs();
    void syncConstraintState();
    void showPanel();
    void refreshPresentation();
    void accept();

    application::CadSession& session_;
    viewport::CadViewport& viewport_;
    ToolPanelHost& panel_;
    std::function<void()> sceneChanged_;
    std::function<void(const QString&)> statusChanged_;
    std::unique_ptr<application::SketchEditorSession> editor_;
    double translationX_{0.0};
    double translationY_{0.0};
    bool editing_{false};
};

} // namespace voelund::desktop
