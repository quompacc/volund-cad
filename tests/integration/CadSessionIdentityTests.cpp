#include "../../src/features/FeatureEvaluationDetail.hpp"
#include "../CommandResultAssertions.hpp"
#include "../TestPaths.hpp"
#include <fstream>
#include <iostream>
#include <limits>
#include <voelund/application/CadSession.hpp>
#include <voelund/kernel/OcctCompatibility.hpp>

namespace voelund::application {
class CadSessionPersistenceTestAccess {
  public:
    static void failSave(CadSession& session, const std::filesystem::path& path) {
        session.saveDocumentUsing(
            path, [](const auto&, auto) { throw std::runtime_error("Injected store failure"); });
    }
    static void exhaust(CadSession& session) { session.generation_ = UINT64_MAX; }
};
} // namespace voelund::application
namespace {
using namespace voelund;
using application::CadSession;
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class Action> void fails(Action action) {
    bool caught = false;
    try {
        action();
    } catch (const std::exception&) {
        caught = true;
    }
    require(caught, "Expected failure missing");
}
void lifecycle(const std::filesystem::path& root) {
    CadSession session, independent;
    require(session.documentId() != independent.documentId(), "New sessions share identity");
    const auto original = session.identifiedSnapshot();
    const auto copy = session;
    require(copy.identifiedSnapshot() == original, "Value copy changed identity");
    const auto created = test::requireCommittedFeature(session.createBoxFeature());
    require(session.documentId() == original.id(), "Edit changed identity");
    require(session.undo().status() == application::CommandStatus::committed &&
                session.documentId() == original.id(),
            "Undo changed identity");
    require(session.redo().status() == application::CommandStatus::committed &&
                session.documentId() == original.id(),
            "Redo changed identity");
    const auto beforeSave = session.identifiedSnapshot();
    session.saveDocument(root / "one.volund");
    session.saveDocument(root / "two.volund");
    require(session.identifiedSnapshot() == beforeSave && !session.isModified(),
            "Save-As changed namespace");
    const auto path = session.currentFile();
    fails([&] {
        application::CadSessionPersistenceTestAccess::failSave(session, root / "failed.volund");
    });
    require(session.identifiedSnapshot() == beforeSave && session.currentFile() == path &&
                !session.isModified(),
            "Failed save changed identity/state");
    {
        std::ofstream malformed(root / "bad.volund");
        malformed << "broken";
    }
    fails([&] { session.loadDocument(root / "bad.volund"); });
    require(session.identifiedSnapshot() == beforeSave && session.currentFile() == path &&
                session.canUndo(),
            "Failed open changed identity/history");
    session.loadDocument(root / "one.volund");
    const auto imported = session.identifiedSnapshot();
    require(imported.id() == beforeSave.id() && imported.snapshot() == beforeSave.snapshot(),
            "Native open did not preserve namespace with exact content");
    session.loadDocument(root / "one.volund");
    require(session.documentId() == imported.id(), "Repeated native open changed file identity");
    require(session.selectFeature(created), "Lost feature after import");
    const auto prior = session.identifiedSnapshot();
    const auto shape = session.currentShape();
    const auto stamp = session.sceneStatus().document;
    session.duplicateDocument();
    require(session.documentId() != prior.id() &&
                session.identifiedSnapshot().snapshot() == prior.snapshot() &&
                session.currentFile().empty() && session.isModified() && !session.canUndo() &&
                !session.canRedo() && session.selectedFeature() == created &&
                session.currentShape().IsSame(shape) && session.sceneStatus().document != stamp &&
                session.sceneStatus().isCurrent() && !session.lastImportReport(),
            "Duplicate did not publish coherent unsaved namespace");
    const auto duplicateId = session.documentId();
    session.saveDocument(root / "duplicate.volund");
    require(session.documentId() == duplicateId && !session.isModified(),
            "Saving duplicate changed identity");
    session.newDocument();
    require(session.documentId() != duplicateId && session.features().empty() &&
                !session.isModified() && session.currentFile().empty() && !session.canUndo(),
            "New document did not reset identity/state");
    const auto emptyId = session.documentId();
    session.duplicateDocument();
    require(session.documentId() != emptyId && session.features().empty() && session.isModified(),
            "Empty duplicate failed");
}
void failureBoundaries(const std::filesystem::path& root) {
    CadSession session;
    const auto prior = session.identifiedSnapshot();
    application::CadSessionPersistenceTestAccess::exhaust(session);
    fails([&] { session.newDocument(); });
    fails([&] { session.duplicateDocument(); });
    fails([&] { session.loadDocument(root / "one.volund"); });
    require(session.identifiedSnapshot() == prior, "Generation overflow changed namespace");
}
void duplicateFailedGeometry() {
    CadSession session;
    require(session.selectFeature(document::ModelDocument::initialBoxId()), "Select box failed");
    const auto previousHook = features::detail::beforeFeatureEvaluation;
    features::detail::beforeFeatureEvaluation = [](document::FeatureId id) {
        if (id == document::ModelDocument::initialCylinderId())
            throw kernel::KernelError(kernel::KernelOperation::fuse, "Injected geometry failure");
    };
    auto dimensions = session.selectedBoxDimensions();
    dimensions.width += 1;
    try {
        const auto result = session.editSelectedBoxDimensions(
            dimensions, application::ErrorDisposition::keepEditableErrors);
        require(result.status() == application::CommandStatus::committedWithErrors,
                "Missing editable error");
    } catch (...) {
        features::detail::beforeFeatureEvaluation = previousHook;
        throw;
    }
    features::detail::beforeFeatureEvaluation = previousHook;
    const auto prior = session.identifiedSnapshot();
    require(!session.sceneStatus().isCurrent(), "Failed geometry marked current");
    session.duplicateDocument();
    require(session.documentId() != prior.id() &&
                session.identifiedSnapshot().snapshot() == prior.snapshot() &&
                !session.sceneStatus().isCurrent() && session.isModified() && !session.canUndo(),
            "Duplicate promoted stale geometry or lost failed parameters");
}
} // namespace
int main() {
    const auto root = test::temporaryFilePath(
        "identity-" + document::documentIdToHex(document::generateDocumentId()));
    try {
        std::filesystem::create_directory(root);
        lifecycle(root);
        failureBoundaries(root);
        duplicateFailedGeometry();
        std::filesystem::remove_all(root);
        std::cout << "PASS session identity: New, edit, history, Save-As, legacy Open, duplicate, "
                     "failures\n";
        return 0;
    } catch (const Standard_Failure& error) {
        std::cerr << kernel::occtFailureDiagnostic(error) << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
    }
    std::error_code cleanup;
    std::filesystem::remove_all(root, cleanup);
    return 1;
}
