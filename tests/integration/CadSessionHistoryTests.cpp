#include "CadSessionTestSupport.hpp"
#include "CadSessionTestCases.hpp"

namespace voelund::test {

bool recomputeTracksDocumentHistory() {
    voelund::application::CadSession session;
    const double initialVolume = volumeOf(session.currentShape());
    constexpr voelund::kernel::BoxDimensions dimensions{80.0, 25.0, 15.0};

    if (!session.setSelectedBoxDimensions(dimensions)) {
        return false;
    }
    const double changedVolume = volumeOf(session.currentShape());
    const double expectedVolume = 30'000.0 + std::numbers::pi * 64.0 * 23.0;
    if (std::abs(changedVolume - expectedVolume) > 1.0e-6 || !session.undo()) {
        return false;
    }
    if (std::abs(volumeOf(session.currentShape()) - initialVolume) > 1.0e-9) {
        return false;
    }
    return session.redo() && std::abs(volumeOf(session.currentShape()) - changedVolume) < 1.0e-9;
}

bool selectionUsesStableFeatureIdentity() {
    voelund::application::CadSession session;
    const auto features = session.features();
    if (features.size() != 2 || session.selectedFeature() != features.front().id) {
        return false;
    }
    constexpr voelund::document::FeatureId unknownId{999};
    return !session.selectFeature(unknownId) && session.selectedFeature() == features.front().id;
}

bool cylinderCanProduceSeparateBodies() {
    voelund::application::CadSession session;
    const voelund::document::FeatureId cylinderId =
        voelund::document::ModelDocument::initialCylinderId();
    voelund::document::CylinderParameters parameters = session.cylinderParameters(cylinderId);
    parameters.resultMode = voelund::document::CylinderResultMode::separate;
    if (!session.setCylinderParameters(cylinderId, parameters)) {
        return false;
    }
    const double expectedVolume = 24'000.0 + std::numbers::pi * 64.0 * 25.0;
    return session.currentBodies().size() == 2 &&
           session.currentBodies()[0].sourceFeature ==
               voelund::document::ModelDocument::initialBoxId() &&
           session.currentBodies()[1].sourceFeature == cylinderId &&
           session.selectedFeature() == voelund::document::ModelDocument::initialBoxId() &&
           std::abs(volumeOf(session.currentShape()) - expectedVolume) < 1.0e-6;
}

bool separateCylinderMovesIndependentlyAndSupportsUndo() {
    voelund::application::CadSession session;
    if (!session.selectFeature(voelund::document::ModelDocument::initialCylinderId())) {
        return false;
    }
    voelund::document::CylinderParameters parameters = session.selectedCylinderParameters();
    parameters.resultMode = voelund::document::CylinderResultMode::separate;
    if (!session.setSelectedCylinderParameters(parameters)) {
        return false;
    }
    if (!session.canTransformFeature(voelund::document::ModelDocument::initialCylinderId()) ||
        !session.canTransformFeature(voelund::document::ModelDocument::initialBoxId())) {
        return false;
    }

    const double boxMaximumX = maximumXOf(session.currentBodies()[0].shape);
    const double cylinderMaximumX = maximumXOf(session.currentBodies()[1].shape);
    parameters.placement.translationX = 40.0;
    if (!session.setSelectedCylinderParameters(parameters)) {
        return false;
    }
    if (std::abs(maximumXOf(session.currentBodies()[0].shape) - boxMaximumX) > 1.0e-9 ||
        std::abs(maximumXOf(session.currentBodies()[1].shape) - (cylinderMaximumX + 40.0)) >
            1.0e-9) {
        return false;
    }
    return session.undo() && session.selectedCylinderParameters().placement.translationX == 0.0 &&
           std::abs(maximumXOf(session.currentBodies()[1].shape) - cylinderMaximumX) < 1.0e-9;
}

bool separateCylinderDoesNotFollowItsFormerInput() {
    voelund::application::CadSession session;
    if (!session.selectFeature(voelund::document::ModelDocument::initialBoxId())) {
        return false;
    }
    voelund::document::BoxParameters box = session.selectedBoxParameters();
    box.placement.translationX = 25.0;
    if (!session.setSelectedBoxParameters(box) ||
        !session.selectFeature(voelund::document::ModelDocument::initialCylinderId())) {
        return false;
    }

    voelund::document::CylinderParameters cylinder = session.selectedCylinderParameters();
    cylinder.resultMode = voelund::document::CylinderResultMode::separate;
    if (!session.setSelectedCylinderParameters(cylinder) || session.currentBodies().size() != 2) {
        return false;
    }
    const double detachedBoxMaximumX = maximumXOf(session.currentBodies()[0].shape);
    const double detachedCylinderMaximumX = maximumXOf(session.currentBodies()[1].shape);

    if (!session.selectFeature(voelund::document::ModelDocument::initialBoxId())) {
        return false;
    }
    box = session.selectedBoxParameters();
    box.placement.translationX = 45.0;
    if (!session.setSelectedBoxParameters(box)) {
        return false;
    }
    return std::abs(maximumXOf(session.currentBodies()[0].shape) - (detachedBoxMaximumX + 20.0)) <
               1.0e-9 &&
           std::abs(maximumXOf(session.currentBodies()[1].shape) - detachedCylinderMaximumX) <
               1.0e-9;
}

bool joinedResultMovesAsOneBodyAndSupportsUndo() {
    voelund::application::CadSession session;
    const voelund::document::FeatureId boxId = voelund::document::ModelDocument::initialBoxId();
    const voelund::document::FeatureId cylinderId =
        voelund::document::ModelDocument::initialCylinderId();
    if (!session.canTransformFeature(boxId) || !session.canTransformFeature(cylinderId) ||
        session.transformBodyForFeature(boxId) != cylinderId ||
        session.transformBodyForFeature(cylinderId) != cylinderId ||
        !session.selectFeature(boxId)) {
        return false;
    }
    const double initialMaximumX = maximumXOf(session.currentShape());
    const double initialVolume = volumeOf(session.currentShape());
    voelund::kernel::RigidTransform placement = session.selectedFeaturePlacement();
    placement.translationX = 35.0;
    if (!session.setSelectedFeaturePlacement(placement) ||
        std::abs(maximumXOf(session.currentShape()) - (initialMaximumX + 35.0)) > 1.0e-9 ||
        std::abs(volumeOf(session.currentShape()) - initialVolume) > 1.0e-6 ||
        session.cylinderParameters(cylinderId).placement.translationX != 0.0 ||
        session.selectedFeature() != boxId) {
        return false;
    }
    if (session.selectedBoxParameters().placement.translationX != 35.0 || !session.undo()) {
        return false;
    }
    return session.selectedBoxParameters().placement.translationX == 0.0 &&
           std::abs(maximumXOf(session.currentShape()) - initialMaximumX) < 1.0e-9;
}

bool disconnectedJoinReportsAnErrorAndKeepsLastValidShape() {
    voelund::application::CadSession session;
    if (!session.selectFeature(voelund::document::ModelDocument::initialCylinderId())) {
        return false;
    }
    const double validVolume = volumeOf(session.currentShape());
    voelund::document::CylinderParameters parameters = session.selectedCylinderParameters();
    parameters.placement.translationZ = 10.0;
    if (!session.setSelectedCylinderParameters(parameters)) {
        return false;
    }
    const auto states = session.featureStates();
    return states.back().status == voelund::application::FeatureRuntimeStatus::error &&
           states.back().message.find("intersect") != std::string::npos &&
           std::abs(volumeOf(session.currentShape()) - validVolume) < 1.0e-9;
}

bool separateCylinderRotationIsParametricAndUndoable() {
    voelund::application::CadSession session;
    if (!session.selectFeature(voelund::document::ModelDocument::initialCylinderId())) {
        return false;
    }
    voelund::document::CylinderParameters parameters = session.selectedCylinderParameters();
    parameters.resultMode = voelund::document::CylinderResultMode::separate;
    if (!session.setSelectedCylinderParameters(parameters)) {
        return false;
    }
    const double initialMaximumX = maximumXOf(session.currentBodies()[1].shape);
    parameters.placement.rotationY = 90.0;
    if (!session.setSelectedCylinderParameters(parameters)) {
        return false;
    }
    const double rotatedMaximumX = maximumXOf(session.currentBodies()[1].shape);
    return std::abs(rotatedMaximumX - initialMaximumX) > 1.0 &&
           session.selectedCylinderParameters().placement.rotationY == 90.0 && session.undo() &&
           session.selectedCylinderParameters().placement.rotationY == 0.0 &&
           std::abs(maximumXOf(session.currentBodies()[1].shape) - initialMaximumX) < 1.0e-9;
}

bool graphEvaluatesAdditionalFeatures() {
    voelund::document::ModelDocument document;
    const voelund::document::CylinderParameters parameters{
        4.0, 10.0, voelund::kernel::RigidTransform{30.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        voelund::document::CylinderResultMode::separate};
    const voelund::document::FeatureId featureId = document.addCylinderFeature(
        "Cylinder 2", voelund::document::ModelDocument::initialCylinderId(), parameters);
    const voelund::features::FeatureGraphResult result =
        voelund::features::FeatureGraph{}.evaluate(document);
    return result.bodies.size() == 2 &&
           result.bodies.front().sourceFeature ==
               voelund::document::ModelDocument::initialCylinderId() &&
           result.bodies.back().sourceFeature == featureId;
}

bool cylinderBooleanModesProduceExpectedVolumes() {
    voelund::application::CadSession session;
    if (!session.selectFeature(voelund::document::ModelDocument::initialCylinderId())) {
        return false;
    }
    voelund::document::CylinderParameters parameters = session.selectedCylinderParameters();
    constexpr double boxVolume = 24'000.0;
    const double overlapVolume = std::numbers::pi * 64.0 * 2.0;

    parameters.resultMode = voelund::document::CylinderResultMode::cut;
    if (!session.setSelectedCylinderParameters(parameters) ||
        std::abs(volumeOf(session.currentShape()) - (boxVolume - overlapVolume)) > 1.0e-6 ||
        session.currentBodies().size() != 1) {
        return false;
    }
    parameters.resultMode = voelund::document::CylinderResultMode::intersect;
    return session.setSelectedCylinderParameters(parameters) &&
           std::abs(volumeOf(session.currentShape()) - overlapVolume) < 1.0e-6 &&
           session.currentBodies().size() == 1 &&
           session.currentBodies().front().sourceFeature ==
               voelund::document::ModelDocument::initialCylinderId();
}

bool disconnectedCutReportsAnErrorAndKeepsLastValidShape() {
    voelund::application::CadSession session;
    if (!session.selectFeature(voelund::document::ModelDocument::initialCylinderId())) {
        return false;
    }
    const double validVolume = volumeOf(session.currentShape());
    voelund::document::CylinderParameters parameters = session.selectedCylinderParameters();
    parameters.resultMode = voelund::document::CylinderResultMode::cut;
    parameters.placement.translationZ = 10.0;
    if (!session.setSelectedCylinderParameters(parameters)) {
        return false;
    }
    const auto states = session.featureStates();
    return states.back().status == voelund::application::FeatureRuntimeStatus::error &&
           states.back().message.find("overlap") != std::string::npos &&
           std::abs(volumeOf(session.currentShape()) - validVolume) < 1.0e-9;
}

} // namespace voelund::test
