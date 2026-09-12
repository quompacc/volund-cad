#include <voelund/application/CadSession.hpp>
#include <voelund/features/FeatureGraph.hpp>
#include <voelund/features/SketchSupportResolver.hpp>
#include <voelund/kernel/ShapeFactory.hpp>

#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>

#include <cmath>
#include <filesystem>
#include <iostream>
#include <numbers>
#include <string>

namespace {

double volumeOf(const TopoDS_Shape& shape) {
    GProp_GProps properties;
    BRepGProp::VolumeProperties(shape, properties);
    return properties.Mass();
}

double maximumXOf(const TopoDS_Shape& shape) {
    Bnd_Box bounds;
    BRepBndLib::Add(shape, bounds);
    double minimumX = 0.0;
    double minimumY = 0.0;
    double minimumZ = 0.0;
    double maximumX = 0.0;
    double maximumY = 0.0;
    double maximumZ = 0.0;
    bounds.Get(minimumX, minimumY, minimumZ, maximumX, maximumY, maximumZ);
    return maximumX;
}

double maximumZOf(const TopoDS_Shape& shape) {
    Bnd_Box bounds;
    BRepBndLib::Add(shape, bounds);
    double minimumX = 0.0;
    double minimumY = 0.0;
    double minimumZ = 0.0;
    double maximumX = 0.0;
    double maximumY = 0.0;
    double maximumZ = 0.0;
    bounds.Get(minimumX, minimumY, minimumZ, maximumX, maximumY, maximumZ);
    return maximumZ;
}

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

bool sessionCreatesFeaturesAndRestoresTheirHistory() {
    voelund::application::CadSession session;
    const voelund::document::FeatureId boxId = session.addBoxFeature();
    if (session.features().size() != 3 || session.currentBodies().size() != 2 ||
        session.selectedFeature() != boxId || !session.undo() || session.features().size() != 2 ||
        session.selectedFeature() == boxId || !session.redo() || session.features().size() != 3) {
        return false;
    }

    const voelund::document::FeatureId cylinderId = session.addCylinderFeature();
    const auto features = session.features();
    return features.size() == 4 && features.back().id == cylinderId &&
           features.back().inputFeature == boxId && session.selectedFeature() == cylinderId &&
           session.currentBodies().size() == 3 && session.canTransformFeature(cylinderId);
}

bool separateBoxPlacementIsParametricAndUndoable() {
    voelund::application::CadSession session;
    const voelund::document::FeatureId boxId = session.addBoxFeature();
    const double initialMaximumX = maximumXOf(session.currentBodies().back().shape);
    if (!session.canTransformFeature(boxId)) {
        return false;
    }
    voelund::kernel::RigidTransform placement = session.selectedFeaturePlacement();
    placement.translationX = 30.0;
    placement.rotationY = 90.0;
    if (!session.setSelectedFeaturePlacement(placement)) {
        return false;
    }
    const double transformedMaximumX = maximumXOf(session.currentBodies().back().shape);
    return transformedMaximumX > initialMaximumX + 20.0 &&
           session.selectedBoxParameters().placement == placement && session.undo() &&
           session.selectedBoxParameters().placement.translationX == 0.0 &&
           session.selectedBoxParameters().placement.rotationY == 0.0 &&
           std::abs(maximumXOf(session.currentBodies().back().shape) - initialMaximumX) < 1.0e-9;
}

bool sessionFileRoundtripRestoresTheDocument() {
    const std::filesystem::path path =
        std::filesystem::current_path() / "voelund-session-roundtrip.volund";
    voelund::application::CadSession session;
    if (session.isModified()) {
        return false;
    }
    const voelund::document::FeatureId boxId = session.addBoxFeature();
    voelund::kernel::RigidTransform placement = session.selectedFeaturePlacement();
    placement.translationX = 37.0;
    static_cast<void>(session.setSelectedFeaturePlacement(placement));
    if (!session.isModified()) {
        return false;
    }
    session.saveDocument(path);
    if (session.isModified()) {
        return false;
    }
    voelund::kernel::RigidTransform changedPlacement = placement;
    changedPlacement.translationX = 38.0;
    static_cast<void>(session.setSelectedFeaturePlacement(changedPlacement));
    if (!session.isModified() || !session.undo() || session.isModified()) {
        return false;
    }
    static_cast<void>(session.addCylinderFeature());
    if (!session.isModified()) {
        return false;
    }
    session.loadDocument(path);
    std::error_code error;
    std::filesystem::remove(path, error);
    return !error && !session.isModified() && session.features().size() == 3 &&
           session.selectedFeature() == boxId && session.selectedFeaturePlacement() == placement &&
           !session.canUndo() && !session.canRedo();
}

bool newDocumentIsEmptyCleanAndReadyForItsFirstFeature() {
    voelund::application::CadSession session;
    session.newDocument();
    if (!session.features().empty() || !session.currentBodies().empty() ||
        !session.currentShape().IsNull() || session.selectedFeature().has_value() ||
        session.isModified() || session.canAddCylinder()) {
        return false;
    }
    const voelund::document::FeatureId firstId = session.addBoxFeature();
    return firstId.value() == 1 && session.features().size() == 1 &&
           session.selectedFeature() == firstId && session.isModified();
}

bool sketchExtrusionIsParametricAndUndoable() {
    voelund::application::CadSession session;
    session.newDocument();
    const voelund::document::FeatureId boxId = session.addBoxFeature();
    const double boxVolume = volumeOf(session.currentShape());
    const voelund::document::SketchPlane plane{0.0, 0.0, 40.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0};
    const voelund::document::SketchRectangle rectangle{2.0, 3.0, 8.0, 9.0};
    const voelund::document::FeatureId extrusionId =
        session.addSketchExtrusion(boxId, plane, rectangle, 10.0);
    if (session.features().size() != 3 || session.selectedFeature() != extrusionId ||
        session.currentBodies().size() != 1 ||
        std::abs(volumeOf(session.currentShape()) - (boxVolume + 360.0)) > 1.0e-6) {
        return false;
    }
    if (!session.undo() || session.features().size() != 2 ||
        std::abs(volumeOf(session.currentShape()) - boxVolume) > 1.0e-6) {
        return false;
    }
    return session.undo() && session.features().size() == 1 &&
           std::abs(volumeOf(session.currentShape()) - boxVolume) < 1.0e-6;
}

bool repeatedSketchExtrusionsRemainConnected() {
    voelund::application::CadSession session;
    session.newDocument();
    const voelund::document::FeatureId boxId = session.addBoxFeature();
    const double boxVolume = volumeOf(session.currentShape());
    const voelund::document::SketchRectangle firstRectangle{2.0, 3.0, 8.0, 9.0};
    const voelund::document::SketchPlane firstPlane{0.0, 0.0, 40.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0};
    const voelund::document::FeatureId firstExtrusion =
        session.addSketchExtrusion(boxId, firstPlane, firstRectangle, 10.0);
    const voelund::document::SketchRectangle secondRectangle{3.0, 4.0, 7.0, 8.0};
    const voelund::document::SketchPlane secondPlane{0.0, 0.0, 50.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0};
    const voelund::document::FeatureId secondExtrusion =
        session.addSketchExtrusion(firstExtrusion, secondPlane, secondRectangle, 10.0);
    const double expectedVolume = boxVolume + 360.0 + 160.0;
    const bool initialChainIsValid =
        session.features().size() == 5 && session.selectedFeature() == secondExtrusion &&
        session.featureStates().back().status ==
            voelund::application::FeatureRuntimeStatus::current &&
        std::abs(volumeOf(session.currentShape()) - expectedVolume) < 1.0e-4;
    if (!initialChainIsValid || !session.setSelectedExtrusionParameters({20.0, {}})) {
        return false;
    }
    const double editedVolume = boxVolume + 360.0 + 320.0;
    return std::abs(volumeOf(session.currentShape()) - editedVolume) < 1.0e-4 && session.undo() &&
           std::abs(volumeOf(session.currentShape()) - expectedVolume) < 1.0e-4;
}

bool sketchSupportFollowsChangedFace() {
    voelund::application::CadSession session;
    session.newDocument();
    const auto boxId = session.addBoxFeature();
    const voelund::document::SketchPlane topPlane{0.0, 0.0, 40.0, 0.0, 0.0, 1.0,
                                                   1.0, 0.0, 0.0};
    static_cast<void>(session.addSketchExtrusion(
        boxId, topPlane, voelund::document::SketchRectangle{2.0, 3.0, 8.0, 9.0}, 10.0));
    if (!session.featureShape(boxId).has_value() ||
        std::abs(maximumZOf(session.currentShape()) - 50.0) > 1.0e-6)
        return false;
    if (!session.selectFeature(boxId))
        return false;
    auto changed = session.selectedBoxParameters();
    changed.dimensions.height = 47.0;
    return session.setSelectedBoxParameters(changed) &&
           std::abs(maximumZOf(session.currentShape()) - 57.0) < 1.0e-6;
}

bool projectedSupportPointsFollowChangedEdges() {
    using namespace voelund::document;
    SketchParameters sketch{{0.0, 0.0, 20.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0},
                            {{{SketchElementId{1}, SketchLine{{1.0, 1.0}, {2.0, 2.0}}}}, 2}};
    for (std::uint64_t referenceId = 1; referenceId <= 4; ++referenceId) {
        const auto attachment = std::vector<double>{static_cast<double>(referenceId), 1.0};
        sketch.constraints.values.push_back(
            {SketchConstraintId{sketch.constraints.nextId++}, SketchConstraintKind::coordinateX,
             {SketchElementId{1}, SketchPointPosition::start}, std::nullopt, 0.0, std::nullopt,
             attachment});
        sketch.constraints.values.push_back(
            {SketchConstraintId{sketch.constraints.nextId++}, SketchConstraintKind::coordinateY,
             {SketchElementId{1}, SketchPointPosition::start}, std::nullopt, 0.0, std::nullopt,
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

bool multipleSketchElementsExtrudeAllClosedProfiles() {
    voelund::application::CadSession session;
    session.newDocument();
    const auto boxId = session.addBoxFeature();
    const double boxVolume = volumeOf(session.currentShape());
    const voelund::document::SketchPlane plane{0.0, 0.0, 40.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0};
    const std::vector<voelund::document::SketchElement> elements{
        {voelund::document::SketchElementId{1},
         voelund::document::SketchRectangle{2.0, 3.0, 8.0, 9.0}},
        {voelund::document::SketchElementId{2}, voelund::document::SketchCircle{{14.0, 8.0}, 2.0}},
        {voelund::document::SketchElementId{3},
         voelund::document::SketchLine{{10.0, 15.0}, {14.0, 15.0}}},
        {voelund::document::SketchElementId{4},
         voelund::document::SketchLine{{14.0, 15.0}, {10.0, 19.0}}},
        {voelund::document::SketchElementId{5},
         voelund::document::SketchLine{{10.0, 19.0}, {10.0, 15.0}}},
        {voelund::document::SketchElementId{6},
         voelund::document::SketchLine{{17.0, 17.0}, {19.0, 17.0}}}};
    const voelund::document::SketchParameters sketch{
        plane, voelund::document::SketchElements{elements, 7}};
    const auto extrusionId = session.addSketchExtrusion(boxId, sketch, 10.0);
    const double expected = boxVolume + (36.0 + std::numbers::pi * 4.0 + 8.0) * 10.0;
    if (session.featureStates().back().status !=
            voelund::application::FeatureRuntimeStatus::current ||
        std::abs(volumeOf(session.currentShape()) - expected) >= 1.0e-4) {
        return false;
    }
    const auto sketchId = session.features()[1].id;
    if (!session.selectFeature(sketchId)) {
        return false;
    }
    const auto evaluatedElements = session.selectedSketchElements();
    if (evaluatedElements.size() != elements.size() ||
        std::ranges::any_of(evaluatedElements,
                            [](const auto& element) { return element.shape.IsNull(); }) ||
        !session.selectSketchElement(voelund::document::SketchElementId{4}) ||
        session.selectedSketchElement() != voelund::document::SketchElementId{4} ||
        session.selectSketchElement(voelund::document::SketchElementId{99}) ||
        !session.selectFeature(extrusionId) || session.selectedSketchElement().has_value()) {
        return false;
    }
    const std::filesystem::path path =
        std::filesystem::current_path() / "voelund-multi-sketch-roundtrip.volund";
    session.saveDocument(path);
    voelund::application::CadSession loaded;
    loaded.loadDocument(path);
    std::error_code error;
    std::filesystem::remove(path, error);
    return !error && loaded.features().size() == 3 &&
           std::abs(volumeOf(loaded.currentShape()) - expected) < 1.0e-4;
}

bool selectedSketchProfilesExtrudeAndRoundtrip() {
    using namespace voelund;
    application::CadSession session;
    session.newDocument();
    const auto boxId = session.addBoxFeature();
    const double boxVolume = volumeOf(session.currentShape());
    const document::SketchPlane plane{0.0, 0.0, 40.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0};
    const std::vector<document::SketchElement> elements{
        {document::SketchElementId{1}, document::SketchRectangle{2.0, 3.0, 8.0, 9.0}},
        {document::SketchElementId{2}, document::SketchCircle{{14.0, 8.0}, 2.0}},
        {document::SketchElementId{3}, document::SketchLine{{10.0, 15.0}, {14.0, 15.0}}},
        {document::SketchElementId{4}, document::SketchLine{{14.0, 15.0}, {10.0, 19.0}}},
        {document::SketchElementId{5}, document::SketchLine{{10.0, 19.0}, {10.0, 15.0}}},
        {document::SketchElementId{6}, document::SketchLine{{17.0, 17.0}, {19.0, 17.0}}}};
    const auto sketchId = session.addSketchFeature(
        boxId, document::SketchParameters{plane, document::SketchElements{elements, 7}});
    if (!session.selectSketchProfileByElement(document::SketchElementId{4}, false) ||
        !session.selectSketchProfileByElement(document::SketchElementId{2}, true) ||
        session.selectSketchProfileByElement(document::SketchElementId{6}, true) ||
        session.selectedSketchProfiles().size() != 2 ||
        session.selectedSketchProfileElements().size() != 4 ||
        !session.canExtrudeSelectedSketchProfiles())
        return false;

    const std::vector<document::SketchProfileId> selected(session.selectedSketchProfiles().begin(),
                                                          session.selectedSketchProfiles().end());
    const auto extrusionId = session.addSketchProfileExtrusion(sketchId, selected, 10.0);
    const double expected = boxVolume + (8.0 + std::numbers::pi * 4.0) * 10.0;
    if (session.selectedFeature() != extrusionId ||
        session.selectedExtrusionParameters().profiles != selected ||
        std::abs(volumeOf(session.currentShape()) - expected) >= 1.0e-4 || !session.undo() ||
        std::abs(volumeOf(session.currentShape()) - boxVolume) >= 1.0e-4 || !session.redo() ||
        std::abs(volumeOf(session.currentShape()) - expected) >= 1.0e-4)
        return false;

    const std::filesystem::path path =
        std::filesystem::current_path() / "voelund-selected-profiles-roundtrip.volund";
    session.saveDocument(path);
    application::CadSession loaded;
    loaded.loadDocument(path);
    std::error_code error;
    std::filesystem::remove(path, error);
    return !error && loaded.selectedExtrusionParameters().profiles == selected &&
           std::abs(volumeOf(loaded.currentShape()) - expected) < 1.0e-4;
}

} // namespace

int main() {
    if (!recomputeTracksDocumentHistory()) {
        std::cerr << "Session recompute history test failed\n";
        return 1;
    }
    if (!selectionUsesStableFeatureIdentity()) {
        std::cerr << "Session feature selection test failed\n";
        return 1;
    }
    if (!cylinderCanProduceSeparateBodies()) {
        std::cerr << "Cylinder result mode test failed\n";
        return 1;
    }
    if (!separateCylinderMovesIndependentlyAndSupportsUndo()) {
        std::cerr << "Independent cylinder movement test failed\n";
        return 1;
    }
    if (!separateCylinderDoesNotFollowItsFormerInput()) {
        std::cerr << "Detached cylinder followed its former input\n";
        return 1;
    }
    if (!joinedResultMovesAsOneBodyAndSupportsUndo()) {
        std::cerr << "Joined result did not move as one body\n";
        return 1;
    }
    if (!disconnectedJoinReportsAnErrorAndKeepsLastValidShape()) {
        std::cerr << "Disconnected join validation test failed\n";
        return 1;
    }
    if (!separateCylinderRotationIsParametricAndUndoable()) {
        std::cerr << "Parametric cylinder rotation test failed\n";
        return 1;
    }
    if (!graphEvaluatesAdditionalFeatures()) {
        std::cerr << "Generic feature graph test failed\n";
        return 1;
    }
    if (!cylinderBooleanModesProduceExpectedVolumes()) {
        std::cerr << "Cylinder boolean modes test failed\n";
        return 1;
    }
    if (!disconnectedCutReportsAnErrorAndKeepsLastValidShape()) {
        std::cerr << "Disconnected cut validation test failed\n";
        return 1;
    }
    if (!sessionCreatesFeaturesAndRestoresTheirHistory()) {
        std::cerr << "Session feature creation test failed\n";
        return 1;
    }
    if (!separateBoxPlacementIsParametricAndUndoable()) {
        std::cerr << "Parametric box placement test failed\n";
        return 1;
    }
    if (!sessionFileRoundtripRestoresTheDocument()) {
        std::cerr << "Session document file roundtrip test failed\n";
        return 1;
    }
    if (!newDocumentIsEmptyCleanAndReadyForItsFirstFeature()) {
        std::cerr << "New empty document test failed\n";
        return 1;
    }
    if (!sketchExtrusionIsParametricAndUndoable()) {
        std::cerr << "Sketch extrusion test failed\n";
        return 1;
    }
    if (!repeatedSketchExtrusionsRemainConnected()) {
        std::cerr << "Repeated sketch extrusion test failed\n";
        return 1;
    }
    if (!sketchSupportFollowsChangedFace()) {
        std::cerr << "Associative sketch support test failed\n";
        return 1;
    }
    if (!projectedSupportPointsFollowChangedEdges()) {
        std::cerr << "Projected sketch support reference test failed\n";
        return 1;
    }
    if (!multipleSketchElementsExtrudeAllClosedProfiles()) {
        std::cerr << "Multiple sketch profile extrusion test failed\n";
        return 1;
    }
    if (!selectedSketchProfilesExtrudeAndRoundtrip()) {
        std::cerr << "Selected sketch profile extrusion test failed\n";
        return 1;
    }
    return 0;
}
