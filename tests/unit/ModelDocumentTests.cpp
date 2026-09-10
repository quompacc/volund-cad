#include <voelund/document/DocumentSerializer.hpp>
#include <voelund/document/ModelDocument.hpp>

#include <iostream>
#include <sstream>

namespace {

bool undoAndRedoRestoreDimensions()
{
    voelund::document::ModelDocument document;
    constexpr voelund::document::FeatureId boxId =
        voelund::document::ModelDocument::initialBoxId();
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

bool newChangeClearsRedoHistory()
{
    voelund::document::ModelDocument document;
    constexpr voelund::document::FeatureId boxId =
        voelund::document::ModelDocument::initialBoxId();
    constexpr voelund::kernel::BoxDimensions first{80.0, 25.0, 12.0};
    constexpr voelund::kernel::BoxDimensions second{30.0, 20.0, 10.0};

    static_cast<void>(document.setBoxDimensions(boxId, first));
    static_cast<void>(document.undo());
    static_cast<void>(document.setBoxDimensions(boxId, second));
    return !document.canRedo();
}

bool featureIdentitySurvivesHistory()
{
    voelund::document::ModelDocument document;
    const auto featuresBefore = document.features();
    const voelund::document::FeatureId boxId = featuresBefore.front().id;
    const voelund::document::FeatureId cylinderId = featuresBefore.back().id;
    constexpr voelund::kernel::BoxDimensions changed{50.0, 40.0, 30.0};

    static_cast<void>(document.setBoxDimensions(boxId, changed));
    static_cast<void>(document.undo());
    static_cast<void>(document.redo());
    const auto featuresAfter = document.features();
    return featuresAfter.size() == 2 && featuresAfter.front().id == boxId
        && featuresAfter.back().id == cylinderId;
}

bool featureCreationParticipatesInHistory()
{
    voelund::document::ModelDocument document;
    const voelund::document::CylinderParameters parameters{
        4.0,
        10.0,
        voelund::kernel::RigidTransform{25.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        voelund::document::CylinderResultMode::separate};
    const voelund::document::FeatureId featureId = document.addCylinderFeature(
        "Cylinder 2",
        voelund::document::ModelDocument::initialCylinderId(),
        parameters);
    if (document.features().size() != 3 || document.cylinderParameters(featureId) != parameters) {
        return false;
    }
    if (!document.undo() || document.features().size() != 2 || document.contains(featureId)) {
        return false;
    }
    return document.redo() && document.features().size() == 3
        && document.contains(featureId) && document.cylinderParameters(featureId) == parameters;
}

bool serializationPreservesGraphAndResetsHistory()
{
    voelund::document::ModelDocument source;
    voelund::document::BoxParameters box =
        source.boxParameters(voelund::document::ModelDocument::initialBoxId());
    box.placement = voelund::kernel::RigidTransform{12.5, -4.0, 3.0, 0.0, 45.0, 0.0};
    static_cast<void>(source.setBoxParameters(
        voelund::document::ModelDocument::initialBoxId(), box));
    const voelund::document::CylinderParameters cylinder{
        3.5,
        11.0,
        voelund::kernel::RigidTransform{8.0, 2.0, 0.0, 5.0, 10.0, 15.0},
        voelund::document::CylinderResultMode::separate};
    const voelund::document::FeatureId thirdId = source.addCylinderFeature(
        "Cylinder with spaces",
        voelund::document::ModelDocument::initialCylinderId(),
        cylinder);

    std::ostringstream output;
    voelund::document::DocumentSerializer::write(source, output);
    std::istringstream input(output.str());
    voelund::document::ModelDocument loaded =
        voelund::document::DocumentSerializer::read(input);
    const auto features = loaded.features();
    if (features.size() != 3 || features.back().id != thirdId
        || features.back().name != "Cylinder with spaces"
        || loaded.boxParameters(voelund::document::ModelDocument::initialBoxId()) != box
        || loaded.cylinderParameters(thirdId) != cylinder || loaded.canUndo()) {
        return false;
    }
    return loaded.addBoxFeature("Box after load", {1.0, 2.0, 3.0}).value() == 4;
}

bool serializationRejectsForwardReferences()
{
    std::istringstream input(
        "VOLUND 1\nNEXT_ID 3\nFEATURES 1\n"
        "CYLINDER 2 \"Broken\" 1 5 10 0 0 0 0 0 0 JOIN\n");
    try {
        static_cast<void>(voelund::document::DocumentSerializer::read(input));
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

bool emptyDocumentRoundtripRemainsUsable()
{
    const voelund::document::ModelDocument empty =
        voelund::document::ModelDocument::empty();
    std::ostringstream output;
    voelund::document::DocumentSerializer::write(empty, output);
    std::istringstream input(output.str());
    voelund::document::ModelDocument loaded =
        voelund::document::DocumentSerializer::read(input);
    return loaded.features().empty()
        && loaded.addBoxFeature("First Box", {10.0, 20.0, 30.0}).value() == 1;
}

} // namespace

int main()
{
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
    return 0;
}
