#include "ActionDetail.hpp"

#include <voelund/features/SketchRegionIdentity.hpp>

#include <algorithm>

namespace voelund::application {
namespace {
using document::FeatureKind;

SketchRegionsSummary summarizeRegions(document::FeatureId sketch,
                                      const document::SketchParameters& parameters) {
    const auto identities = features::identifySketchRegions(parameters.elements.values);
    SketchRegionsSummary summary{sketch, identities.regions.valid(), {}};
    for (std::size_t index = 0; index < identities.ids.size(); ++index)
        summary.regions.push_back({identities.ids[index], identities.regions.regions[index].area,
                                   identities.ambiguous(index)});
    return summary;
}
} // namespace

ActionKind actionKind(const Action& action) noexcept {
    return static_cast<ActionKind>(action.index());
}

namespace detail {
std::vector<ActionKind> availableActions(const CadSession& session,
                                         const document::DocumentSnapshot& snapshot,
                                         const ActionTarget& target) {
    if (std::holds_alternative<std::monostate>(target))
        return {ActionKind::createParameter,  ActionKind::createDatumSketch,
                ActionKind::createBox,        ActionKind::createSketchExtrusion,
                ActionKind::createDatumPlane, ActionKind::createDatumAxis};
    if (const auto* parameter = std::get_if<document::ParameterId>(&target)) {
        if (document::findParameter(snapshot.parameterTable(), *parameter) == nullptr)
            return {};
        return {ActionKind::setParameterValue, ActionKind::renameParameter};
    }
    const auto feature = std::get<document::FeatureId>(target);
    const auto features = snapshot.features();
    const auto descriptor = std::ranges::find(features, feature, &document::FeatureDescriptor::id);
    if (descriptor == features.end())
        return {};
    std::vector<ActionKind> actions;
    switch (descriptor->kind) {
    case FeatureKind::box:
        actions = {ActionKind::setBoxDimensions, ActionKind::editBoxParameters};
        break;
    case FeatureKind::cylinder:
        actions = {ActionKind::setCylinderSize, ActionKind::editCylinderParameters};
        break;
    case FeatureKind::sketch:
        actions = {ActionKind::editSketch, ActionKind::bindSketchDimension,
                   ActionKind::createProfileExtrusion};
        break;
    case FeatureKind::extrusion:
        actions = {ActionKind::bindExtrusionDistance, ActionKind::editExtrusionParameters};
        // A bound distance follows its parameter; it is changed there or after unbinding.
        if (!snapshot.extrusionParameters(feature).distanceParameter)
            actions.push_back(ActionKind::setExtrusionDistance);
        break;
    case FeatureKind::fillet:
        actions = {ActionKind::editFilletParameters};
        break;
    case FeatureKind::chamfer:
        actions = {ActionKind::editChamferParameters};
        break;
    case FeatureKind::hole:
        actions = {ActionKind::editHoleParameters};
        break;
    case FeatureKind::combine:
        actions = {ActionKind::editCombineParameters};
        break;
    case FeatureKind::datumPlane:
        return {ActionKind::editDatumPlaneParameters};
    case FeatureKind::datumAxis:
        return {ActionKind::editDatumAxisParameters};
    case FeatureKind::shell:
        actions = {ActionKind::editShellParameters};
        break;
    case FeatureKind::primitive:
        actions = {ActionKind::editPrimitiveParameters};
        break;
    case FeatureKind::draft:
        actions = {ActionKind::editDraftParameters};
        break;
    case FeatureKind::mirror:
        actions = {ActionKind::editMirrorParameters};
        break;
    case FeatureKind::pattern:
        actions = {ActionKind::editPatternParameters};
        break;
    case FeatureKind::patternCopy: // edited through its pattern
        break;
    }
    if (session.placementFeature(feature))
        actions.push_back(ActionKind::setPlacement);
    // A current result offers faces to sketch on or to push/pull, and an input for a
    // cylinder.
    if (session.canAddCylinder(feature)) {
        actions.push_back(ActionKind::createCylinder);
        actions.push_back(ActionKind::createPrimitive);
        if (descriptor->kind != FeatureKind::sketch) {
            actions.push_back(ActionKind::createSketch);
            actions.push_back(ActionKind::pushPullFace);
            actions.push_back(ActionKind::createFillet);
            actions.push_back(ActionKind::createChamfer);
            actions.push_back(ActionKind::createHole);
            actions.push_back(ActionKind::createCombine);
            actions.push_back(ActionKind::createDatumPlane);
            actions.push_back(ActionKind::createDatumAxis);
            actions.push_back(ActionKind::createShell);
            actions.push_back(ActionKind::createDraft);
            actions.push_back(ActionKind::createMirror);
            actions.push_back(ActionKind::createPattern);
        }
    }
    return actions;
}
} // namespace detail

ActionContext describeActionContext(const CadSession& session) {
    const auto identified = session.identifiedSnapshot();
    const auto& snapshot = identified.snapshot();
    const auto states = session.featureStates();
    ActionContext context{identified.id(),
                          session.sceneStatus().document,
                          {},
                          {snapshot.features().begin(), snapshot.features().end()},
                          session.modelOutline(),
                          {states.begin(), states.end()},
                          snapshot.parameterTable(),
                          {},
                          {},
                          {}};
    context.available.push_back(
        {std::monostate{}, detail::availableActions(session, snapshot, {})});
    for (const auto& feature : context.features) {
        if (feature.kind == FeatureKind::sketch)
            context.sketchRegions.push_back(
                summarizeRegions(feature.id, snapshot.sketchParameters(feature.id)));
        auto actions = detail::availableActions(session, snapshot, feature.id);
        if (!actions.empty())
            context.available.push_back({feature.id, std::move(actions)});
    }
    for (const auto& body : session.validBodies())
        context.bodyFaces.push_back(
            {body.bodyId, body.sourceFeature, session.supportFaces(body.sourceFeature)});
    for (const auto& parameter : context.parameters.values)
        context.available.push_back(
            {parameter.id, detail::availableActions(session, snapshot, parameter.id)});
    return context;
}

} // namespace voelund::application
