#include "../../src/document/NativeDocumentMetadata.hpp"
#include <BinDrivers.hxx>
#include <PCDM_ReaderStatus.hxx>
#include <PCDM_StoreStatus.hxx>
#include <TDataStd_AsciiString.hxx>
#include <TDataStd_Integer.hxx>
#include <TDocStd_Application.hxx>
#include <fstream>
#include <iostream>
#include <sstream>
#include <voelund/document/IdCapacity.hpp>
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
    require(failed, "Invalid native metadata accepted");
}
Handle(TDocStd_Document) fresh(const Handle(TDocStd_Application) & app) {
    Handle(TDocStd_Document) doc;
    app->NewDocument("BinOcaf", doc);
    doc->ChangeStorageFormatVersion(TDocStd_FormatVersion_VERSION_12);
    return doc;
}
std::string save(const Handle(TDocStd_Application) & app, const Handle(TDocStd_Document) & doc) {
    std::ostringstream bytes(std::ios::binary);
    require(app->SaveAs(doc, bytes) == PCDM_SS_OK && bytes.good(), "Native save failed");
    return bytes.str();
}
Handle(TDocStd_Document) open(const Handle(TDocStd_Application) & app, std::istream& bytes) {
    Handle(TDocStd_Document) doc;
    require(app->Open(bytes, doc) == PCDM_RS_OK && !doc.IsNull(), "Native open failed");
    doc->ChangeStorageFormatVersion(TDocStd_FormatVersion_VERSION_12);
    return doc;
}
void regressions(const Handle(TDocStd_Application) & app) {
    Handle(TDocStd_Document) missing;
    rejected([&] { (void)readNativeMetadata(missing); });
    auto doc = fresh(app);
    const NativeDocumentMetadata expected{generateDocumentId(), UINT64_MAX - 1};
    rejected([&] { (void)readNativeMetadata(doc); });
    require(!doc->Main().HasChild(), "Reading empty root created labels");
    rejected([&] { initializeNativeMetadata(doc, {expected.id, 0}); });
    rejected([&] { initializeNativeMetadata(doc, {expected.id, UINT64_MAX}); });
    require(!doc->Main().HasChild(), "Invalid initialization mutated root");
    initializeNativeMetadata(doc, expected);
    rejected([&] { initializeNativeMetadata(doc, {generateDocumentId(), 1}); });
    require(readNativeMetadata(doc) == expected, "Reinitialization changed identity");
    std::istringstream bytes(save(app, doc), std::ios::binary);
    auto restored = open(app, bytes);
    require(readNativeMetadata(restored) == expected && !hasIdCapacity(expected.nextFeatureId, 1),
            "Native storage lost ID bits or exhausted watermark");
    app->Close(restored);
    doc->SetUndoLimit(8);
    doc->NewCommand();
    TDataStd_AsciiString::Set(doc->Main().FindChild(4), "1");
    require(doc->CommitCommand() && readNativeMetadata(doc).id == expected.id && doc->Undo() &&
                readNativeMetadata(doc) == expected && doc->Redo() &&
                readNativeMetadata(doc).nextFeatureId == 1 && doc->Undo(),
            "Metadata history failed");
    for (const std::string value :
         {"0", "01", "+1", "-1", "1 ", "18446744073709551615", "18446744073709551616", ""}) {
        doc->NewCommand();
        TDataStd_AsciiString::Set(doc->Main().FindChild(4), value.c_str());
        rejected([&] { (void)readNativeMetadata(doc); });
        doc->AbortCommand();
        require(readNativeMetadata(doc) == expected, "Invalid watermark damaged original");
    }
    for (int slot : {1, 2, 3, 4}) {
        doc->NewCommand();
        doc->Main().FindChild(slot).ForgetAllAttributes();
        rejected([&] { (void)readNativeMetadata(doc); });
        doc->AbortCommand();
    }
    for (int version : {0, 12, -1}) {
        doc->NewCommand();
        TDataStd_Integer::Set(doc->Main().FindChild(2), version);
        rejected([&] { (void)readNativeMetadata(doc); });
        doc->AbortCommand();
    }
    doc->NewCommand();
    TDataStd_AsciiString::Set(doc->Main().FindChild(1), "Foreign.Native");
    rejected([&] { (void)readNativeMetadata(doc); });
    doc->AbortCommand();
    doc->NewCommand();
    TDataStd_AsciiString::Set(doc->Main().FindChild(3), "00000000000000000000000000000000");
    std::istringstream malformed(save(app, doc), std::ios::binary);
    restored = open(app, malformed);
    rejected([&] { (void)readNativeMetadata(restored); });
    app->Close(restored);
    doc->AbortCommand();
    require(readNativeMetadata(doc) == expected, "Negative tests changed metadata");
    app->Close(doc);
}
} // namespace
int main(int argc, char** argv) {
    try {
        Handle(TDocStd_Application) app = new TDocStd_Application;
        BinDrivers::DefineFormat(app);
        if (argc == 1) {
            regressions(app);
        } else {
            require(argc == 3 || argc == 4, "Usage: create FILE | read FILE [REWRITE]");
            const NativeDocumentMetadata expected{DocumentId{UINT64_MAX, UINT64_MAX},
                                                  UINT64_MAX - 1};
            Handle(TDocStd_Document) doc;
            const std::string mode = argv[1];
            if (mode == "create") {
                doc = fresh(app);
                initializeNativeMetadata(doc, expected);
            } else {
                require(mode == "read", "Unknown metadata test mode");
                std::ifstream input(argv[2], std::ios::binary);
                doc = open(app, input);
                require(readNativeMetadata(doc) == expected, "Cross-platform metadata changed");
            }
            if (mode == "create" || argc == 4) {
                std::ofstream output(argv[argc - 1], std::ios::binary);
                output << save(app, doc);
                output.close();
                require(!output.fail(), "Metadata fixture write failed");
            }
            app->Close(doc);
        }
        std::cout << "PASS native document metadata, application schema 11, container 12\n";
        return 0;
    } catch (const Standard_Failure& error) {
        std::cerr << voelund::kernel::occtFailureDiagnostic(error) << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
