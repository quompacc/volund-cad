#include "../../src/document/NativePrimitiveCodec.hpp"
#include "../../src/document/NativeSketchGeometry.hpp"
#include <BinDrivers.hxx>
#include <TDataStd_Integer.hxx>
#include <TDocStd_Application.hxx>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <source_location>
#include <voelund/application/CadSession.hpp>
#include <voelund/document/DocumentSerializer.hpp>
using namespace voelund;
using namespace document;
using namespace application;
namespace {
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
void committed(const CommandResult& r,
               const std::source_location location = std::source_location::current()) {
    if (r.status() != CommandStatus::committed)
        throw std::runtime_error("Expected valid repair at line " +
                                 std::to_string(location.line()) + ": " + r.diagnostic() +
                                 " status " + std::to_string(static_cast<int>(r.status())));
}
FeatureRuntimeStatus status(const CadSession& session, FeatureId id) {
    const auto states = session.featureStates();
    const auto found = std::ranges::find(states, id, &FeatureRuntimeState::id);
    require(found != states.end(), "Missing feature state");
    return found->status;
}
ModelDocument fixture(bool edges) {
    auto m = ModelDocument::empty();
    const auto box = m.addBoxFeature("Box", {10, 10, 10});
    SketchParameters p{{0, 0, 10, 0, 0, 1, 1, 0, 0},
                       {{{SketchElementId{1}, SketchRectangle{2, 2, 5, 5}},
                         {SketchElementId{2}, SketchLine{{2, 2}, {7, 7}}}},
                        3},
                       {}};
    if (edges)
        for (auto kind : {SketchConstraintKind::coordinateX, SketchConstraintKind::coordinateY})
            p.constraints.values.push_back({SketchConstraintId{p.constraints.nextId++},
                                            kind,
                                            {SketchElementId{2}, SketchPointPosition::start},
                                            std::nullopt,
                                            2,
                                            std::nullopt,
                                            {3, .5}});
    const auto sketch = m.addSketchFeature("Legacy", box, p);
    (void)m.addExtrusionFeature("Pad", sketch, {2, {SketchProfileId{1}}});
    (void)m.addBoxFeature("Independent", {2, 2, 2});
    return m;
}
void write(const ModelDocument& model, const std::filesystem::path& path) {
    std::ofstream file(path, std::ios::binary);
    DocumentSerializer::write(model, file);
}
features::SketchSupportCandidate top(const SketchReferenceInspection& inspection) {
    const auto it = std::ranges::find_if(inspection.review.candidates,
                                         [](const auto& c) { return c.matchesOriginalPlane; });
    require(it != inspection.review.candidates.end(), "Missing top proposal");
    return *it;
}
void checkPending(const CadSession& s) {
    require(s.sketchParameters(FeatureId{2}).repairReason == SketchRepairReason::legacySupport &&
                status(s, FeatureId{2}) == FeatureRuntimeStatus::error &&
                status(s, FeatureId{3}) == FeatureRuntimeStatus::blocked &&
                status(s, FeatureId{4}) == FeatureRuntimeStatus::current,
            "Repair error was not isolated");
}
void supportFlow(const std::filesystem::path& directory) {
    const auto file = directory / "repair-unbound.volund";
    write(fixture(false), file);
    CadSession session;
    session.loadDocument(file);
    checkPending(session);
    const auto snapshot = session.identifiedSnapshot();
    require(!session.isModified() && !session.canUndo(), "Open created edit history");
    auto inspection = session.inspectSketchReferences(FeatureId{2});
    auto choice = top(inspection);
    auto stale = inspection;
    ++stale.stamp.revision;
    require(session.repairSketchSupport(stale, choice).status() == CommandStatus::rejected &&
                session.identifiedSnapshot() == snapshot && !session.canUndo(),
            "Stale repair mutated state");
    auto wrongFeature = inspection;
    wrongFeature.feature = FeatureId{1};
    require(session.repairSketchSupport(wrongFeature, choice).status() == CommandStatus::rejected,
            "Non-sketch request accepted");
    auto foreign = choice;
    foreign.binding.reference.body = BodyId{999};
    require(session.repairSketchSupport(inspection, foreign).status() == CommandStatus::rejected,
            "Forged candidate accepted");
    auto changed = inspection;
    changed.review.original.plane.originX = 1;
    require(session.repairSketchSupport(changed, choice).status() == CommandStatus::rejected,
            "Changed slot accepted");
    committed(session.repairSketchSupport(inspection, choice));
    require(!session.sketchParameters(FeatureId{2}).repairReason &&
                status(session, FeatureId{3}) == FeatureRuntimeStatus::current,
            "Repair did not restore successor");
    const auto repaired = session.identifiedSnapshot();
    require(session.repairSketchSupport(inspection, choice).status() == CommandStatus::rejected,
            "Old request reused");
    require(session.undo().status() == CommandStatus::committedWithErrors,
            "Undo must restore repair errors");
    checkPending(session);
    require(session.identifiedSnapshot() == snapshot && !session.canUndo(),
            "Support repair was not one undo step");
    committed(session.redo());
    require(session.identifiedSnapshot() == repaired, "Redo changed binding");
    session.saveDocument(directory / "repair-fixed.volund");
    session.loadDocument(directory / "repair-fixed.volund");
    require(session.identifiedSnapshot() == repaired &&
                status(session, FeatureId{3}) == FeatureRuntimeStatus::current,
            "Repaired Save/Open failed");
    session.loadDocument(file);
    checkPending(session);
    session.saveDocument(directory / "repair-pending.volund");
    session.loadDocument(directory / "repair-pending.volund");
    checkPending(session);
    std::cout << "support repair, stale slots, atomic undo/redo, isolated errors and both "
                 "persistent states passed\n";
}
void edgeFlow(const std::filesystem::path& directory) {
    const auto path = directory / "repair-edges.volund";
    write(fixture(true), path);
    CadSession session;
    session.loadDocument(path);
    auto inspection = session.inspectSketchReferences(FeatureId{2});
    const auto original = session.identifiedSnapshot();
    require(session.repairSketchSupport(inspection, top(inspection)).status() ==
                    CommandStatus::rejected &&
                session.identifiedSnapshot() == original && !session.canUndo(),
            "Unaccepted remaining edge errors committed");
    require(session.repairSketchSupport(inspection, top(inspection),
                                        ErrorDisposition::keepEditableErrors)
                    .status() == CommandStatus::committedWithErrors,
            "Explicit partial repair did not retain edge error");
    require(session.sketchParameters(FeatureId{2}).repairReason == SketchRepairReason::legacyEdges,
            "Support repair silently removed legacy edge state");
    inspection = session.inspectSketchReferences(FeatureId{2});
    const ProjectedEdgeReference edge{session.documentId(),
                                      BodyId{1},
                                      PrimitiveEdgeOrigin{FeatureId{1},
                                                          kernel::PrimitiveRole::boxNegativeY,
                                                          kernel::PrimitiveRole::boxPositiveZ},
                                      {EdgePointKind::parameter, .5}};
    const std::vector slots{SketchConstraintId{1}, SketchConstraintId{2}};
    const std::vector duplicate{SketchConstraintId{1}, SketchConstraintId{1}};
    require(session.repairSketchEdgeReferences(inspection, duplicate, edge).status() ==
                CommandStatus::rejected,
            "Duplicate slots accepted");
    auto missing = edge;
    std::get<PrimitiveEdgeOrigin>(missing.origin.value).feature = FeatureId{999};
    require(session.repairSketchEdgeReferences(inspection, slots, missing).status() ==
                CommandStatus::rejected,
            "Missing edge accepted");
    committed(session.repairSketchEdgeReferences(inspection, slots, edge));
    const auto repaired = session.sketchParameters(FeatureId{2});
    require(!repaired.repairReason && repaired.constraints.values[0].auxiliaryValues.empty() &&
                repaired.constraints.values[1].projectedEdge == edge &&
                status(session, FeatureId{3}) == FeatureRuntimeStatus::current,
            "Typed edge repair failed");
    require(session.undo().status() == CommandStatus::committedWithErrors,
            "Edge undo must restore repair errors");
    require(session.sketchParameters(FeatureId{2}).repairReason == SketchRepairReason::legacyEdges,
            "Undo lost unresolved edge state");
    committed(session.redo());
    session.saveDocument(directory / "repair-edges-fixed.volund");
    session.loadDocument(directory / "repair-edges-fixed.volund");
    require(session.sketchParameters(FeatureId{2}) == repaired, "Edge repair persistence failed");
    auto broken = repaired;
    for (auto& c : broken.constraints.values)
        c.projectedEdge = missing;
    require(session.editSketchParameters(FeatureId{2}, broken, ErrorDisposition::keepEditableErrors)
                    .status() == CommandStatus::committedWithErrors,
            "Broken typed reference fixture failed");
    committed(session.repairSketchEdgeReferences(session.inspectSketchReferences(FeatureId{2}),
                                                 slots, edge));
    std::cout << "legacy and broken typed edges, explicit error disposition and paired-slot "
                 "transaction passed\n";
}
void codec() {
    auto m = fixture(false);
    auto snapshot = DocumentSnapshot::capture(m).markLegacySketchReferencesForRepair();
    const auto identified = IdentifiedDocumentSnapshot::restore(m.documentId(), snapshot);
    Handle(TDocStd_Application) app = new TDocStd_Application;
    BinDrivers::DefineFormat(app);
    Handle(TDocStd_Document) doc;
    app->NewDocument("BinOcaf", doc);
    detail::NativePrimitiveCodec::write(doc, identified);
    require(detail::NativePrimitiveCodec::read(doc) == identified,
            "Schema4 marker roundtrip failed");
    for (int version : {1, 2, 3}) {
        TDataStd_Integer::Set(doc->Main().FindChild(2), version);
        bool rejected = false;
        try {
            (void)detail::NativePrimitiveCodec::read(doc);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        require(rejected, "Repair field accepted under old schema");
    }
    auto invalid = snapshot.sketchParameters(FeatureId{2});
    for (const auto reason :
         {static_cast<SketchRepairReason>(0), static_cast<SketchRepairReason>(3),
          SketchRepairReason::legacyEdges}) {
        invalid.repairReason = reason;
        bool rejected = false;
        try {
            detail::validateNativeSketchGeometry(invalid);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        require(rejected, "Invalid or inconsistent repair reason accepted");
    }
    app->Close(doc);
    std::cout << "schema4 repair marker and older-schema rejection passed\n";
}
} // namespace
int main(int argc, char** argv) {
    try {
        if (argc == 3 && std::string(argv[1]) == "read-pending") {
            CadSession s;
            s.loadDocument(argv[2]);
            checkPending(s);
            return 0;
        }
        if (argc == 3 && std::string(argv[1]) == "read-fixed") {
            CadSession s;
            s.loadDocument(argv[2]);
            require(s.sceneStatus().isCurrent() && !s.sketchParameters(FeatureId{2}).repairReason,
                    "Cross-platform repaired file failed");
            return 0;
        }
        require(argc == 2, "Expected artifact directory");
        supportFlow(argv[1]);
        edgeFlow(argv[1]);
        codec();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
