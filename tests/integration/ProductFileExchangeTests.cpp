#include "../BoundSketchFixture.hpp"
#include "../CommandResultAssertions.hpp"
#include "../TestPaths.hpp"
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <numbers>
#include <voelund/application/CadSession.hpp>
#include <voelund/document/DocumentSerializer.hpp>

namespace {
using namespace voelund;
using namespace document;
namespace fs = std::filesystem;
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
ModelDocument mixed() {
    auto model = ModelDocument::empty();
    const auto a = model.addBoxFeature("A", {40, 30, 20});
    const auto b = model.addBoxFeature("B", {10, 10, 10});
    const auto sketch =
        model.addSketchFeature("Skizze Ω", a,
                               {{0, 0, 20, 0, 0, 1, 1, 0, 0},
                                {{{SketchElementId{9}, SketchRectangle{1, 1, 5, 5}},
                                  {SketchElementId{2}, SketchCircle{{8, 8}, 2}, true}},
                                 10},
                                {{{SketchConstraintId{8},
                                   SketchConstraintKind::radius,
                                   {SketchElementId{2}, SketchPointPosition::center},
                                   std::nullopt,
                                   2}},
                                 11}});
    (void)model.addCylinderFeature("C", b,
                                   {2, 5, {60, 0, 0, {}, {}, {}}, CylinderResultMode::separate});
    (void)model.addExtrusionFeature("A erweitert", sketch,
                                    {3, {}, {*SketchRegionId::parse("o:9.0,9.1,9.2,9.3")}});
    test::bindTop(model, sketch);
    const DocumentId identity{UINT64_MAX, 0x0123456789abcdefULL};
    return DocumentSnapshot::capture(model)
        .remapSupportNamespace(model.documentId(), identity)
        .toDocument(identity);
}
void checkGeometry(const application::CadSession& session) {
    const auto result = features::FeatureGraph{}.evaluate(
        session.identifiedSnapshot().snapshot().toDocument(session.documentId()));
    const std::map<std::uint64_t, std::pair<std::uint64_t, double>> expected{
        {1, {5, 24048}}, {2, {2, 1000}}, {4, {4, 20 * std::numbers::pi}}};
    require(session.sceneStatus().isCurrent() && result.bodies.size() == expected.size(),
            "Lost current bodies");
    for (const auto& body : result.bodies) {
        const auto& [source, volume] = expected.at(body.bodyId.value());
        GProp_GProps properties;
        BRepGProp::VolumeProperties(body.shape, properties);
        require(body.sourceFeature.value() == source && BRepCheck_Analyzer(body.shape).IsValid() &&
                    std::abs(properties.Mass() - volume) <= 1e-7 + 1e-12 * volume,
                "Body identity, validity or analytic volume changed");
    }
}
void checkClean(const application::CadSession& session) {
    require(!session.isModified() && !session.canUndo() && !session.canRedo() &&
                !session.lastImportReport(),
            "Native open retained history, dirty state or legacy report");
}
void broken(application::CadSession& session) {
    require(session.selectFeature(ModelDocument::initialCylinderId()), "Missing cylinder");
    auto parameters = session.selectedCylinderParameters();
    parameters.placement.translationZ = 10;
    require(session.editSelectedCylinderParameters(
                       parameters, application::ErrorDisposition::keepEditableErrors)
                    .status() == application::CommandStatus::committedWithErrors,
            "Error fixture was not explicitly committed");
}
void checkError(application::CadSession& session) {
    require(session.selectFeature(ModelDocument::initialCylinderId()), "Missing error cylinder");
    require(!session.sceneStatus().isCurrent() &&
                session.featureStates().back().status == application::FeatureRuntimeStatus::error &&
                session.selectedCylinderParameters().placement.translationZ == 10,
            "Error state lost or stale geometry exposed");
}
void seed(const ModelDocument& model, const fs::path& path) {
    std::ofstream output(path, std::ios::binary);
    DocumentSerializer::write(model, output);
    output.close();
    require(!output.fail(), "Seed write failed");
}
void create(const fs::path& root) {
    fs::create_directories(root);
    // Public product serializer seeds fixed IDs; exchanged files are all CadSession saves.
    seed(mixed(), root / "seed.volund");
    application::CadSession session;
    session.loadDocument(root / "seed.volund");
    session.saveDocument(root / "mixed.volund");
    application::CadSession initial;
    seed(initial.identifiedSnapshot().snapshot().toDocument({0x123456789abcdef0ULL, UINT64_MAX}),
         root / "seed.volund");
    session.loadDocument(root / "seed.volund");
    broken(session);
    session.saveDocument(root / "error.volund");
    fs::remove(root / "seed.volund");
}
void read(const fs::path& root, const fs::path& rewritten) {
    application::CadSession session;
    session.loadDocument(root / "mixed.volund");
    checkClean(session);
    const auto expected = mixed();
    require(session.documentId() == expected.documentId() &&
                session.identifiedSnapshot().snapshot() == DocumentSnapshot::capture(expected),
            "Mixed product snapshot changed");
    checkGeometry(session);
    const auto before = session.identifiedSnapshot();
    require(session.selectFeature(FeatureId{5}), "Missing extrusion");
    auto parameters = session.selectedExtrusionParameters();
    parameters.distance = 4;
    require(session.editSelectedExtrusionParameters(parameters).status() ==
                    application::CommandStatus::committed &&
                session.isModified(),
            "Exchanged file not editable");
    require(session.undo().status() == application::CommandStatus::committed &&
                !session.isModified() && session.identifiedSnapshot() == before,
            "Undo failed to restore saved exchanged state");
    require(session.redo().status() == application::CommandStatus::committed &&
                session.isModified(),
            "Redo failed");
    require(session.undo().status() == application::CommandStatus::committed, "Second undo failed");
    if (!rewritten.empty()) {
        fs::create_directories(rewritten);
        session.saveDocument(rewritten / "mixed.volund");
        require(session.documentId() == before.id() && !session.isModified(),
                "Save-As changed identity/dirty");
    }
    session.loadDocument(root / "error.volund");
    checkClean(session);
    require(session.displayShape().IsNull(), "Open exposed geometry from previous document");
    checkError(session);
    application::CadSession errorExpected;
    broken(errorExpected);
    require(session.documentId() == DocumentId{0x123456789abcdef0ULL, UINT64_MAX} &&
                session.identifiedSnapshot().snapshot() ==
                    errorExpected.identifiedSnapshot().snapshot(),
            "Error snapshot changed");
    const auto failed = session.identifiedSnapshot();
    auto cylinder = session.selectedCylinderParameters();
    cylinder.placement.translationZ = 0;
    require(session.editSelectedCylinderParameters(cylinder).status() ==
                    application::CommandStatus::committed &&
                session.sceneStatus().isCurrent() && !session.currentShape().IsNull() &&
                session.isModified(),
            "Repair failed");
    require(session.undo().status() == application::CommandStatus::committedWithErrors &&
                session.identifiedSnapshot() == failed && !session.isModified(),
            "Undo repair lost saved error");
    checkError(session);
    require(session.redo().status() == application::CommandStatus::committed &&
                session.sceneStatus().isCurrent(),
            "Redo repair failed");
    require(session.undo().status() == application::CommandStatus::committedWithErrors,
            "Restore error for rewrite failed");
    if (!rewritten.empty()) {
        session.saveDocument(rewritten / "error.volund");
        require(session.identifiedSnapshot() == failed && !session.isModified(),
                "Error rewrite changed snapshot");
    }
}
} // namespace
int main(int argc, char** argv) {
    try {
        if (argc == 1) {
            const auto root = test::temporaryFilePath("product-exchange-" +
                                                      documentIdToHex(generateDocumentId()));
            fs::create_directory(root);
            try {
                create(root / "source");
                read(root / "source", root / "rewrite");
                read(root / "rewrite", {});
            } catch (...) {
                fs::remove_all(root);
                throw;
            }
            fs::remove_all(root);
        } else {
            require(argc == 3 || argc == 4,
                    "Usage: create DIRECTORY | read DIRECTORY [REWRITE_DIRECTORY]");
            const std::string mode = argv[1];
            if (mode == "create") {
                require(argc == 3, "create takes one directory");
                create(argv[2]);
            } else {
                require(mode == "read", "Unknown mode");
                read(argv[2], argc == 4 ? fs::path(argv[3]) : fs::path{});
            }
        }
        std::cout << "PASS product exchange: full snapshots/IDs, three analytic bodies, edits, "
                     "Save-As, error repair/undo/redo\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
