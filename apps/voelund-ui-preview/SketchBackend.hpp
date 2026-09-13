#pragma once

#include "PreviewCanvas.hpp"

#include <voelund/viewport/CadViewport.hpp>

#include <QPointF>
#include <QString>

#include <memory>
#include <optional>
#include <cstdint>
#include <vector>

namespace voelund::application {
class CadSession;
class SketchEditorSession;
}

namespace voelund::document { class FeatureId; }
namespace voelund::viewport { struct PlanarFaceSelection; }

namespace voelund::preview {

class SketchBackend final {
public:
    explicit SketchBackend(application::CadSession& session);
    ~SketchBackend();

    bool begin();
    bool begin(const viewport::PlanarFaceSelection& support);
    bool add(PreviewCanvas::SketchTool tool, const QPointF& first, const QPointF& second);
    void setTool(PreviewCanvas::SketchTool tool);
    void updatePreview(const std::optional<viewport::SketchElementIntent>& intent);
    bool confirmElement(const viewport::SketchElementIntent& intent);
    bool selectElement(const viewport::SketchSelectionIntent& intent);
    bool translateSelected(double deltaX, double deltaY);
    [[nodiscard]] std::vector<viewport::SceneSketchElement> sceneElements() const;
    [[nodiscard]] std::vector<viewport::SceneSketchConstraint> sceneConstraints() const;
    [[nodiscard]] std::vector<std::uint64_t> selectedElementIds() const;
    bool finish();
    void cancel();
    [[nodiscard]] bool active() const noexcept;
    [[nodiscard]] int degreesOfFreedom() const noexcept;
    [[nodiscard]] QString lastMessage() const;

private:
    application::CadSession& session_;
    std::unique_ptr<application::SketchEditorSession> editor_;
    QString lastMessage_;
};

} // namespace voelund::preview
