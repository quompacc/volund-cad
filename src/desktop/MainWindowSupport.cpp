// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "MainWindowSupport.hpp"

#include <voelund/application/CadSession.hpp>

namespace voelund::desktop {

QString bodyLabel(const application::ModelOutline& outline, document::BodyId body) {
    int number = 1;
    for (const auto& item : outline.bodies) {
        if (item.id == body)
            return item.name.empty() ? QObject::tr("Körper %1").arg(number)
                                     : QString::fromStdString(item.name);
        ++number;
    }
    return QObject::tr("Körper");
}

std::vector<viewport::SceneBody> makeSceneBodies(const application::CadSession& session,
                                                 const std::set<std::uint64_t>& hiddenBodies) {
    std::vector<viewport::SceneBody> bodies;
    bodies.reserve(session.displayBodies().size());
    for (const features::EvaluatedBody& body : session.displayBodies()) {
        if (hiddenBodies.contains(body.bodyId.value()))
            continue;
        bodies.push_back({body.sourceFeature.value(), body.shape,
                          session.canTransformFeature(body.sourceFeature),
                          !session.sceneStatus().isCurrent()});
    }
    return bodies;
}

std::vector<viewport::SceneDatum> makeSceneDatums(const application::CadSession& session) {
    std::vector<viewport::SceneDatum> datums;
    for (const auto& feature : session.features()) {
        const bool axis = feature.kind == document::FeatureKind::datumAxis;
        if (!axis && feature.kind != document::FeatureKind::datumPlane)
            continue;
        if (const auto frame = session.datumFrame(feature.id))
            datums.push_back({feature.id.value(),
                              axis,
                              {frame->originX, frame->originY, frame->originZ},
                              {frame->normalX, frame->normalY, frame->normalZ},
                              {frame->xDirectionX, frame->xDirectionY, frame->xDirectionZ}});
    }
    return datums;
}

int resultModeIndex(document::CylinderResultMode mode) {
    switch (mode) {
    case document::CylinderResultMode::join:
        return 0;
    case document::CylinderResultMode::cut:
        return 1;
    case document::CylinderResultMode::intersect:
        return 2;
    case document::CylinderResultMode::separate:
        return 3;
    }
    return 0;
}

document::CylinderResultMode resultModeFromIndex(int index) {
    switch (index) {
    case 1:
        return document::CylinderResultMode::cut;
    case 2:
        return document::CylinderResultMode::intersect;
    case 3:
        return document::CylinderResultMode::separate;
    default:
        return document::CylinderResultMode::join;
    }
}

bool selectTransformFeatureForBody(application::CadSession& session, document::FeatureId bodyId) {
    const auto selected = session.selectedFeature();
    if (selected.has_value() && session.canTransformFeature(*selected) &&
        session.transformBodyForFeature(*selected) == bodyId) {
        return true;
    }
    return session.selectFeature(bodyId) && session.canTransformFeature(bodyId);
}

} // namespace voelund::desktop
