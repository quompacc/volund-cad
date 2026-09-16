#include "../../src/document/ModelDocumentImpl.hpp"
#include <voelund/document/DocumentTransaction.hpp>
#include <voelund/application/CadSession.hpp>
#include <voelund/kernel/OcctCompatibility.hpp>
#include <iostream>
#include <stdexcept>

namespace voelund::document {
struct ModelDocumentBackendTestAccess {
    static IdentifiedDocumentSnapshot native(const ModelDocument& model) {
        return model.impl_->native.snapshot(); // Read attributes, not the derived query view.
    }
};
}
namespace {
using namespace voelund;
using namespace document;
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void parity(const ModelDocument& model) {
    const auto native = ModelDocumentBackendTestAccess::native(model);
    require(native.id() == model.documentId() && native.snapshot() == DocumentSnapshot::capture(model),
            "Product query view diverged from native OCAF attributes");
}
void productBackend() {
    auto model = ModelDocument::empty();
    const auto id = model.documentId();
    parity(model);
    const auto box = model.addBoxFeature("Base", {10, 20, 30});
    const auto baseline = DocumentSnapshot::capture(model);
    DocumentTransaction change(model);
    auto& edit = change.edit();
    require(edit.documentId() == id, "Transaction candidate changed namespace");
    const auto cylinder = edit.addCylinderFeature("Independent", box,
        {2, 5, {}, CylinderResultMode::separate});
    const auto sketch = edit.addSketchFeature("Sketch", box,
        {{0, 0, 30, 0, 0, 1, 1, 0, 0},
         SketchElements{{{SketchElementId{1}, SketchRectangle{1, 1, 4, 4}}}, 2}});
    const auto extrusion = edit.addExtrusionFeature("Extrusion", sketch, {3, {}});
    parity(edit);
    require(!edit.canUndo() && baseline == DocumentSnapshot::capture(model),
            "Intermediate candidate edits published or created history");
    require(change.prepare(), "Compound preparation failed");
    parity(model);
    require(change.commit() && model.documentId() == id, "Compound commit identity");
    parity(model);
    const auto after = DocumentSnapshot::capture(model);
    auto copy = model;
    require(copy.documentId() == id && copy.undo(), "Value copy lost namespace/history");
    parity(copy);
    require(after == DocumentSnapshot::capture(model), "Copy undo mutated original native state");
    require(model.undo() && baseline.matchesContent(model), "Compound undo failed");
    parity(model);
    require(model.redo() && after == DocumentSnapshot::capture(model), "Compound redo failed");
    parity(model);
    require(model.undo(), "Prepare branch");
    const auto replacement = model.addBoxFeature("Replacement", {2, 3, 4});
    require(replacement.value() > extrusion.value() && !model.canRedo() && !model.contains(cylinder),
            "Native branch reused IDs or retained discarded history");
    parity(model);
    const auto imported = after.toDocument(id);
    require(imported.documentId() == id && !imported.canUndo(), "Snapshot restore history/identity");
    parity(imported);
}
void sessionIdentity() {
    application::CadSession session;
    const auto initial = session.documentId();
    session.newDocument();
    require(session.documentId() != initial, "New retained old namespace");
    const auto id = session.documentId();
    require(session.createBoxFeature().status() == application::CommandStatus::committed,
            "Product box creation failed");
    const auto before = session.identifiedSnapshot();
    session.duplicateDocument();
    require(session.documentId() != id && session.identifiedSnapshot().snapshot() == before.snapshot(),
            "Duplicate lost namespace/content separation");
}
}
int main() {
    try {
        productBackend();
        sessionIdentity();
        std::cout << "PASS product OCAF attribute/query parity and transactional identity\n";
        return 0;
    } catch (const Standard_Failure& error) {
        std::cerr << kernel::occtFailureDiagnostic(error) << '\n';
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; }
    return 1;
}
