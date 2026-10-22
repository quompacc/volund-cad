// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "../../src/document/DocumentSerializationDetail.hpp"
#include "../../src/document/NativeDocumentStore.hpp"
#include "../../src/document/NativePrimitiveCodec.hpp"
#include <BinDrivers.hxx>
#include <PCDM_StoreStatus.hxx>
#include <TDataStd_AsciiString.hxx>
#include <TDataStd_Integer.hxx>
#include <TDocStd_Application.hxx>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <voelund/kernel/OcctCompatibility.hpp>

namespace {
using namespace voelund::document;
using namespace voelund::document::detail;
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class Action> void rejected(Action action) {
    bool failed = false;
    try {
        action();
    } catch (const std::invalid_argument&) {
        failed = true;
    }
    require(failed, "Invalid native payload accepted");
}
IdentifiedDocumentSnapshot fixture() {
    auto model = ModelDocument::empty();
    const auto box = model.addBoxFeature("Gehäuse Ω", {40, 30, 20});
    const auto other = model.addBoxFeature("Zweiter Körper", {10, 10, 10});
    const auto sketch =
        model.addSketchFeature("Profil", box,
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
    (void)model.addCylinderFeature("Separat", other,
                                   {2, 5, {60, 0, 0, {}, {}, {}}, CylinderResultMode::separate});
    (void)model.addExtrusionFeature("Extrusion", sketch, {3, {SketchProfileId{9}}});
    (void)model.addBoxFeature("Zurückgenommen", {2, 2, 2});
    require(model.undo(), "Fixture undo failed");
    return IdentifiedDocumentSnapshot::restore(DocumentId{UINT64_MAX, 0x123456789abcdefULL},
                                               DocumentSnapshot::capture(model));
}
std::string changedMetadata(const IdentifiedDocumentSnapshot& source, bool schema) {
    Handle(TDocStd_Application) app = new TDocStd_Application;
    BinDrivers::DefineFormat(app);
    Handle(TDocStd_Document) doc;
    app->NewDocument("BinOcaf", doc);
    doc->ChangeStorageFormatVersion(TDocStd_FormatVersion_VERSION_12);
    NativePrimitiveCodec::write(doc, source);
    if (schema)
        TDataStd_Integer::Set(doc->Main().FindChild(2), 999);
    else
        TDataStd_AsciiString::Set(doc->Main().FindChild(1), "Foreign.Application");
    std::ostringstream output(std::ios::binary);
    require(app->SaveAs(doc, output) == PCDM_SS_OK, "Invalid fixture save failed");
    app->Close(doc);
    return output.str();
}
void roundtrip(const IdentifiedDocumentSnapshot& source) {
    const auto bytes = NativeDocumentStore::encode(source);
    const auto restored = NativeDocumentStore::decode(bytes);
    require(restored == source, "Native store lost model fields or identity");
    require(NativeDocumentStore::decode(NativeDocumentStore::encode(restored)) == source,
            "Native rewrite changed snapshot");
    auto model = restored.snapshot().toDocument(restored.id());
    require(model.documentId() == source.id() && !model.canUndo() && !model.canRedo(),
            "Restore changed identity or persisted transient history");
    require(model.addBoxFeature("Weiter", {1, 1, 1}).value() == source.snapshot().nextFeatureId(),
            "Native store lost allocation watermark");
    require(restored == source, "Editing restored model changed snapshot");
}
void regressions() {
    const auto empty = IdentifiedDocumentSnapshot::restore(
        DocumentId{1, 2}, DocumentSnapshot::capture(ModelDocument::empty()));
    roundtrip(empty);
    const auto source = fixture();
    roundtrip(source);
    const auto bytes = NativeDocumentStore::encode(source);
    for (const auto count : {std::size_t{0}, std::size_t{1}, std::size_t{7}, bytes.size() / 2})
        rejected(
            [&] { (void)NativeDocumentStore::decode(std::string_view(bytes).substr(0, count)); });
    rejected([&] { (void)NativeDocumentStore::decode("VOLUND 16\n"); });
    rejected([&] { (void)NativeDocumentStore::decode(changedMetadata(source, true)); });
    rejected([&] { (void)NativeDocumentStore::decode(changedMetadata(source, false)); });
    const std::string oversized(maximumDocumentBytes + 1, '\0');
    rejected([&] { (void)NativeDocumentStore::decode(oversized); });
    // Failure cleanup must not poison the next application/session.
    require(NativeDocumentStore::decode(bytes) == source,
            "Valid open failed after rejected inputs");
}
} // namespace
int main() {
    try {
        regressions();
        std::cout << "PASS native store: snapshots, identity, history, watermark, malformed input, "
                     "schema, size\n";
        return 0;
    } catch (const Standard_Failure& error) {
        std::cerr << voelund::kernel::occtFailureDiagnostic(error) << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
