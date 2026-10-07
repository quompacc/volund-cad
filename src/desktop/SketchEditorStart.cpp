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
    startOnSupport(drawingSupport, document::FeatureId{support.bodyId}, sketchPlane(support),
                   *binding);
}

void SketchEditorToolController::startOnSupport(const viewport::PlanarFaceSelection& drawing,
                                                document::FeatureId support,
                                                const document::SketchPlane& plane,
                                                const document::SketchSupportBinding& binding) {
    editing_ = false;
    draftEdited_.reset();
    draftSupport_ = support;
    editor_ = std::make_unique<application::SketchEditorSession>(session_, support, plane, binding);
    showPanel();
    panel_.setAcceptEnabled(editor_->canAccept());
    viewport_.beginSketchEditor(drawing, viewport::SketchElementTool::line);
    refreshPresentation();
    statusChanged_(QObject::tr("Skizzeneditor: zwei Punkte für eine Linie setzen"));
}

// The drawing frame of a base plane; bodyId 0 because no body supports it.
viewport::PlanarFaceSelection basePlaneSupport(document::DatumPlane datum) {
    const auto plane = document::datumSketchPlane(datum);
    return {0,
            plane.originX,
            plane.originY,
            plane.originZ,
            plane.normalX,
            plane.normalY,
            plane.normalZ,
            plane.xDirectionX,
            plane.xDirectionY,
            plane.xDirectionZ};
}

void SketchEditorToolController::beginOnBasePlane(document::DatumPlane datum) {
    editing_ = false;
    draftEdited_.reset();
    draftSupport_.reset();
    editor_ = std::make_unique<application::SketchEditorSession>(session_, datum);
    showPanel();
    panel_.setAcceptEnabled(editor_->canAccept());
    viewport_.beginSketchEditor(basePlaneSupport(datum), viewport::SketchElementTool::line);
    refreshPresentation();
    statusChanged_(QObject::tr("Skizzeneditor: zwei Punkte für eine Linie setzen"));
}

bool SketchEditorToolController::beginSelectedSketch() {
    const auto selected = session_.selectedFeature();
    if (!selected.has_value())
        return false;
    const document::FeatureDescriptor& descriptor = session_.selectedFeatureDescriptor();
    if (descriptor.kind != document::FeatureKind::sketch)
        return false;
    document::SketchParameters parameters = session_.sketchParameters(*selected);
    if (!descriptor.inputFeature.has_value()) {
        if (!parameters.datum)
            return false;
        editing_ = true;
        draftEdited_ = *selected;
        draftSupport_.reset();
        editor_ =
            std::make_unique<application::SketchEditorSession>(session_, *selected, parameters);
        showPanel();
        panel_.setAcceptEnabled(editor_->canAccept());
        viewport_.beginSketchEditor(basePlaneSupport(*parameters.datum),
                                    viewport::SketchElementTool::line);
        refreshPresentation();
        statusChanged_(QObject::tr("Vorhandene Skizze wird bearbeitet"));
        return true;
    }
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
    draftEdited_ = *selected;
    draftSupport_ = *descriptor.inputFeature;
    editor_ = std::make_unique<application::SketchEditorSession>(
        session_, *selected, *descriptor.inputFeature, parameters);
    showPanel();
    panel_.setAcceptEnabled(editor_->canAccept());
    viewport_.beginSketchEditor(support, viewport::SketchElementTool::line);
    refreshPresentation();
    statusChanged_(QObject::tr("Vorhandene Skizze wird bearbeitet"));
    return true;
}

} // namespace voelund::desktop
