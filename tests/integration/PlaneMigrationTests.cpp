#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>
#include <voelund/document/DocumentSerializer.hpp>
#include <voelund/document/SketchPlaneValidation.hpp>
#include <voelund/features/GeometryEvaluationError.hpp>
#include <voelund/features/SketchConstraintAnnotations.hpp>
#include <voelund/features/SketchEvaluation.hpp>
#include <voelund/features/SketchSupportResolver.hpp>

namespace {
using namespace voelund;
void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
std::string bytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    require(bool(input), "Fixture missing");
    return {std::istreambuf_iterator<char>(input), {}};
}
document::SketchParameters sketch(document::SketchPlane plane) {
    return {
        plane, {{{document::SketchElementId{1}, document::SketchRectangle{1, 2, 6, 8}}}, 2}, {}};
}
void legacyVersions(const std::filesystem::path& fixtures) {
    for (int version = 1; version <= 16; ++version) {
        std::ostringstream name;
        name << 'v' << std::setw(2) << std::setfill('0') << version << ".volund";
        const auto path = fixtures / name.str();
        const auto original = bytes(path);
        for (const auto* axes :
             {"0 0 2 3 0 0", "0 0 0.5 1 0 0", "0 0 1 1 0 0.000000005", "0 0 1e200 1 0 0"}) {
            auto changed = original;
            const auto at = changed.find("0 0 30 0 0 1 1 0 0");
            require(at != std::string::npos, "Fixture plane missing");
            changed.replace(at, std::string("0 0 30 0 0 1 1 0 0").size(),
                            std::string("0 0 30 ") + axes);
            std::istringstream input(changed), repeated(changed);
            const auto imported = document::DocumentSerializer::readWithReport(input);
            const auto second = document::DocumentSerializer::readWithReport(repeated);
            const auto& issues = imported.report->planeIssues;
            require(issues.size() == 1 && issues[0].feature == document::FeatureId{3} &&
                        issues[0].original == imported.snapshot.toDocument()
                                                  .sketchParameters(document::FeatureId{3})
                                                  .plane &&
                        issues[0].message.find("migration") != std::string::npos &&
                        imported.snapshot == second.snapshot && imported.report == second.report,
                    "Legacy plane report lost original values or determinism");
            std::ostringstream output;
            document::DocumentSerializer::write(imported.snapshot.toDocument(), output);
            std::istringstream saved(output.str());
            const auto roundtrip = document::DocumentSerializer::readWithReport(saved);
            require(roundtrip.snapshot == imported.snapshot &&
                        !roundtrip.report,
                    "Save silently normalized an unresolved legacy plane");
        }
        std::istringstream valid(original);
        require(document::DocumentSerializer::readWithReport(valid).report->planeIssues.empty(),
                "Valid fixture was incorrectly marked for plane migration");
        require(bytes(path) == original, "Fixture was modified");
    }
}
template <class Operation> void rejectsGeometry(Operation operation) {
    bool rejected = false;
    try {
        operation();
    } catch (const features::GeometryEvaluationError&) {
        rejected = true;
    }
    require(rejected, "Noncanonical legacy plane reached geometry");
}
void boundaries() {
    const auto shape = kernel::ShapeFactory::makeBox({20, 20, 30});
    for (const auto& plane : {document::SketchPlane{0, 0, 30, 0, 0, 2, 3, 0, 0},
                              document::SketchPlane{0, 0, 30, 0, 0, 1, 1, 0, 0.5e-8}}) {
        auto parameters = sketch(plane);
        application::CadSession session;
        const auto stamp = session.sceneStatus().document;
        const auto result = session.createSketchFeature(document::FeatureId{1}, parameters);
        require(result.reason() == application::RejectionReason::invalidInput &&
                    session.sceneStatus().document == stamp && !session.canUndo(),
                "New sketch accepted noncanonical axes or mutated history");
        bool rejected = false;
        try {
            application::SketchEditorSession editor(session, document::FeatureId{1}, plane);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        require(rejected, "Sketch editor accepted ambiguous plane");
        rejectsGeometry([&] { static_cast<void>(features::evaluateSketchElements(parameters)); });
        rejectsGeometry(
            [&] { static_cast<void>(features::evaluateSketchProfileFaces(parameters)); });
        rejectsGeometry(
            [&] { static_cast<void>(features::evaluateSketchConstraintAnnotations(parameters)); });
        rejectsGeometry(
            [&] { static_cast<void>(features::updateSketchSupportReferences(parameters, shape)); });
        require(parameters.plane == plane, "Failed support resolution mutated plane");
    }
}
void lifecycle(const std::filesystem::path& directory) {
    const auto path = directory / "d10b-legacy-plane.volund";
    const std::string original = "VOLUND 1\nNEXT_ID 4\nFEATURES 3\n"
                                 "BOX 1 \"Support\" 20 20 30 0 0 0 0 0 0\n"
                                 "SKETCH 2 \"Ambiguous plane\" 1 0 0 30 0 0 2 3 0 0 1 2 6 8\n"
                                 "EXTRUSION 3 \"Extrusion\" 2 5\n";
    {
        std::ofstream output(path, std::ios::binary);
        output << original;
    }
    application::CadSession session;
    session.loadDocument(path);
    require(bytes(path) == original && !session.isModified() &&
                !session.sceneStatus().isCurrent() && session.displayShape().IsNull() &&
                session.lastImportReport()->planeIssues.size() == 1,
            "Load changed legacy values or exposed a current shape");
    const auto states = session.featureStates();
    require(states[1].status == application::FeatureRuntimeStatus::error &&
                states[2].status == application::FeatureRuntimeStatus::blocked &&
                states[1].message.find("migration") != std::string::npos,
            "Legacy ambiguity was not visible as an evaluation failure");
    session.saveDocument(directory / "d10b-legacy-resaved.volund");
    require(session.lastImportReport()->planeIssues[0].original.normalZ == 2,
            "Save discarded original plane provenance");
    // Explicit command, never automatic migration: choose unit axes and keep distance 5.
    auto corrected = session.sketchParameters(document::FeatureId{2});
    corrected.plane = {0, 0, 30, 0, 0, 1, 1, 0, 0};
    require(session.editSketchParameters(document::FeatureId{2}, corrected).status() ==
                    application::CommandStatus::committed &&
                session.sceneStatus().isCurrent(),
            "Explicit plane correction did not recover evaluation");
    GProp_GProps properties;
    BRepGProp::VolumeProperties(session.currentShape(), properties);
    require(std::abs(properties.Mass() - 12150.0) < 1e-6,
            "Explicit unit plane did not give requested 5 mm extrusion");
    require(session.undo().status() == application::CommandStatus::committedWithErrors &&
                session.sketchParameters(document::FeatureId{2}).plane.normalZ == 2 &&
                !session.sceneStatus().isCurrent(),
            "Undo normalized legacy history");
    require(session.redo().status() == application::CommandStatus::committed &&
                session.sceneStatus().isCurrent(),
            "Redo did not restore explicit correction");
}
} // namespace
int main(int argc, char** argv) {
    try {
        require(argc == 3, "Pass fixture and test output directories");
        legacyVersions(argv[1]);
        boundaries();
        lifecycle(argv[2]);
        std::cout << "PASS legacy plane preservation, canonical input gate, evaluation guard, "
                     "explicit repair history\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
