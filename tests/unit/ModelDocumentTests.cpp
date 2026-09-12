#include <voelund/document/DocumentSerializer.hpp>
#include <voelund/document/ModelDocument.hpp>

#include <iostream>
#include <sstream>
#include <variant>

namespace {

bool undoAndRedoRestoreDimensions() {
    voelund::document::ModelDocument document;
    constexpr voelund::document::FeatureId boxId = voelund::document::ModelDocument::initialBoxId();
    const voelund::kernel::BoxDimensions initial = document.boxDimensions(boxId);
    constexpr voelund::kernel::BoxDimensions changed{80.0, 25.0, 12.0};

    if (!document.setBoxDimensions(boxId, changed) || !document.canUndo() || document.canRedo()) {
        return false;
    }
    if (!document.undo() || document.boxDimensions(boxId) != initial || !document.canRedo()) {
        return false;
    }
    return document.redo() && document.boxDimensions(boxId) == changed;
}

bool newChangeClearsRedoHistory() {
    voelund::document::ModelDocument document;
    constexpr voelund::document::FeatureId boxId = voelund::document::ModelDocument::initialBoxId();
    constexpr voelund::kernel::BoxDimensions first{80.0, 25.0, 12.0};
    constexpr voelund::kernel::BoxDimensions second{30.0, 20.0, 10.0};

    static_cast<void>(document.setBoxDimensions(boxId, first));
    static_cast<void>(document.undo());
    static_cast<void>(document.setBoxDimensions(boxId, second));
    return !document.canRedo();
}

bool featureIdentitySurvivesHistory() {
    voelund::document::ModelDocument document;
    const auto featuresBefore = document.features();
    const voelund::document::FeatureId boxId = featuresBefore.front().id;
    const voelund::document::FeatureId cylinderId = featuresBefore.back().id;
    constexpr voelund::kernel::BoxDimensions changed{50.0, 40.0, 30.0};

    static_cast<void>(document.setBoxDimensions(boxId, changed));
    static_cast<void>(document.undo());
    static_cast<void>(document.redo());
    const auto featuresAfter = document.features();
    return featuresAfter.size() == 2 && featuresAfter.front().id == boxId &&
           featuresAfter.back().id == cylinderId;
}

bool featureCreationParticipatesInHistory() {
    voelund::document::ModelDocument document;
    const voelund::document::CylinderParameters parameters{
        4.0, 10.0, voelund::kernel::RigidTransform{25.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        voelund::document::CylinderResultMode::separate};
    const voelund::document::FeatureId featureId = document.addCylinderFeature(
        "Cylinder 2", voelund::document::ModelDocument::initialCylinderId(), parameters);
    if (document.features().size() != 3 || document.cylinderParameters(featureId) != parameters) {
        return false;
    }
    if (!document.undo() || document.features().size() != 2 || document.contains(featureId)) {
        return false;
    }
    return document.redo() && document.features().size() == 3 && document.contains(featureId) &&
           document.cylinderParameters(featureId) == parameters;
}

bool serializationPreservesGraphAndResetsHistory() {
    voelund::document::ModelDocument source;
    voelund::document::BoxParameters box =
        source.boxParameters(voelund::document::ModelDocument::initialBoxId());
    box.placement = voelund::kernel::RigidTransform{12.5, -4.0, 3.0, 0.0, 45.0, 0.0};
    static_cast<void>(
        source.setBoxParameters(voelund::document::ModelDocument::initialBoxId(), box));
    const voelund::document::CylinderParameters cylinder{
        3.5, 11.0, voelund::kernel::RigidTransform{8.0, 2.0, 0.0, 5.0, 10.0, 15.0},
        voelund::document::CylinderResultMode::separate};
    const voelund::document::FeatureId thirdId = source.addCylinderFeature(
        "Cylinder with spaces", voelund::document::ModelDocument::initialCylinderId(), cylinder);

    std::ostringstream output;
    voelund::document::DocumentSerializer::write(source, output);
    std::istringstream input(output.str());
    voelund::document::ModelDocument loaded = voelund::document::DocumentSerializer::read(input);
    const auto features = loaded.features();
    if (features.size() != 3 || features.back().id != thirdId ||
        features.back().name != "Cylinder with spaces" ||
        loaded.boxParameters(voelund::document::ModelDocument::initialBoxId()) != box ||
        loaded.cylinderParameters(thirdId) != cylinder || loaded.canUndo()) {
        return false;
    }
    return loaded.addBoxFeature("Box after load", {1.0, 2.0, 3.0}).value() == 4;
}

bool serializationRejectsForwardReferences() {
    std::istringstream input("VOLUND 1\nNEXT_ID 3\nFEATURES 1\n"
                             "CYLINDER 2 \"Broken\" 1 5 10 0 0 0 0 0 0 JOIN\n");
    try {
        static_cast<void>(voelund::document::DocumentSerializer::read(input));
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

bool emptyDocumentRoundtripRemainsUsable() {
    const voelund::document::ModelDocument empty = voelund::document::ModelDocument::empty();
    std::ostringstream output;
    voelund::document::DocumentSerializer::write(empty, output);
    std::istringstream input(output.str());
    voelund::document::ModelDocument loaded = voelund::document::DocumentSerializer::read(input);
    return loaded.features().empty() &&
           loaded.addBoxFeature("First Box", {10.0, 20.0, 30.0}).value() == 1;
}

voelund::document::SketchParameters sampleSketch() {
    const voelund::document::SketchPlane plane{0.0, 0.0, 30.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0};
    const voelund::document::SketchElement rectangle{
        voelund::document::SketchElementId{7},
        voelund::document::SketchRectangle{1.0, 2.0, 5.0, 6.0}};
    return {plane, voelund::document::SketchElements{{rectangle}, 8}};
}

bool sketchElementsKeepStableIdsThroughHistoryAndSerialization() {
    voelund::document::ModelDocument document = voelund::document::ModelDocument::empty();
    const auto boxId = document.addBoxFeature("Support", {20.0, 20.0, 30.0});
    const auto sketchId = document.addSketchFeature("General sketch", boxId, sampleSketch());
    const auto circleId =
        document.addSketchElement(sketchId, voelund::document::SketchCircle{{12.0, 8.0}, 2.0});
    if (circleId.value() != 8 || !document.undo() ||
        document.sketchParameters(sketchId).elements.values.size() != 1 || !document.redo()) {
        return false;
    }
    std::ostringstream output;
    voelund::document::DocumentSerializer::write(document, output);
    if (!output.str().starts_with("VOLUND 6\n"))
        return false;
    std::istringstream input(output.str());
    const auto loaded = voelund::document::DocumentSerializer::read(input);
    const auto& elements = loaded.sketchParameters(sketchId).elements;
    return elements.values.size() == 2 && elements.values[0].id.value() == 7 &&
           elements.values[1].id == circleId && elements.nextId == 9 &&
           std::holds_alternative<voelund::document::SketchCircle>(elements.values[1].geometry);
}

bool pointConstraintsRoundtripInVersionSix() {
    using namespace voelund::document;
    ModelDocument document = ModelDocument::empty();
    const auto support = document.addBoxFeature("Support", {20.0, 20.0, 30.0});
    SketchParameters sketch{
        {0.0, 0.0, 30.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0},
        {{{SketchElementId{1}, SketchLine{{2.0, 0.0}, {4.0, 3.0}}},
          {SketchElementId{2}, SketchLine{{0.0, 0.0}, {10.0, 0.0}}}},
         3},
        {{{SketchConstraintId{1},
           SketchConstraintKind::coordinateX,
           {SketchElementId{1}, SketchPointPosition::start},
           std::nullopt,
           2.0},
          {SketchConstraintId{2},
           SketchConstraintKind::coordinateY,
           {SketchElementId{1}, SketchPointPosition::start},
           std::nullopt,
           0.0},
          {SketchConstraintId{3},
           SketchConstraintKind::pointOnObject,
           {SketchElementId{1}, SketchPointPosition::start},
           SketchGeometryReference{SketchElementId{2}, SketchPointPosition::start},
           0.0}},
         4}};
    const auto sketchId = document.addSketchFeature("Constrained", support, sketch);
    std::ostringstream output;
    DocumentSerializer::write(document, output);
    std::istringstream input(output.str());
    const auto loaded = DocumentSerializer::read(input);
    const auto& constraints = loaded.sketchParameters(sketchId).constraints;
    return output.str().starts_with("VOLUND 6\n") && constraints == sketch.constraints;
}

bool versionTwoLineMigratesWithoutConstraint() {
    std::istringstream input("VOLUND 2\nNEXT_ID 3\nFEATURES 2\n"
                             "BOX 1 \"Support\" 20 20 30 0 0 0 0 0 0\n"
                             "SKETCH 2 \"Sketch\" 1 0 0 30 0 0 1 1 0 0 1 2\n"
                             "ELEMENT LINE 1 0 0 3 4\n");
    const auto loaded = voelund::document::DocumentSerializer::read(input);
    const auto& geometry =
        loaded.sketchParameters(voelund::document::FeatureId{2}).elements.values.front().geometry;
    const auto& line = std::get<voelund::document::SketchLine>(geometry);
    return line.constraint == voelund::document::SketchLineConstraint::none;
}

bool versionFourExtrusionMigratesToAllProfiles() {
    std::istringstream input("VOLUND 4\nNEXT_ID 4\nFEATURES 3\n"
                             "BOX 1 \"Support\" 20 20 30 0 0 0 0 0 0\n"
                             "SKETCH 2 \"Sketch\" 1 0 0 30 0 0 1 1 0 0 1 2 0 1\n"
                             "ELEMENT RECTANGLE 1 1 1 4 4\n"
                             "EXTRUSION 3 \"Extrusion\" 2 5\n");
    const auto loaded = voelund::document::DocumentSerializer::read(input);
    return loaded.extrusionParameters(voelund::document::FeatureId{3}).profiles.empty();
}

bool unsatisfiedLineConstraintIsRejected() {
    std::istringstream input("VOLUND 3\nNEXT_ID 3\nFEATURES 2\n"
                             "BOX 1 \"Support\" 20 20 30 0 0 0 0 0 0\n"
                             "SKETCH 2 \"Sketch\" 1 0 0 30 0 0 1 1 0 0 1 2\n"
                             "ELEMENT LINE 1 0 0 3 4 HORIZONTAL\n");
    try {
        static_cast<void>(voelund::document::DocumentSerializer::read(input));
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

bool legacySketchMigratesToStableRectangleElement() {
    std::istringstream input("VOLUND 1\nNEXT_ID 3\nFEATURES 2\n"
                             "BOX 1 \"Support\" 20 20 30 0 0 0 0 0 0\n"
                             "SKETCH 2 \"Legacy\" 1 0 0 30 0 0 1 1 0 0 2 3 8 9\n");
    const auto loaded = voelund::document::DocumentSerializer::read(input);
    const auto& elements = loaded.sketchParameters(voelund::document::FeatureId{2}).elements;
    return elements.values.size() == 1 && elements.values.front().id.value() == 1 &&
           elements.nextId == 2 &&
           std::holds_alternative<voelund::document::SketchRectangle>(
               elements.values.front().geometry);
}

bool invalidSketchGeometryIsRejected() {
    auto sketch = sampleSketch();
    sketch.elements.values.push_back(
        {voelund::document::SketchElementId{7}, voelund::document::SketchCircle{{1.0, 1.0}, 0.0}});
    voelund::document::ModelDocument document = voelund::document::ModelDocument::empty();
    const auto support = document.addBoxFeature("Support", {1.0, 1.0, 1.0});
    try {
        static_cast<void>(document.addSketchFeature("Invalid", support, sketch));
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

} // namespace

int main() {
    if (!undoAndRedoRestoreDimensions()) {
        std::cerr << "Document undo/redo test failed\n";
        return 1;
    }
    if (!newChangeClearsRedoHistory()) {
        std::cerr << "Document redo invalidation test failed\n";
        return 1;
    }
    if (!featureIdentitySurvivesHistory()) {
        std::cerr << "Stable feature identity test failed\n";
        return 1;
    }
    if (!featureCreationParticipatesInHistory()) {
        std::cerr << "Feature creation history test failed\n";
        return 1;
    }
    if (!serializationPreservesGraphAndResetsHistory()) {
        std::cerr << "Document serialization roundtrip test failed\n";
        return 1;
    }
    if (!serializationRejectsForwardReferences()) {
        std::cerr << "Document serialization validation test failed\n";
        return 1;
    }
    if (!emptyDocumentRoundtripRemainsUsable()) {
        std::cerr << "Empty document roundtrip test failed\n";
        return 1;
    }
    if (!sketchElementsKeepStableIdsThroughHistoryAndSerialization()) {
        std::cerr << "Sketch element identity/history test failed\n";
        return 1;
    }
    if (!pointConstraintsRoundtripInVersionSix()) {
        std::cerr << "Version-six point constraint roundtrip failed\n";
        return 1;
    }
    if (!legacySketchMigratesToStableRectangleElement()) {
        std::cerr << "Legacy sketch migration test failed\n";
        return 1;
    }
    if (!versionTwoLineMigratesWithoutConstraint()) {
        std::cerr << "Version two sketch constraint migration failed\n";
        return 1;
    }
    if (!versionFourExtrusionMigratesToAllProfiles()) {
        std::cerr << "Version-four extrusion migration failed\n";
        return 1;
    }
    if (!unsatisfiedLineConstraintIsRejected()) {
        std::cerr << "Unsatisfied sketch line constraint was accepted\n";
        return 1;
    }
    if (!invalidSketchGeometryIsRejected()) {
        std::cerr << "Sketch geometry validation test failed\n";
        return 1;
    }
    return 0;
}
