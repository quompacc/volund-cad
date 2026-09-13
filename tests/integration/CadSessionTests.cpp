#include "CadSessionTestCases.hpp"

#include <iostream>

namespace {
struct TestCase {
    const char* name;
    bool (*run)();
};
constexpr TestCase cases[] = {
    {"recomputeTracksDocumentHistory", voelund::test::recomputeTracksDocumentHistory},
    {"selectionUsesStableFeatureIdentity", voelund::test::selectionUsesStableFeatureIdentity},
    {"cylinderCanProduceSeparateBodies", voelund::test::cylinderCanProduceSeparateBodies},
    {"separateCylinderMovesIndependentlyAndSupportsUndo",
     voelund::test::separateCylinderMovesIndependentlyAndSupportsUndo},
    {"separateCylinderDoesNotFollowItsFormerInput",
     voelund::test::separateCylinderDoesNotFollowItsFormerInput},
    {"joinedResultMovesAsOneBodyAndSupportsUndo",
     voelund::test::joinedResultMovesAsOneBodyAndSupportsUndo},
    {"disconnectedJoinReportsAnErrorAndKeepsLastValidShape",
     voelund::test::disconnectedJoinReportsAnErrorAndKeepsLastValidShape},
    {"separateCylinderRotationIsParametricAndUndoable",
     voelund::test::separateCylinderRotationIsParametricAndUndoable},
    {"graphEvaluatesAdditionalFeatures", voelund::test::graphEvaluatesAdditionalFeatures},
    {"cylinderBooleanModesProduceExpectedVolumes",
     voelund::test::cylinderBooleanModesProduceExpectedVolumes},
    {"disconnectedCutReportsAnErrorAndKeepsLastValidShape",
     voelund::test::disconnectedCutReportsAnErrorAndKeepsLastValidShape},
    {"sessionCreatesFeaturesAndRestoresTheirHistory",
     voelund::test::sessionCreatesFeaturesAndRestoresTheirHistory},
    {"separateBoxPlacementIsParametricAndUndoable",
     voelund::test::separateBoxPlacementIsParametricAndUndoable},
    {"sessionFileRoundtripRestoresTheDocument",
     voelund::test::sessionFileRoundtripRestoresTheDocument},
    {"newDocumentIsEmptyCleanAndReadyForItsFirstFeature",
     voelund::test::newDocumentIsEmptyCleanAndReadyForItsFirstFeature},
    {"sketchExtrusionIsParametricAndUndoable",
     voelund::test::sketchExtrusionIsParametricAndUndoable},
    {"repeatedSketchExtrusionsRemainConnected",
     voelund::test::repeatedSketchExtrusionsRemainConnected},
    {"sketchSupportFollowsChangedFace", voelund::test::sketchSupportFollowsChangedFace},
    {"projectedSupportPointsFollowChangedEdges",
     voelund::test::projectedSupportPointsFollowChangedEdges},
    {"multipleSketchElementsExtrudeAllClosedProfiles",
     voelund::test::multipleSketchElementsExtrudeAllClosedProfiles},
    {"selectedSketchProfilesExtrudeAndRoundtrip",
     voelund::test::selectedSketchProfilesExtrudeAndRoundtrip},
};
} // namespace

int main() {
    for (const auto& test : cases) {
        if (!test.run()) {
            std::cerr << test.name << " failed\n";
            return 1;
        }
    }
    return 0;
}
