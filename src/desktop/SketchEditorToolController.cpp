#include "SketchEditorToolController.hpp"

#include "ToolPanelHost.hpp"

#include <voelund/application/CadSession.hpp>

#include <QObject>

#include <utility>
#include <vector>

namespace voelund::desktop {
namespace {

document::SketchPlane sketchPlane(const viewport::PlanarFaceSelection& support) {
    return {support.originX, support.originY, support.originZ,
            support.normalX, support.normalY, support.normalZ,
            support.xDirectionX, support.xDirectionY, support.xDirectionZ};
}

application::SketchDrawingTool applicationTool(int index) {
    if (index == 1) return application::SketchDrawingTool::rectangle;
    if (index == 2) return application::SketchDrawingTool::circle;
    return application::SketchDrawingTool::line;
}

viewport::SketchElementTool viewportTool(int index) {
    if (index == 1) return viewport::SketchElementTool::rectangle;
    if (index == 2) return viewport::SketchElementTool::circle;
    return viewport::SketchElementTool::line;
}

} // namespace

SketchEditorToolController::SketchEditorToolController(
    application::CadSession& session, viewport::CadViewport& viewport,
    ToolPanelHost& panel, std::function<void()> sceneChanged,
    std::function<void(const QString&)> statusChanged)
    : session_(session), viewport_(viewport), panel_(panel),
      sceneChanged_(std::move(sceneChanged)), statusChanged_(std::move(statusChanged)) {}

void SketchEditorToolController::begin(const viewport::PlanarFaceSelection& support) {
    editor_ = std::make_unique<application::SketchEditorSession>(
        session_, document::FeatureId{support.bodyId}, sketchPlane(support));
    panel_.begin(ToolPanelDefinition{
        QObject::tr("Sketch Editor"),
        QObject::tr("Choose a tool and set two points. Add any number of elements, then confirm."),
        {}, {{"tool", QObject::tr("Tool"),
             {QObject::tr("Line"), QObject::tr("Rectangle"), QObject::tr("Circle")}, 0}},
        {}, [this](const std::string& id, int index) {
            if (id == "tool") setTool(index);
        },
        [this]() { accept(); }, [this]() { cancel(); }});
    viewport_.beginSketchEditor(support, viewport::SketchElementTool::line);
    refreshPresentation();
    statusChanged_(QObject::tr("Sketch editor: set two points for a line"));
}

void SketchEditorToolController::setTool(int index) {
    if (!editor_) return;
    editor_->setTool(applicationTool(index));
    viewport_.setSketchEditorTool(viewportTool(index));
    refreshPresentation();
    statusChanged_(QObject::tr("Sketch tool changed"));
}

void SketchEditorToolController::updatePreview(
    const std::optional<viewport::SketchElementIntent>& intent) {
    if (!editor_) return;
    if (intent.has_value()) {
        static_cast<void>(editor_->setPreview(
            {intent->firstX, intent->firstY}, {intent->secondX, intent->secondY}));
    } else {
        editor_->clearPreview();
    }
    refreshPresentation();
}

void SketchEditorToolController::confirmElement(const viewport::SketchElementIntent& intent) {
    if (!editor_ || !editor_->setPreview(
            {intent.firstX, intent.firstY}, {intent.secondX, intent.secondY})) return;
    const auto id = editor_->confirmPreview();
    panel_.setAcceptEnabled(editor_->canAccept());
    refreshPresentation();
    if (id.has_value()) {
        statusChanged_(QObject::tr("Sketch element #%1 added").arg(id->value()));
    }
}

bool SketchEditorToolController::selectElement(std::uint64_t rawId) {
    if (!editor_ || !editor_->selectElement(document::SketchElementId{rawId})) return false;
    viewport_.setSelectedSketchElement(rawId);
    statusChanged_(QObject::tr("Sketch element #%1 selected").arg(rawId));
    return true;
}

void SketchEditorToolController::refreshPresentation() {
    if (!editor_) return;
    const auto evaluated = editor_->evaluatedElements();
    std::vector<viewport::SceneSketchElement> elements;
    elements.reserve(evaluated.size());
    for (const features::EvaluatedSketchElement& element : evaluated) {
        elements.push_back({element.id.value(), element.shape});
    }
    const auto selected = editor_->selectedElement();
    viewport_.setSketchElements(elements, selected.has_value()
        ? std::optional<std::uint64_t>{selected->value()} : std::nullopt);
}

void SketchEditorToolController::accept() {
    if (!editor_ || !editor_->canAccept()) return;
    static_cast<void>(editor_->accept());
    editor_.reset();
    viewport_.cancelSketch();
    panel_.end();
    sceneChanged_();
    statusChanged_(QObject::tr("Sketch created"));
}

void SketchEditorToolController::cancel() {
    if (editor_) editor_->cancel();
    editor_.reset();
    viewport_.cancelSketch();
    viewport_.setSketchElements({});
    panel_.end();
    statusChanged_(QObject::tr("Sketch editor cancelled"));
}

bool SketchEditorToolController::isActive() const noexcept { return editor_ != nullptr; }

} // namespace voelund::desktop
