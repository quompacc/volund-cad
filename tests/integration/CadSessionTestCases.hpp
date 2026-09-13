#pragma once

namespace voelund::test {
bool recomputeTracksDocumentHistory();
bool selectionUsesStableFeatureIdentity();
bool cylinderCanProduceSeparateBodies();
bool separateCylinderMovesIndependentlyAndSupportsUndo();
bool separateCylinderDoesNotFollowItsFormerInput();
bool joinedResultMovesAsOneBodyAndSupportsUndo();
bool disconnectedJoinReportsAnErrorAndKeepsLastValidShape();
bool separateCylinderRotationIsParametricAndUndoable();
bool graphEvaluatesAdditionalFeatures();
bool cylinderBooleanModesProduceExpectedVolumes();
bool disconnectedCutReportsAnErrorAndKeepsLastValidShape();
bool sessionCreatesFeaturesAndRestoresTheirHistory();
bool separateBoxPlacementIsParametricAndUndoable();
bool sessionFileRoundtripRestoresTheDocument();
bool newDocumentIsEmptyCleanAndReadyForItsFirstFeature();
bool sketchExtrusionIsParametricAndUndoable();
bool repeatedSketchExtrusionsRemainConnected();
bool sketchSupportFollowsChangedFace();
bool projectedSupportPointsFollowChangedEdges();
bool multipleSketchElementsExtrudeAllClosedProfiles();
bool selectedSketchProfilesExtrudeAndRoundtrip();
} // namespace voelund::test
