#include "SketchEditorToolController.hpp"

#include "SketchViewAdapter.hpp"
#include "ToolPanelHost.hpp"

#include <voelund/application/CadSession.hpp>

#include <QObject>

#include <utility>
#include <vector>

namespace voelund::desktop {
namespace {

document::SketchPlane sketchPlane(const viewport::PlanarFaceSelection& support) {
    return {support.originX,     support.originY,     support.originZ,
            support.normalX,     support.normalY,     support.normalZ,
            support.xDirectionX, support.xDirectionY, support.xDirectionZ};
}

application::SketchDrawingTool applicationTool(int index) {
    if (index == 1)
        return application::SketchDrawingTool::rectangle;
    if (index == 2)
        return application::SketchDrawingTool::circle;
    return application::SketchDrawingTool::line;
}

viewport::SketchElementTool viewportTool(int index) {
    if (index == 1)
        return viewport::SketchElementTool::rectangle;
    if (index == 2)
        return viewport::SketchElementTool::circle;
    return viewport::SketchElementTool::line;
}

} // namespace

SketchEditorToolController::SketchEditorToolController(
    application::CadSession& session, viewport::CadViewport& viewport, ToolPanelHost& panel,
    std::function<void()> sceneChanged, std::function<void(const QString&)> statusChanged)
    : session_(session), viewport_(viewport), panel_(panel), sceneChanged_(std::move(sceneChanged)),
      statusChanged_(std::move(statusChanged)) {}

void SketchEditorToolController::begin(const viewport::PlanarFaceSelection& support) {
    editing_ = false;
    editor_ = std::make_unique<application::SketchEditorSession>(
        session_, document::FeatureId{support.bodyId}, sketchPlane(support));
    showPanel();
    panel_.setAcceptEnabled(editor_->canAccept());
    viewport_.beginSketchEditor(support, viewport::SketchElementTool::line);
    refreshPresentation();
    statusChanged_(QObject::tr("Sketch editor: set two points for a line"));
}

bool SketchEditorToolController::beginSelectedSketch() {
    const auto selected = session_.selectedFeature();
    if (!selected.has_value())
        return false;
    const document::FeatureDescriptor& descriptor = session_.selectedFeatureDescriptor();
    if (descriptor.kind != document::FeatureKind::sketch || !descriptor.inputFeature.has_value())
        return false;
    const document::SketchParameters parameters = session_.sketchParameters(*selected);
    editing_ = true;
    editor_ = std::make_unique<application::SketchEditorSession>(
        session_, *selected, *descriptor.inputFeature, parameters);
    const auto& plane = parameters.plane;
    const viewport::PlanarFaceSelection support{descriptor.inputFeature->value(),
                                                plane.originX,
                                                plane.originY,
                                                plane.originZ,
                                                plane.normalX,
                                                plane.normalY,
                                                plane.normalZ,
                                                plane.xDirectionX,
                                                plane.xDirectionY,
                                                plane.xDirectionZ};
    showPanel();
    panel_.setAcceptEnabled(editor_->canAccept());
    viewport_.beginSketchEditor(support, viewport::SketchElementTool::line);
    refreshPresentation();
    statusChanged_(QObject::tr("Editing existing sketch"));
    return true;
}

void SketchEditorToolController::showPanel() {
    translationX_ = 0.0;
    translationY_ = 0.0;
    panel_.begin(ToolPanelDefinition{
        QObject::tr("Sketch Editor"),
        QObject::tr("Choose a tool and set two points. Add any number of elements, then confirm."),
        {{"translation_x", QObject::tr("Selected ΔX"), 0.0, -1'000'000.0, 1'000'000.0},
         {"translation_y", QObject::tr("Selected ΔY"), 0.0, -1'000'000.0, 1'000'000.0},
         {"line_length", QObject::tr("Line length"), 1.0, 0.001, 1'000'000.0}},
        {{"tool",
          QObject::tr("Tool"),
          {QObject::tr("Line"), QObject::tr("Rectangle"), QObject::tr("Circle")},
          0}},
        {{"horizontal", QObject::tr("Constrain line horizontally")},
         {"vertical", QObject::tr("Constrain line vertically")},
         {"free", QObject::tr("Remove line direction constraint")},
         {"delete", QObject::tr("Delete selected element")}},
        [this](const std::string& id, double value) {
            if (id == "line_length")
                applyLineLength(value);
            else
                applyTranslation(id, value);
        },
        [this](const std::string& id, int index) {
            if (id == "tool")
                setTool(index);
        },
        [this](const std::string& id) {
            if (id == "horizontal") {
                applyLineConstraint(document::SketchLineConstraint::horizontal);
            } else if (id == "vertical") {
                applyLineConstraint(document::SketchLineConstraint::vertical);
            } else if (id == "free") {
                applyLineConstraint(document::SketchLineConstraint::none);
            } else if (id == "delete")
                removeSelected();
        },
        [this]() { accept(); },
        [this]() { cancel(); }});
    syncSelectedLineInputs();
}

void SketchEditorToolController::setTool(int index) {
    if (!editor_)
        return;
    editor_->setTool(applicationTool(index));
    viewport_.setSketchEditorTool(viewportTool(index));
    refreshPresentation();
    statusChanged_(QObject::tr("Sketch tool changed"));
}

void SketchEditorToolController::updatePreview(
    const std::optional<viewport::SketchElementIntent>& intent) {
    if (!editor_)
        return;
    if (intent.has_value()) {
        static_cast<void>(editor_->setPreview({intent->firstX, intent->firstY},
                                              {intent->secondX, intent->secondY}));
    } else {
        editor_->clearPreview();
    }
    refreshPresentation();
}

void SketchEditorToolController::confirmElement(const viewport::SketchElementIntent& intent) {
    if (!editor_ ||
        !editor_->setPreview({intent.firstX, intent.firstY}, {intent.secondX, intent.secondY}))
        return;
    const auto id = editor_->confirmPreview();
    panel_.setAcceptEnabled(editor_->canAccept());
    resetTranslationInputs();
    syncSelectedLineInputs();
    refreshPresentation();
    if (id.has_value()) {
        statusChanged_(QObject::tr("Sketch element #%1 added").arg(id->value()));
    }
}

void SketchEditorToolController::translateFromViewport(double deltaX, double deltaY) {
    if (!editor_ || !editor_->translateSelected(deltaX, deltaY))
        return;
    translationX_ += deltaX;
    translationY_ += deltaY;
    panel_.setLengthValue("translation_x", translationX_);
    panel_.setLengthValue("translation_y", translationY_);
    refreshPresentation();
    statusChanged_(QObject::tr("Sketch element moved: ΔX %1 mm, ΔY %2 mm")
                       .arg(translationX_, 0, 'f', 3)
                       .arg(translationY_, 0, 'f', 3));
}

bool SketchEditorToolController::selectElement(std::uint64_t rawId) {
    if (!editor_ || !editor_->selectElement(document::SketchElementId{rawId}))
        return false;
    viewport_.setSelectedSketchElement(rawId);
    resetTranslationInputs();
    syncSelectedLineInputs();
    statusChanged_(QObject::tr("Sketch element #%1 selected").arg(rawId));
    return true;
}

void SketchEditorToolController::applyLineLength(double value) {
    if (!editor_ || !editor_->setSelectedLineLength(value))
        return;
    refreshPresentation();
    statusChanged_(QObject::tr("Line length set to %1 mm").arg(value, 0, 'f', 3));
}

void SketchEditorToolController::applyLineConstraint(document::SketchLineConstraint constraint) {
    if (!editor_ || !editor_->setSelectedLineConstraint(constraint))
        return;
    syncSelectedLineInputs();
    refreshPresentation();
    const char* text = constraint == document::SketchLineConstraint::horizontal
                           ? "Line constrained horizontally"
                       : constraint == document::SketchLineConstraint::vertical
                           ? "Line constrained vertically"
                           : "Line direction constraint removed";
    statusChanged_(QObject::tr(text));
}

void SketchEditorToolController::applyTranslation(const std::string& id, double value) {
    if (!editor_ || !editor_->selectedElement().has_value())
        return;
    double deltaX = 0.0;
    double deltaY = 0.0;
    if (id == "translation_x") {
        deltaX = value - translationX_;
        translationX_ = value;
    } else if (id == "translation_y") {
        deltaY = value - translationY_;
        translationY_ = value;
    } else {
        return;
    }
    if (editor_->translateSelected(deltaX, deltaY))
        refreshPresentation();
}

void SketchEditorToolController::removeSelected() {
    if (!editor_ || !editor_->removeSelected())
        return;
    resetTranslationInputs();
    syncSelectedLineInputs();
    panel_.setAcceptEnabled(editor_->canAccept());
    refreshPresentation();
    statusChanged_(QObject::tr("Sketch element deleted"));
}

void SketchEditorToolController::syncSelectedLineInputs() {
    if (!editor_)
        return;
    const auto length = editor_->selectedLineLength();
    panel_.setLengthEnabled("line_length", length.has_value());
    panel_.setActionEnabled("horizontal", length.has_value());
    panel_.setActionEnabled("vertical", length.has_value());
    panel_.setActionEnabled("free", length.has_value());
    if (length.has_value())
        panel_.setLengthValue("line_length", *length);
}

void SketchEditorToolController::resetTranslationInputs() {
    translationX_ = 0.0;
    translationY_ = 0.0;
    panel_.setLengthValue("translation_x", 0.0);
    panel_.setLengthValue("translation_y", 0.0);
}

void SketchEditorToolController::refreshPresentation() {
    if (!editor_)
        return;
    const auto evaluated = editor_->evaluatedElements();
    std::vector<viewport::SceneSketchElement> elements;
    elements.reserve(evaluated.size());
    for (const features::EvaluatedSketchElement& element : evaluated) {
        elements.push_back({element.id.value(), element.shape});
    }
    const auto selected = editor_->selectedElement();
    viewport_.setSketchElements(elements, selected.has_value()
                                              ? std::optional<std::uint64_t>{selected->value()}
                                              : std::nullopt);
}

void SketchEditorToolController::accept() {
    if (!editor_ || !editor_->canAccept())
        return;
    const bool wasEditing = editing_;
    static_cast<void>(editor_->accept());
    editor_.reset();
    editing_ = false;
    viewport_.cancelSketch();
    panel_.end();
    sceneChanged_();
    statusChanged_(wasEditing ? QObject::tr("Sketch updated") : QObject::tr("Sketch created"));
}

void SketchEditorToolController::cancel() {
    if (editor_)
        editor_->cancel();
    editor_.reset();
    editing_ = false;
    viewport_.cancelSketch();
    const auto selected = session_.selectedSketchElement();
    viewport_.setSketchElements(
        makeSceneSketchElements(session_),
        selected.has_value() ? std::optional<std::uint64_t>{selected->value()} : std::nullopt);
    panel_.end();
    statusChanged_(QObject::tr("Sketch editor cancelled"));
}

bool SketchEditorToolController::isActive() const noexcept { return editor_ != nullptr; }

} // namespace voelund::desktop
