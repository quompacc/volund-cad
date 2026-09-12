#include "SketchBackend.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>
#include <voelund/document/ModelDocument.hpp>
#include <voelund/viewport/CadViewport.hpp>

#include <cmath>

namespace voelund::preview {
namespace {

application::SketchDrawingTool drawingTool(PreviewCanvas::SketchTool tool)
{
    switch (tool) {
    case PreviewCanvas::SketchTool::Line: return application::SketchDrawingTool::line;
    case PreviewCanvas::SketchTool::Rectangle: return application::SketchDrawingTool::rectangle;
    case PreviewCanvas::SketchTool::Circle: return application::SketchDrawingTool::circle;
    case PreviewCanvas::SketchTool::Arc: return application::SketchDrawingTool::arc;
    default: return application::SketchDrawingTool::select;
    }
}

} // namespace

SketchBackend::SketchBackend(application::CadSession& session)
    : session_(session)
{
}

SketchBackend::~SketchBackend() = default;

bool SketchBackend::begin()
{
    const viewport::PlanarFaceSelection top{document::ModelDocument::initialBoxId().value(),
        0.0, 0.0, 20.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0};
    return begin(top);
}

bool SketchBackend::begin(const viewport::PlanarFaceSelection& support)
{
    if (editor_) return true;
    const document::SketchPlane plane{support.originX, support.originY, support.originZ,
        support.normalX, support.normalY, support.normalZ,
        support.xDirectionX, support.xDirectionY, support.xDirectionZ};
    editor_ = std::make_unique<application::SketchEditorSession>(
        session_, document::FeatureId{support.bodyId}, plane);
    lastMessage_ = QStringLiteral("Parametric sketch session started on top face");
    return true;
}

bool SketchBackend::add(PreviewCanvas::SketchTool tool, const QPointF& first, const QPointF& second)
{
    if (!editor_ || drawingTool(tool) == application::SketchDrawingTool::select) {
        lastMessage_ = QStringLiteral("No active creation tool");
        return false;
    }
    editor_->setTool(drawingTool(tool));
    std::optional<document::SketchPoint> third;
    if (tool == PreviewCanvas::SketchTool::Arc) {
        const QPointF radial = second - first;
        third = document::SketchPoint{first.x() - radial.y(), first.y() + radial.x()};
    }
    if (!editor_->setPreview({first.x(), first.y()}, {second.x(), second.y()}, third)) {
        lastMessage_ = QStringLiteral("Geometry is degenerate and was not added");
        return false;
    }
    const auto id = editor_->confirmPreview();
    if (!id.has_value()) {
        lastMessage_ = QStringLiteral("Sketch solver rejected the geometry");
        return false;
    }
    if (tool == PreviewCanvas::SketchTool::Line) {
        if (std::abs(first.y() - second.y()) < 1.0e-9)
            static_cast<void>(editor_->setSelectedLineConstraint(
                document::SketchLineConstraint::horizontal));
        else if (std::abs(first.x() - second.x()) < 1.0e-9)
            static_cast<void>(editor_->setSelectedLineConstraint(
                document::SketchLineConstraint::vertical));
    }
    editor_->commitHistory();
    lastMessage_ = QStringLiteral("Element #%1 stored in the parametric sketch").arg(id->value());
    return true;
}

bool SketchBackend::finish()
{
    if (!editor_ || !editor_->canAccept()) {
        lastMessage_ = QStringLiteral("Add valid geometry before finishing the sketch");
        return false;
    }
    const auto feature = editor_->accept();
    lastMessage_ = QStringLiteral("Sketch feature #%1 committed to document history").arg(feature.value());
    editor_.reset();
    return true;
}

void SketchBackend::cancel()
{
    if (editor_) editor_->cancel();
    editor_.reset();
    lastMessage_ = QStringLiteral("Sketch cancelled");
}

bool SketchBackend::active() const noexcept
{
    return editor_ != nullptr && editor_->isActive();
}

int SketchBackend::degreesOfFreedom() const noexcept
{
    return editor_ ? editor_->lastSolveResult().degreesOfFreedom : 0;
}

QString SketchBackend::lastMessage() const
{
    return lastMessage_;
}

} // namespace voelund::preview
