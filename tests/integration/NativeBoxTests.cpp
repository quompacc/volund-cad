#include "../../src/document/NativeBoxCodec.hpp"
#include <voelund/document/LegacyDocumentImporter.hpp>
#include <voelund/features/FeatureGraph.hpp>
#include <BinDrivers.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <TDataStd_AsciiString.hxx>
#include <TDataStd_Integer.hxx>
#include <TDataStd_RealArray.hxx>
#include <TDocStd_Application.hxx>
#include <PCDM_ReaderStatus.hxx>
#include <PCDM_StoreStatus.hxx>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>

namespace {
using namespace voelund;
using namespace document;
using detail::NativeBoxCodec;
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template<class Action> void rejected(Action action) {
    bool failed = false;
    try { action(); } catch (const std::invalid_argument&) { failed = true; }
    require(failed, "Invalid or unsupported box data accepted");
}
IdentifiedDocumentSnapshot fixture() {
    std::istringstream legacy("VOLUND 16\nNEXT_ID 18446744073709551614\nFEATURES 2\n"
        "BOX 18446744073709551613 \"Körper Ω \\\"A\\\"\" 2 3 4 1 -2 3 90 -45 360\n"
        "BOX 7 \"Körper Ω \\\"A\\\"\" 5 6 7 -8 9 10 0 0 0\n");
    return IdentifiedDocumentSnapshot::restore(DocumentId{UINT64_MAX, UINT64_MAX},
                                               LegacyDocumentImporter::read(legacy).snapshot);
}
Handle(TDocStd_Document) fresh(const Handle(TDocStd_Application)& app) {
    Handle(TDocStd_Document) doc;
    app->NewDocument("BinOcaf", doc);
    doc->ChangeStorageFormatVersion(TDocStd_FormatVersion_VERSION_12);
    return doc;
}
void save(const Handle(TDocStd_Application)& app, const Handle(TDocStd_Document)& doc,
          std::ostream& output) {
    require(app->SaveAs(doc, output) == PCDM_SS_OK && output.good(), "Native save failed");
}
Handle(TDocStd_Document) open(const Handle(TDocStd_Application)& app, std::istream& input) {
    Handle(TDocStd_Document) doc;
    require(app->Open(input, doc) == PCDM_RS_OK && !doc.IsNull(), "Native open failed");
    doc->ChangeStorageFormatVersion(TDocStd_FormatVersion_VERSION_12);
    return doc;
}
void check(const Handle(TDocStd_Document)& doc, const IdentifiedDocumentSnapshot& expected) {
    const auto actual = NativeBoxCodec::read(doc);
    require(actual == expected, "Native box snapshot lost identity, order, names or parameters");
    const auto model = actual.snapshot().toDocument();
    require(!model.canUndo() && !model.canRedo(), "Transient history leaked into native snapshot");
    if (expected.snapshot().features().empty()) return;
    const auto evaluated = features::FeatureGraph{}.evaluate(model);
    require(evaluated.bodies.size() == expected.snapshot().features().size(), "Body count changed");
    for (std::size_t index = 0; index < evaluated.bodies.size(); ++index) {
        const auto& body = evaluated.bodies[index];
        require(body.bodyId.value() == expected.snapshot().features()[index].id.value() &&
                    body.sourceFeature == expected.snapshot().features()[index].id &&
                    BRepCheck_Analyzer(body.shape).IsValid(), "Body identity or rebuilt BRep changed");
    }
}
void regressions(const Handle(TDocStd_Application)& app) {
    const auto expected = fixture();
    auto doc = fresh(app);
    NativeBoxCodec::write(doc, expected);
    check(doc, expected);
    std::stringstream bytes(std::ios::in | std::ios::out | std::ios::binary);
    save(app, doc, bytes);
    auto loaded = open(app, bytes);
    check(loaded, expected);
    app->Close(loaded);
    rejected([&] { NativeBoxCodec::write(doc, expected.duplicate()); });
    check(doc, expected);
    const auto bodies = doc->Main().FindChild(10);
    const auto first = bodies.FindChild(1).FindChild(10);
    doc->SetUndoLimit(10);
    doc->NewCommand();
    Handle(TDataStd_RealArray) dimensions;
    first.FindChild(4).FindAttribute(TDataStd_RealArray::GetID(), dimensions);
    dimensions->SetValue(1, 3);
    require(doc->CommitCommand(), "Native box edit did not commit");
    const auto edited = NativeBoxCodec::read(doc);
    require(edited.id() == expected.id() && edited.snapshot() != expected.snapshot() &&
                doc->Undo(), "Native box edit changed identity or failed undo");
    check(doc, expected);
    require(doc->Redo() && NativeBoxCodec::read(doc) == edited && doc->Undo(),
            "Native box redo failed");
    const auto corrupt = [&](auto mutate) {
        doc->NewCommand();
        mutate();
        rejected([&] { (void)NativeBoxCodec::read(doc); });
        doc->AbortCommand();
        check(doc, expected);
    };
    corrupt([&] { TDataStd_AsciiString::Set(first.FindChild(1), "cylinder"); });
    corrupt([&] { TDataStd_AsciiString::Set(first.FindChild(2), "7"); });
    corrupt([&] {
        TDataStd_AsciiString::Set(first.FindChild(2), "7");
        TDataStd_AsciiString::Set(bodies.FindChild(1).FindChild(1), "7");
    });
    corrupt([&] { TDataStd_AsciiString::Set(bodies.FindChild(1).FindChild(1), "8"); });
    corrupt([&] { TDataStd_Integer::Set(bodies, 3); });
    corrupt([&] { TDataStd_Integer::Set(bodies, 10001); });
    corrupt([&] { TDataStd_AsciiString::Set(doc->Main().FindChild(4), "8"); });
    corrupt([&] { first.FindChild(3).ForgetAllAttributes(); });
    corrupt([&] { TDataStd_RealArray::Set(first.FindChild(4), 0, 8); });
    corrupt([&] {
        Handle(TDataStd_RealArray) data;
        first.FindChild(4).FindAttribute(TDataStd_RealArray::GetID(), data);
        data->SetValue(1, -1);
    });
    corrupt([&] {
        Handle(TDataStd_RealArray) data;
        first.FindChild(4).FindAttribute(TDataStd_RealArray::GetID(), data);
        data->SetValue(7, std::numeric_limits<double>::quiet_NaN());
    });
    app->Close(doc);
    auto empty = fresh(app);
    const auto emptySnapshot = IdentifiedDocumentSnapshot::create(
        DocumentSnapshot::capture(ModelDocument::empty()));
    NativeBoxCodec::write(empty, emptySnapshot);
    check(empty, emptySnapshot);
    app->Close(empty);
    auto unsupported = fresh(app);
    const auto mixed = IdentifiedDocumentSnapshot::create(DocumentSnapshot::capture(ModelDocument{}));
    rejected([&] { NativeBoxCodec::write(unsupported, mixed); });
    require(!unsupported->Main().HasChild(), "Rejected family partially wrote native header");
    app->Close(unsupported);
}
} // namespace
int main(int argc, char** argv) {
    try {
        Handle(TDocStd_Application) app = new TDocStd_Application;
        BinDrivers::DefineFormat(app);
        if (argc == 1) regressions(app);
        else {
            require(argc == 3 || argc == 4, "Usage: create FILE | read FILE [REWRITE]");
            Handle(TDocStd_Document) doc;
            const std::string mode = argv[1];
            if (mode == "create") {
                doc = fresh(app);
                NativeBoxCodec::write(doc, fixture());
            } else {
                require(mode == "read", "Unknown mode");
                std::ifstream input(argv[2], std::ios::binary);
                doc = open(app, input);
            }
            check(doc, fixture());
            if (mode == "create" || argc == 4) {
                std::ofstream output(argv[argc - 1], std::ios::binary);
                save(app, doc, output);
                output.close();
                require(!output.fail(), "Fixture flush failed");
            }
            app->Close(doc);
        }
        std::cout << "PASS native boxes: identities, names, order, radians, parameters, BRep\n";
        return 0;
    } catch (const Standard_Failure& error) {
        std::cerr << kernel::occtFailureDiagnostic(error) << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
