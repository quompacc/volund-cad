#include "../CommandResultAssertions.hpp"
#include "CadSessionTestCases.hpp"
#include "CadSessionTestSupport.hpp"

namespace voelund::test {

bool sessionCreatesFeaturesAndRestoresTheirHistory() {
    voelund::application::CadSession session;
    const voelund::document::FeatureId boxId =
        voelund::test::requireCommittedFeature(session.createBoxFeature());
    if (session.features().size() != 3 || session.currentBodies().size() != 2 ||
        session.selectedFeature() != boxId ||
        !(session.undo().status() == voelund::application::CommandStatus::committed) ||
        session.features().size() != 2 || session.selectedFeature() == boxId ||
        !(session.redo().status() == voelund::application::CommandStatus::committed) ||
        session.features().size() != 3) {
        return false;
    }

    const voelund::document::FeatureId cylinderId =
        voelund::test::requireCommittedFeature(session.createCylinderFeature());
    const auto features = session.features();
    return features.size() == 4 && features.back().id == cylinderId &&
           features.back().inputFeature == boxId && session.selectedFeature() == cylinderId &&
           session.currentBodies().size() == 3 && session.canTransformFeature(cylinderId);
}

bool separateBoxPlacementIsParametricAndUndoable() {
    voelund::application::CadSession session;
    const voelund::document::FeatureId boxId =
        voelund::test::requireCommittedFeature(session.createBoxFeature());
    const double initialMaximumX = maximumXOf(session.currentBodies().back().shape);
    if (!session.canTransformFeature(boxId)) {
        return false;
    }
    voelund::kernel::RigidTransform placement = session.selectedFeaturePlacement();
    placement.translationX = 30.0;
    placement.rotationY = voelund::kernel::toRadians(voelund::kernel::Degrees(90.0));
    if (!(session.editSelectedFeaturePlacement(placement).status() ==
          voelund::application::CommandStatus::committed)) {
        return false;
    }
    const double transformedMaximumX = maximumXOf(session.currentBodies().back().shape);
    return transformedMaximumX > initialMaximumX + 20.0 &&
           session.selectedBoxParameters().placement == placement &&
           (session.undo().status() == voelund::application::CommandStatus::committed) &&
           session.selectedBoxParameters().placement.translationX == 0.0 &&
           session.selectedBoxParameters().placement.rotationY == voelund::kernel::Radians{} &&
           std::abs(maximumXOf(session.currentBodies().back().shape) - initialMaximumX) < 1.0e-9;
}

bool newDocumentIsEmptyCleanAndReadyForItsFirstFeature() {
    voelund::application::CadSession session;
    session.newDocument();
    if (!session.features().empty() || !session.currentBodies().empty() ||
        !session.currentShape().IsNull() || session.selectedFeature().has_value() ||
        session.isModified() || !session.canAddCylinder()) { // M01: a new-body cylinder
        return false;
    }
    const voelund::document::FeatureId firstId =
        voelund::test::requireCommittedFeature(session.createBoxFeature());
    return firstId.value() == 1 && session.features().size() == 1 &&
           session.selectedFeature() == firstId && session.isModified();
}

bool sketchExtrusionIsParametricAndUndoable() {
    voelund::application::CadSession session;
    session.newDocument();
    const voelund::document::FeatureId boxId =
        voelund::test::requireCommittedFeature(session.createBoxFeature());
    const double boxVolume = volumeOf(session.currentShape());
    const voelund::document::SketchPlane plane{0.0, 0.0, 40.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0};
    const voelund::document::SketchRectangle rectangle{2.0, 3.0, 8.0, 9.0};
    const voelund::document::FeatureId extrusionId = voelund::test::requireCommittedFeature(
        session.createSketchExtrusion(boxId, plane, rectangle, 10.0));
    if (session.features().size() != 3 || session.selectedFeature() != extrusionId ||
        session.currentBodies().size() != 1 ||
        std::abs(volumeOf(session.currentShape()) - (boxVolume + 360.0)) > 1.0e-6) {
        return false;
    }
    if (!(session.undo().status() == voelund::application::CommandStatus::committed) ||
        session.features().size() != 1 ||
        std::abs(volumeOf(session.currentShape()) - boxVolume) > 1.0e-6) {
        return false;
    }
    return (session.redo().status() == voelund::application::CommandStatus::committed) &&
           session.features().size() == 3 && session.features().back().id == extrusionId &&
           std::abs(volumeOf(session.currentShape()) - (boxVolume + 360.0)) < 1.0e-6;
}

bool repeatedSketchExtrusionsRemainConnected() {
    voelund::application::CadSession session;
    session.newDocument();
    const voelund::document::FeatureId boxId =
        voelund::test::requireCommittedFeature(session.createBoxFeature());
    const double boxVolume = volumeOf(session.currentShape());
    const voelund::document::SketchRectangle firstRectangle{2.0, 3.0, 8.0, 9.0};
    const voelund::document::SketchPlane firstPlane{0.0, 0.0, 40.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0};
    const voelund::document::FeatureId firstExtrusion = voelund::test::requireCommittedFeature(
        session.createSketchExtrusion(boxId, firstPlane, firstRectangle, 10.0));
    const voelund::document::SketchRectangle secondRectangle{3.0, 4.0, 7.0, 8.0};
    const voelund::document::SketchPlane secondPlane{0.0, 0.0, 50.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0};
    const voelund::document::FeatureId secondExtrusion = voelund::test::requireCommittedFeature(
        session.createSketchExtrusion(firstExtrusion, secondPlane, secondRectangle, 10.0));
    const double expectedVolume = boxVolume + 360.0 + 160.0;
    const bool initialChainIsValid =
        session.features().size() == 5 && session.selectedFeature() == secondExtrusion &&
        session.featureStates().back().status ==
            voelund::application::FeatureRuntimeStatus::current &&
        std::abs(volumeOf(session.currentShape()) - expectedVolume) < 1.0e-4;
    if (!initialChainIsValid || !(session.editSelectedExtrusionParameters({20.0, {}}).status() ==
                                  voelund::application::CommandStatus::committed)) {
        return false;
    }
    const double editedVolume = boxVolume + 360.0 + 320.0;
    return std::abs(volumeOf(session.currentShape()) - editedVolume) < 1.0e-4 &&
           (session.undo().status() == voelund::application::CommandStatus::committed) &&
           std::abs(volumeOf(session.currentShape()) - expectedVolume) < 1.0e-4;
}

bool sketchSupportFollowsChangedFace() {
    voelund::application::CadSession session;
    session.newDocument();
    const auto boxId = voelund::test::requireCommittedFeature(session.createBoxFeature());
    const voelund::document::SketchPlane topPlane{0.0, 0.0, 40.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0};
    static_cast<void>(voelund::test::requireCommittedFeature(session.createSketchExtrusion(
        boxId, topPlane, voelund::document::SketchRectangle{2.0, 3.0, 8.0, 9.0}, 10.0)));
    if (!session.featureShape(boxId).has_value() ||
        std::abs(maximumZOf(session.currentShape()) - 50.0) > 1.0e-6)
        return false;
    if (!session.selectFeature(boxId))
        return false;
    auto changed = session.selectedBoxParameters();
    changed.dimensions.height = 47.0;
    return (session.editSelectedBoxParameters(changed).status() ==
            voelund::application::CommandStatus::committed) &&
           std::abs(maximumZOf(session.currentShape()) - 57.0) < 1.0e-6;
}

bool projectedSupportPointsFollowChangedEdges() {
    using namespace voelund::document;
    SketchParameters sketch{{0.0, 0.0, 20.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0},
                            {{{SketchElementId{1}, SketchLine{{1.0, 1.0}, {2.0, 2.0}}}}, 2}};
    for (std::uint64_t referenceId = 1; referenceId <= 4; ++referenceId) {
        const auto attachment = std::vector<double>{static_cast<double>(referenceId), 1.0};
        sketch.constraints.values.push_back({SketchConstraintId{sketch.constraints.nextId++},
                                             SketchConstraintKind::coordinateX,
                                             {SketchElementId{1}, SketchPointPosition::start},
                                             std::nullopt,
                                             0.0,
                                             std::nullopt,
                                             attachment});
        sketch.constraints.values.push_back({SketchConstraintId{sketch.constraints.nextId++},
                                             SketchConstraintKind::coordinateY,
                                             {SketchElementId{1}, SketchPointPosition::start},
                                             std::nullopt,
                                             0.0,
                                             std::nullopt,
                                             attachment});
    }
    const auto original = voelund::kernel::ShapeFactory::makeBox({40.0, 30.0, 20.0});
    if (!voelund::features::updateSketchSupportReferences(sketch, original))
        return false;
    std::vector<double> originalValues;
    for (const auto& constraint : sketch.constraints.values)
        originalValues.push_back(constraint.value);
    const auto changed = voelund::kernel::ShapeFactory::makeBox({55.0, 36.0, 27.0});
    if (!voelund::features::updateSketchSupportReferences(sketch, changed) ||
        std::abs(sketch.plane.originZ - 27.0) > 1.0e-9)
        return false;
    for (std::size_t index = 0; index < originalValues.size(); ++index) {
        if (std::abs(originalValues[index] - sketch.constraints.values[index].value) > 1.0e-9)
            return true;
    }
    return false;
}

} // namespace voelund::test
