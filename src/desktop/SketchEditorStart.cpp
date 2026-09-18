#include "SketchEditorToolController.hpp"
#include "SketchViewAdapter.hpp"
#include "ToolPanelHost.hpp"
#include <QObject>
#include <voelund/application/CadSession.hpp>
#include <voelund/document/SketchPlaneValidation.hpp>

namespace voelund::desktop {
namespace {
document::SketchPlane sketchPlane(const viewport::PlanarFaceSelection& support) {
    return {support.originX,     support.originY,     support.originZ,
            support.normalX,     support.normalY,     support.normalZ,
            support.xDirectionX, support.xDirectionY, support.xDirectionZ};
}

} // namespace

void SketchEditorToolController::begin(const viewport::PlanarFaceSelection& support) {
    std::optional<document::SketchSupportBinding> binding;
    auto drawingSupport = support;
    try {
        binding = session_.bindSelectedSupport(document::FeatureId{support.bodyId},
                                               support.selectedFace, sketchPlane(support));
        setSupportProjection(drawingSupport, session_.projectSketchSupport(
                                                 document::FeatureId{support.bodyId}, *binding));
    } catch (const std::exception& error) {
        statusChanged_(QString::fromUtf8(error.what()));
        return;
    }
    editing_ = false;
    editor_ = std::make_unique<application::SketchEditorSession>(
        session_, document::FeatureId{support.bodyId}, sketchPlane(support), binding);
    showPanel();
    panel_.setAcceptEnabled(editor_->canAccept());
    viewport_.beginSketchEditor(drawingSupport, viewport::SketchElementTool::line);
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
    document::SketchParameters parameters = session_.sketchParameters(*selected);
    try {
        parameters = session_.sketchParametersForEditing(*selected);
        document::validateCanonicalSketchPlane(parameters.plane);
    } catch (const std::exception& error) {
        statusChanged_(QString::fromUtf8(error.what()));
        return false;
    }
    const auto& persistedPlane = parameters.plane;
    const viewport::PlanarFaceSelection persistedSupport{
        descriptor.inputFeature->value(), persistedPlane.originX,     persistedPlane.originY,
        persistedPlane.originZ,           persistedPlane.normalX,     persistedPlane.normalY,
        persistedPlane.normalZ,           persistedPlane.xDirectionX, persistedPlane.xDirectionY,
        persistedPlane.xDirectionZ};
    viewport::PlanarFaceSelection support = persistedSupport;
    if (const auto shape = session_.featureShape(*descriptor.inputFeature);
        !parameters.supportBinding && shape.has_value()) {
        if (const auto resolved = viewport::resolvePlanarFaceSelection(persistedSupport, *shape);
            resolved.has_value())
            support = *resolved;
    }
    parameters.plane = sketchPlane(support);
    if (parameters.supportBinding) {
        try {
            setSupportProjection(support,
                                 session_.projectSketchSupport(*descriptor.inputFeature,
                                                               *parameters.supportBinding));
        } catch (const std::exception& error) {
            statusChanged_(QString::fromUtf8(error.what()));
            return false;
        }
    }
    editing_ = true;
    editor_ = std::make_unique<application::SketchEditorSession>(
        session_, *selected, *descriptor.inputFeature, parameters);
    showPanel();
    panel_.setAcceptEnabled(editor_->canAccept());
    viewport_.beginSketchEditor(support, viewport::SketchElementTool::line);
    refreshPresentation();
    statusChanged_(QObject::tr("Editing existing sketch"));
    return true;
}

} // namespace voelund::desktop
