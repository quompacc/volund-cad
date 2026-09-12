#include "MainWindowSupport.hpp"

#include <voelund/application/CadSession.hpp>

#include <algorithm>

namespace voelund::desktop {

std::vector<viewport::SceneBody> makeSceneBodies(const application::CadSession& session) {
    std::vector<viewport::SceneBody> bodies;
    bodies.reserve(session.currentBodies().size());
    for (const features::EvaluatedBody& body : session.currentBodies()) {
        bodies.push_back({body.sourceFeature.value(), body.shape,
                          session.canTransformFeature(body.sourceFeature)});
    }
    return bodies;
}

QString featureErrorText(
    const application::CadSession& session, document::FeatureId featureId) {
    for (const application::FeatureRuntimeState& state : session.featureStates()) {
        if (state.id == featureId && state.status == application::FeatureRuntimeStatus::error) {
            return QString::fromStdString(state.message);
        }
    }
    return {};
}

int resultModeIndex(document::CylinderResultMode mode) {
    switch (mode) {
    case document::CylinderResultMode::join: return 0;
    case document::CylinderResultMode::cut: return 1;
    case document::CylinderResultMode::intersect: return 2;
    case document::CylinderResultMode::separate: return 3;
    }
    return 0;
}

document::CylinderResultMode resultModeFromIndex(int index) {
    switch (index) {
    case 1: return document::CylinderResultMode::cut;
    case 2: return document::CylinderResultMode::intersect;
    case 3: return document::CylinderResultMode::separate;
    default: return document::CylinderResultMode::join;
    }
}

std::optional<document::FeatureId> latestCylinderId(
    const application::CadSession& session) {
    const auto features = session.features();
    const auto cylinder = std::ranges::find(
        features.rbegin(), features.rend(), document::FeatureKind::cylinder,
        &document::FeatureDescriptor::kind);
    return cylinder == features.rend() ? std::optional<document::FeatureId>{}
                                      : std::optional<document::FeatureId>{cylinder->id};
}

bool selectTransformFeatureForBody(
    application::CadSession& session, document::FeatureId bodyId) {
    const auto selected = session.selectedFeature();
    if (selected.has_value() && session.canTransformFeature(*selected)
        && session.transformBodyForFeature(*selected) == bodyId) {
        return true;
    }
    return session.selectFeature(bodyId) && session.canTransformFeature(bodyId);
}

} // namespace voelund::desktop
