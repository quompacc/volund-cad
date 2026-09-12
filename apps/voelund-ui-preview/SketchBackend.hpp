#pragma once

#include "PreviewCanvas.hpp"

#include <QPointF>
#include <QString>

#include <memory>
#include <optional>

namespace voelund::application {
class CadSession;
class SketchEditorSession;
}

namespace voelund::document { class FeatureId; }

namespace voelund::preview {

class SketchBackend final {
public:
    explicit SketchBackend(application::CadSession& session);
    ~SketchBackend();

    bool begin();
    bool add(PreviewCanvas::SketchTool tool, const QPointF& first, const QPointF& second);
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
