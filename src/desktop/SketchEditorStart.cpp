#include "SketchEditorToolController.hpp"
#include "SketchViewAdapter.hpp"
#include "ToolPanelHost.hpp"
#include <QObject>
#include <gp_Dir.hxx>
#include <voelund/application/CadSession.hpp>
#include <voelund/document/SketchPlaneValidation.hpp>

#include <cmath>

namespace voelund::desktop {
namespace {
document::SketchPlane sketchPlane(const viewport::PlanarFaceSelection& support) {
    return {support.originX,     support.originY,     support.originZ,
            support.normalX,     support.normalY,     support.normalZ,
            support.xDirectionX, support.xDirectionY, support.xDirectionZ};
}

// S03: a new sketch on a face stands upright: its y axis follows world Z, or world Y on a
// nearly horizontal face, so the editor shows the face the way the model stands.
document::SketchPlane uprightSketchPlane(const viewport::PlanarFaceSelection& support) {
    const gp_Dir normal(support.normalX, support.normalY, support.normalZ);
    const gp_Dir preferredUp = std::abs(normal.Z()) < 0.95 ? gp_Dir(0.0, 0.0, 1.0)
                                                           : gp_Dir(0.0, 1.0, 0.0);
    const gp_Dir x = preferredUp.Crossed(normal); // y = normal x x lies along preferredUp
    return {support.originX, support.originY, support.originZ, normal.X(), normal.Y(),
            normal.Z(),      x.X(),           x.Y(),           x.Z()};
}

viewport::PlanarFaceSelection drawingFrame(std::uint64_t bodyId, const document::SketchPlane& plane) {
    return {bodyId,
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

// The drawing frame of a base plane; bodyId 0 because no body supports it.
viewport::PlanarFaceSelection basePlaneSupport(document::DatumPlane datum) {
    return drawingFrame(0, document::datumSketchPlane(datum));
}

} // namespace

// S03: every start (new on a plane or face, editing, recovery) opens the editor here, so
// orientation, return camera and panel follow one path.
void SketchEditorToolController::open(std::unique_ptr<application::SketchEditorSession> editor,
                                      const viewport::PlanarFaceSelection& drawing,
                                      std::optional<document::FeatureId> edited,
                                      std::optional<document::FeatureId> support) {
    editing_ = edited.has_value();
    draftEdited_ = edited;
    draftSupport_ = support;
    editor_ = std::move(editor);
    projecting_ = false;
    showPanel();
    panel_.setAcceptEnabled(editor_->canAccept());
    viewport_.beginSketchEditor(drawing, viewport::SketchElementTool::line);
    refreshPresentation();
    statusChanged_(editing_ ? QObject::tr("Vorhandene Skizze wird bearbeitet")
                            : QObject::tr("Skizzeneditor: zwei Punkte für eine Linie setzen"));
}

void SketchEditorToolController::begin(const viewport::PlanarFaceSelection& support) {
    std::optional<document::SketchSupportBinding> binding;
    const auto plane = uprightSketchPlane(support);
    auto drawingSupport = support;
    try {
        binding = session_.bindSelectedSupport(document::FeatureId{support.bodyId},
                                               support.selectedFace, plane);
        setSupportProjection(drawingSupport, session_.projectSketchSupport(
                                                 document::FeatureId{support.bodyId}, *binding));
    } catch (const std::exception& error) {
        statusChanged_(QString::fromUtf8(error.what()));
        return;
    }
    startOnSupport(drawingSupport, document::FeatureId{support.bodyId}, plane, *binding);
}

void SketchEditorToolController::startOnSupport(const viewport::PlanarFaceSelection& drawing,
                                                document::FeatureId support,
                                                const document::SketchPlane& plane,
                                                const document::SketchSupportBinding& binding) {
    open(std::make_unique<application::SketchEditorSession>(session_, support, plane, binding),
         drawing, std::nullopt, support);
}

void SketchEditorToolController::beginOnBasePlane(document::DatumPlane datum) {
    open(std::make_unique<application::SketchEditorSession>(session_, datum),
         basePlaneSupport(datum), std::nullopt, std::nullopt);
}

void SketchEditorToolController::beginOnConstructionPlane(document::FeatureId plane) {
    const auto frame = session_.datumFrame(plane);
    if (!frame)
        return;
    open(std::make_unique<application::SketchEditorSession>(session_, *frame, plane),
         drawingFrame(0, *frame), std::nullopt, std::nullopt);
}

bool SketchEditorToolController::beginSelectedSketch() {
    const auto selected = session_.selectedFeature();
    if (!selected.has_value())
        return false;
    const document::FeatureDescriptor& descriptor = session_.selectedFeatureDescriptor();
    if (descriptor.kind != document::FeatureKind::sketch)
        return false;
    document::SketchParameters parameters = session_.sketchParameters(*selected);
    if (!descriptor.inputFeature.has_value() && parameters.constructionPlane) {
        // M19: the plane may have moved since the sketch was stored.
        const auto frame = session_.datumFrame(*parameters.constructionPlane);
        if (!frame)
            return false;
        parameters.plane = *frame;
        open(std::make_unique<application::SketchEditorSession>(session_, *selected, parameters),
             drawingFrame(0, *frame), *selected, std::nullopt);
        return true;
    }
    if (!descriptor.inputFeature.has_value()) {
        if (!parameters.datum)
            return false;
        const auto datum = *parameters.datum;
        open(std::make_unique<application::SketchEditorSession>(session_, *selected, parameters),
             basePlaneSupport(datum), *selected, std::nullopt);
        return true;
    }
    try {
        parameters = session_.sketchParametersForEditing(*selected);
        document::validateCanonicalSketchPlane(parameters.plane);
    } catch (const std::exception& error) {
        statusChanged_(QString::fromUtf8(error.what()));
        return false;
    }
    const auto persistedSupport = drawingFrame(descriptor.inputFeature->value(), parameters.plane);
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
    open(std::make_unique<application::SketchEditorSession>(session_, *selected,
                                                            *descriptor.inputFeature, parameters),
         support, *selected, *descriptor.inputFeature);
    return true;
}

} // namespace voelund::desktop
