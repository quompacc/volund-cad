// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "ActionDetail.hpp"

#include <voelund/features/SketchRegionIdentity.hpp>

#include <algorithm>
#include <utility>

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
namespace {
// The actions that edit a feature of its kind (N12a: a table for the kinds with one). A pattern
// copy, an import and its parts and a split part have none of their own.
std::vector<ActionKind> editActions(const document::DocumentSnapshot& snapshot,
                                    const document::FeatureDescriptor& feature) {
    static constexpr std::pair<FeatureKind, ActionKind> single[]{
        {FeatureKind::fillet, ActionKind::editFilletParameters},
        {FeatureKind::chamfer, ActionKind::editChamferParameters},
        {FeatureKind::hole, ActionKind::editHoleParameters},
        {FeatureKind::combine, ActionKind::editCombineParameters},
        {FeatureKind::datumPlane, ActionKind::editDatumPlaneParameters},
        {FeatureKind::datumAxis, ActionKind::editDatumAxisParameters},
        {FeatureKind::shell, ActionKind::editShellParameters},
        {FeatureKind::primitive, ActionKind::editPrimitiveParameters},
        {FeatureKind::draft, ActionKind::editDraftParameters},
        {FeatureKind::mirror, ActionKind::editMirrorParameters},
        {FeatureKind::pattern, ActionKind::editPatternParameters},
        {FeatureKind::move, ActionKind::editMoveParameters},
        {FeatureKind::split, ActionKind::editSplitParameters}};
    for (const auto& [kind, action] : single)
        if (kind == feature.kind)
            return {action};
    switch (feature.kind) {
    case FeatureKind::box:
        return {ActionKind::setBoxDimensions, ActionKind::editBoxParameters};
    case FeatureKind::cylinder:
        return {ActionKind::setCylinderSize, ActionKind::editCylinderParameters};
    case FeatureKind::sketch: // N09b: holes at its points
        return {ActionKind::editSketch, ActionKind::bindSketchDimension,
                ActionKind::createProfileExtrusion, ActionKind::createHole};
    case FeatureKind::extrusion: {
        std::vector<ActionKind> actions{ActionKind::bindExtrusionDistance,
                                        ActionKind::editExtrusionParameters};
        // A bound distance follows its parameter; it is changed there or after unbinding.
        if (!snapshot.extrusionParameters(feature.id).distanceParameter)
            actions.push_back(ActionKind::setExtrusionDistance);
        return actions;
    }
    default:
        return {};
    }
}
} // namespace

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
    // M19: a construction plane or axis offers its edit and suppression only.
    if (descriptor->kind == FeatureKind::datumPlane || descriptor->kind == FeatureKind::datumAxis)
        return {editActions(snapshot, *descriptor).front(), ActionKind::setFeatureSuppressed};
    std::vector<ActionKind> actions = editActions(snapshot, *descriptor);
    if (session.placementFeature(feature))
        actions.push_back(ActionKind::setPlacement);
    actions.push_back(ActionKind::setFeatureSuppressed); // M14: every feature
    if (session.canTransformFeature(feature))            // M11
        actions.push_back(ActionKind::moveBody);
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
            actions.push_back(ActionKind::createSplit); // N12a
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
