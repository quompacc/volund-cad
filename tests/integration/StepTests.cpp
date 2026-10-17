#include "CadSessionTestSupport.hpp"

#include <voelund/kernel/StepExchange.hpp>

#include <BRepAdaptor_Curve.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <STEPControl_Writer.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

// M17 (A-STEP-01, A-ERR-01; user decision 2026-10-16): a part of three bodies (a plate named
// "Platte", its pattern copy and a cylinder) is exported as STEP and imported into a new
// document: three solids, the same bounding boxes and volumes, the names kept, one feature
// "Import 1" in the history and one undo step. An imported edge is rounded; save and open keep
// the embedded solids. A file in inches arrives in mm; a broken file, a missing one and a file
// without solids report their error and leave the document as it is. A current revision with an
// error is not exported although an older valid display remains. Deleting and suppressing an
// import take its parts along; the rollback marker after a pattern keeps its copies.
namespace {
using namespace voelund;
using application::CommandStatus;
using application::ErrorDisposition;
using application::StepExportStatus;
using application::StepImportStatus;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b, double tolerance = 1e-6) {
    return std::abs(a - b) <= tolerance * std::max(1.0, std::abs(b));
}
std::array<double, 6> bounds(const TopoDS_Shape& shape) {
    Bnd_Box box;
    BRepBndLib::AddOptimal(shape, box, false, false);
    std::array<double, 6> result{};
    box.Get(result[0], result[1], result[2], result[3], result[4], result[5]);
    return result;
}
bool sameBounds(const std::array<double, 6>& a, const std::array<double, 6>& b) {
    for (std::size_t i = 0; i < a.size(); ++i)
        if (std::abs(a[i] - b[i]) > 1e-5)
            return false;
    return true;
}
struct BodyFacts final {
    std::string name;
    double volume;
    std::array<double, 6> box;
};
// The visible bodies as the browser names them, in browser order.
std::vector<BodyFacts> facts(const application::CadSession& session) {
    std::vector<BodyFacts> result;
    const auto outline = session.modelOutline();
    int number = 0;
    for (const auto& body : outline.bodies) {
        ++number;
        const auto found =
            std::ranges::find(session.validBodies(), body.id, &features::EvaluatedBody::bodyId);
        if (found == session.validBodies().end())
            continue;
        result.push_back({body.name.empty() ? "Körper " + std::to_string(number) : body.name,
                          test::volumeOf(found->shape), bounds(found->shape)});
    }
    return result;
}
std::string fileText(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), {}};
}
std::string lower(std::string text) {
    std::ranges::transform(text, text.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}
std::size_t count(const application::CadSession& session, document::FeatureKind kind) {
    return static_cast<std::size_t>(
        std::ranges::count(session.features(), kind, &document::FeatureDescriptor::kind));
}
application::FeatureRuntimeStatus status(const application::CadSession& session,
                                         document::FeatureId feature) {
    const auto states = session.featureStates();
    const auto found = std::ranges::find(states, feature, &application::FeatureRuntimeState::id);
    require(found != states.end(), "Feature state missing");
    return found->status;
}

// A plate, its pattern copy 60 mm along X and a cylinder of its own.
void buildPart(application::CadSession& session) {
    session.newDocument();
    const auto plate = session.createBoxFeature({40, 20, 10}).feature();
    require(plate.has_value(), "Plate");
    require(session.renameBody(document::BodyId{plate->value()}, "Platte").status() ==
                CommandStatus::committed,
            "Plate named");
    application::PatternInput pattern;
    pattern.baseAxis = document::BaseAxis::x;
    pattern.count = 2;
    pattern.spacing = 60;
    const auto copies = session.createPattern(*plate, pattern);
    require(copies.status() == CommandStatus::committed, "Pattern of the plate");
    // The rollback marker after the pattern keeps its copy (M17 fix of M14).
    require(session.setRollback(*copies.feature()), "Marker after the pattern");
    require(session.validBodies().size() == 2, "The marker after a pattern keeps its copy");
    require(session.setRollback(std::nullopt), "Marker back at the end");
    require(session.createNewBodyCylinder().status() == CommandStatus::committed, "Cylinder");
    require(session.validBodies().size() == 3, "Three bodies");
}

void exportAndImport(const std::filesystem::path& directory) {
    application::CadSession source;
    buildPart(source);
    const auto original = facts(source);
    const auto file = directory / "teil.step";
    const auto written = source.exportStep(file);
    require(written.status == StepExportStatus::written && written.solids == 3,
            "Export writes the three visible bodies: " + written.message);
    require(fileText(file).find("Platte") != std::string::npos, "The plate's name is in the file");

    application::CadSession target;
    target.newDocument();
    const auto imported = target.importStep(file);
    require(imported.status == StepImportStatus::imported && imported.solids == 3 &&
                imported.feature && imported.skippedShapes == 0,
            "Import reads three solids: " + imported.message);
    require(lower(imported.sourceUnit).find("milli") != std::string::npos,
            "The source unit is mm: " + imported.sourceUnit);
    require(count(target, document::FeatureKind::stepImport) == 1 &&
                count(target, document::FeatureKind::importPart) == 2 &&
                target.features().front().name == "Import 1",
            "One import feature with two parts");
    const auto read = facts(target);
    require(read.size() == original.size(), "As many bodies as written");
    for (std::size_t index = 0; index < read.size(); ++index) {
        require(read[index].name == original[index].name,
                "Body name kept: " + original[index].name + " / " + read[index].name);
        require(near(read[index].volume, original[index].volume, 1e-6) &&
                    sameBounds(read[index].box, original[index].box),
                "Volume and bounding box kept for " + original[index].name);
    }
    require(target.undo().status() == CommandStatus::committed && target.features().empty(),
            "The import is one undo step");
    require(target.redo().status() == CommandStatus::committed && target.validBodies().size() == 3,
            "Redo brings it back");

    // An imported edge (front top edge of the plate) is rounded like any other.
    const auto import = *imported.feature;
    std::optional<document::FilletEdge> edge;
    for (TopExp_Explorer explorer(*target.featureShape(import), TopAbs_EDGE); explorer.More();
         explorer.Next()) {
        const BRepAdaptor_Curve curve(TopoDS::Edge(explorer.Current()));
        const auto middle = curve.Value((curve.FirstParameter() + curve.LastParameter()) / 2);
        if (std::abs(middle.Y()) < 1e-9 && std::abs(middle.Z() - 10) < 1e-9)
            edge = target.filletEdge(import, explorer.Current());
    }
    require(edge.has_value(), "The imported edge is named by its faces");
    const auto fillet = target.createFillet(import, std::span{&*edge, 1}, 2);
    require(fillet.status() == CommandStatus::committed, "Imported edge rounded");
    const double rounded = facts(target)[0].volume;
    require(rounded < original[0].volume - 1.0, "The rounding removes material");

    // A-ERR-01: a current revision with an error is not exported, although the last valid
    // display remains.
    auto parameters = target.filletParameters(*fillet.feature());
    parameters.radius = 50;
    require(target.editFilletParameters(*fillet.feature(), parameters,
                                        ErrorDisposition::keepEditableErrors)
                    .status() == CommandStatus::committedWithErrors,
            "A radius too large is kept with its error");
    require(!target.displayBodies().empty(), "The last valid display remains");
    const auto refused = target.exportStep(directory / "fehler.step");
    require(refused.status == StepExportStatus::invalidRevision &&
                !std::filesystem::exists(directory / "fehler.step"),
            "Export of the faulty revision refused");
    require(target.undo().status() == CommandStatus::committed, "Undo the radius");

    // Save and open keep the embedded solids; no STEP file is needed.
    const auto saved = directory / "import.volund";
    target.saveDocument(saved);
    std::filesystem::remove(file);
    application::CadSession reopened;
    reopened.loadDocument(saved);
    const auto again = facts(reopened);
    require(again.size() == 3 && again[0].name == "Platte" && near(again[0].volume, rounded) &&
                count(reopened, document::FeatureKind::importPart) == 2,
            "Save and open keep the import and the rounding");

    // Deleting the import names the rounding, not its parts; the cascade takes all.
    const auto plan = reopened.deletionPlan(import);
    require(plan.dependents.size() == 1 && plan.dependents[0] == *fillet.feature() &&
                plan.cascade.size() == 4,
            "Delete names the rounding; the parts go with the import");
    // Suppressing the import suppresses its parts; nothing is blocked.
    require(reopened.setFeatureSuppressed(import, true).status() != CommandStatus::rejected,
            "Import suppressed");
    for (const auto& feature : reopened.features())
        if (feature.kind == document::FeatureKind::importPart)
            require(status(reopened, feature.id) == application::FeatureRuntimeStatus::suppressed,
                    "The parts are suppressed with their import");
}

void inchesAndErrors(const std::filesystem::path& directory) {
    // A cube of one inch, written in inches, arrives as 25.4 mm.
    const auto inches = directory / "zoll.step";
    const kernel::StepSolid cube{BRepPrimAPI_MakeBox(25.4, 25.4, 25.4).Solid(), "Zollwürfel"};
    require(!kernel::writeStep(inches, std::span{&cube, 1}, kernel::StepLengthUnit::inch).error,
            "Inch file written");
    require(fileText(inches).find("INCH") != std::string::npos, "The file is in inches");
    application::CadSession session;
    session.newDocument();
    const auto imported = session.importStep(inches);
    require(imported.status == StepImportStatus::imported &&
                lower(imported.sourceUnit).find("inch") != std::string::npos,
            "Inch file imported: " + imported.message + " / " + imported.sourceUnit);
    const auto cubeFacts = facts(session);
    require(cubeFacts.size() == 1 && cubeFacts[0].name == "Zollwürfel" &&
                near(cubeFacts[0].volume, 25.4 * 25.4 * 25.4, 1e-6) &&
                sameBounds(cubeFacts[0].box, {0, 0, 0, 25.4, 25.4, 25.4}),
            "The inch cube is 25.4 mm");

    // Errors leave the document as it is.
    const auto before = session.features();
    const bool modified = session.isModified();
    const auto unchanged = [&] {
        return session.features() == before && session.isModified() == modified &&
               session.validBodies().size() == 1;
    };
    const auto broken = directory / "kaputt.step";
    std::ofstream(broken) << "ISO-10303-21;\nHEADER;\nFILE_DESCRIPTION(('kaputt\n";
    const auto brokenResult = session.importStep(broken);
    require(brokenResult.status == StepImportStatus::unreadable && !brokenResult.message.empty() &&
                unchanged(),
            "A broken file reports an error and changes nothing");
    require(session.importStep(directory / "fehlt.step").status == StepImportStatus::unreadable &&
                unchanged(),
            "A missing file reports an error and changes nothing");
    // A file with a face but no solid.
    const auto faceOnly = directory / "flaeche.step";
    BRepBuilderAPI_MakePolygon square(gp_Pnt(0, 0, 0), gp_Pnt(10, 0, 0), gp_Pnt(10, 10, 0),
                                      gp_Pnt(0, 10, 0), true);
    STEPControl_Writer writer;
    require(writer.Transfer(BRepBuilderAPI_MakeFace(square.Wire()).Face(), STEPControl_AsIs) ==
                    IFSelect_RetDone &&
                writer.Write(faceOnly.string().c_str()) == IFSelect_RetDone,
            "Face file written");
    const auto faceResult = session.importStep(faceOnly);
    require(faceResult.status == StepImportStatus::noSolids && faceResult.skippedShapes >= 1 &&
                unchanged(),
            "A file without solids reports it and changes nothing");
    // Nothing visible: nothing to export.
    require(session.setBodyHidden(document::BodyId{before.front().id.value()}, true),
            "Cube hidden");
    require(session.exportStep(directory / "leer.step").status == StepExportStatus::noBodies,
            "Nothing visible to export");
}

} // namespace

int main() {
    try {
        const auto directory = std::filesystem::temp_directory_path() / "voelund-m17-step";
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(directory);
        exportAndImport(directory);
        inchesAndErrors(directory);
        std::filesystem::remove_all(directory);
        std::cout << "PASS M17 STEP: three bodies exported and imported with names, volumes and "
                     "bounds, one undo step, imported edge rounded, save/open without the file, "
                     "faulty revision not exported, inches in mm, broken/missing/solid-less files "
                     "change nothing, import parts deleted and suppressed with the import, "
                     "marker after a pattern keeps its copy\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
