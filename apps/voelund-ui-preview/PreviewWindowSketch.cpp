#include "PreviewWindow.hpp"

#include "PreviewCanvas.hpp"
#include "SketchBackend.hpp"
#include "ViewCube.hpp"

#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QToolButton>

#include <span>

namespace voelund::preview {

void PreviewWindow::enterSketch()
{
    if (!selectedPlanarFace_.has_value()) {
        statusText_->setText(QStringLiteral("Select a planar model face before starting a sketch"));
        return;
    }
    if (!sketchBackend_->begin(*selectedPlanarFace_)) {
        statusText_->setText(sketchBackend_->lastMessage());
        return;
    }
    ribbonStack_->setCurrentIndex(1);
    inspectorStack_->setCurrentIndex(1);
    inspectorStack_->show();
    canvasStack_->setCurrentWidget(cadViewport_);
    viewCube_->hide();
    sketchWorkspaceTab_->show();
    setWorkspaceTab(QStringLiteral("SKETCH"));
    cadViewport_->beginSketchEditor(*selectedPlanarFace_, viewport::SketchElementTool::line);
    if (sketchToolButtons_.size() > 1)
        activateSketchTool(static_cast<int>(PreviewCanvas::SketchTool::Line),
                           sketchToolButtons_.at(1));
    refreshSketchScene();
    statusText_->setText(QStringLiteral("Editing sketch  ·  Line tool active"));
}

void PreviewWindow::finishSketch()
{
    if (sketchBackend_->active() && !sketchBackend_->finish()) {
        statusText_->setText(sketchBackend_->lastMessage());
        return;
    }
    ribbonStack_->setCurrentIndex(0);
    inspectorStack_->setCurrentIndex(0);
    inspectorStack_->hide();
    cadViewport_->cancelSketch();
    cadViewport_->setSketchElements(std::span<const viewport::SceneSketchElement>{});
    cadViewport_->setSketchConstraints(std::span<const viewport::SceneSketchConstraint>{});
    canvasStack_->setCurrentWidget(cadViewport_);
    viewCube_->show();
    sketchWorkspaceTab_->hide();
    setWorkspaceTab(QStringLiteral("SOLID"));
    refreshBrowser();
    refreshScene();
    statusText_->setText(sketchBackend_->lastMessage().isEmpty()
                             ? QStringLiteral("Sketch finished  ·  Ready")
                             : sketchBackend_->lastMessage());
}

void PreviewWindow::activateSketchTool(int tool, QToolButton* button)
{
    for (auto* candidate : sketchToolButtons_) {
        candidate->setProperty("active", candidate == button);
        refreshStyle(candidate);
    }
    const auto sketchTool = static_cast<PreviewCanvas::SketchTool>(tool);
    sketchBackend_->setTool(sketchTool);
    viewport::SketchElementTool viewportTool = viewport::SketchElementTool::select;
    if (sketchTool == PreviewCanvas::SketchTool::Line)
        viewportTool = viewport::SketchElementTool::line;
    else if (sketchTool == PreviewCanvas::SketchTool::Polyline)
        viewportTool = viewport::SketchElementTool::polyline;
    else if (sketchTool == PreviewCanvas::SketchTool::Rectangle)
        viewportTool = viewport::SketchElementTool::rectangle;
    else if (sketchTool == PreviewCanvas::SketchTool::Circle)
        viewportTool = viewport::SketchElementTool::circle;
    else if (sketchTool == PreviewCanvas::SketchTool::Arc)
        viewportTool = viewport::SketchElementTool::arc;
    else if (sketchTool == PreviewCanvas::SketchTool::ThreePointArc)
        viewportTool = viewport::SketchElementTool::threePointArc;
    else if (sketchTool == PreviewCanvas::SketchTool::Ellipse)
        viewportTool = viewport::SketchElementTool::ellipse;
    else if (sketchTool == PreviewCanvas::SketchTool::Slot)
        viewportTool = viewport::SketchElementTool::slot;
    else if (sketchTool == PreviewCanvas::SketchTool::Polygon)
        viewportTool = viewport::SketchElementTool::polygon;
    cadViewport_->setSketchEditorTool(viewportTool);
    refreshSketchScene();
}

void PreviewWindow::refreshSketchScene()
{
    if (!sketchBackend_->active()) return;
    cadViewport_->setSketchElements(sketchBackend_->sceneElements());
    cadViewport_->setSelectedSketchElements(sketchBackend_->selectedElementIds());
    cadViewport_->setSketchConstraints(sketchBackend_->sceneConstraints());
    const int freedom = sketchBackend_->degreesOfFreedom();
    sketchStateLabel_->setText(QStringLiteral("●  Under-constrained  ·  %1 DOF").arg(freedom));
}

} // namespace voelund::preview
