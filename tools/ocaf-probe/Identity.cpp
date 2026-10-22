// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "Fixture.hpp"
#include <PCDM_ReaderStatus.hxx>
#include <TDataStd_AsciiString.hxx>
#include <fstream>
#include <iostream>
#include <voelund/document/DocumentId.hpp>

namespace {
using namespace voelund::document;
constexpr DocumentId identity{0xffffffffffffffffULL, 0x0123456789abcdefULL};
DocumentId readIdentity(const Handle(TDocStd_Document) & doc) {
    Handle(TDataStd_AsciiString) value;
    probe::require(doc->Main().FindAttribute(TDataStd_AsciiString::GetID(), value),
                   "Missing document identity");
    const auto& text = value->Get();
    return documentIdFromHex(
        std::string_view(text.ToCString(), static_cast<std::size_t>(text.Length())));
}
void setIdentity(const Handle(TDocStd_Document) & doc, DocumentId id) {
    TDataStd_AsciiString::Set(doc->Main(), documentIdToHex(id).c_str());
}
void checkHistory(const Handle(TDocStd_Document) & doc) {
    probe::require(readIdentity(doc) == identity, "128-bit identity changed in native storage");
    doc->SetUndoLimit(5);
    doc->NewCommand();
    setIdentity(doc, DocumentId{0, 1});
    probe::require(doc->CommitCommand(), "Identity edit failed");
    probe::require(readIdentity(doc) == DocumentId{0, 1} && doc->Undo() &&
                       readIdentity(doc) == identity && doc->Redo() &&
                       readIdentity(doc) == DocumentId{0, 1} && doc->Undo() &&
                       readIdentity(doc) == identity,
                   "Native identity history failed");
}
Handle(TDocStd_Document) load(const Handle(TDocStd_Application) & app, const char* path) {
    std::ifstream input(path, std::ios::binary);
    Handle(TDocStd_Document) doc;
    probe::require(input.is_open() && app->Open(input, doc) == PCDM_RS_OK && !doc.IsNull(),
                   "Native identity read failed");
    doc->ChangeStorageFormatVersion(TDocStd_FormatVersion_VERSION_12);
    return doc;
}
void negative(const std::filesystem::path& directory) {
    for (const auto& text :
         {std::string(32, '0'), std::string(31, '1'), std::string(33, '1'), std::string(32, 'F'),
          std::string(32, 'g'), std::string(32, ' '), std::string(32, '\0')}) {
        bool rejected = false;
        try {
            (void)documentIdFromHex(text);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        probe::require(rejected, "Invalid identity field accepted");
    }
    probe::require(documentIdToHex(DocumentId{0, 1}) == "00000000000000000000000000000001" &&
                       documentIdToHex(identity) == "ffffffffffffffff0123456789abcdef" &&
                       documentIdFromHex("ffffffffffffffffffffffffffffffff") ==
                           DocumentId{0xffffffffffffffffULL, 0xffffffffffffffffULL},
                   "Identity field lost bits, leading zeroes or word order");
    auto app = probe::application();
    for (const char digit : {'0', 'g'}) {
        Handle(TDocStd_Document) doc;
        app->NewDocument("BinOcaf", doc);
        doc->ChangeStorageFormatVersion(TDocStd_FormatVersion_VERSION_12);
        TDataStd_AsciiString::Set(doc->Main(), std::string(32, digit).c_str());
        const auto path = directory / (std::string("invalid-identity-") + digit + ".volund");
        probe::save(app, doc, path);
        app->Close(doc);
        doc = load(app, path.string().c_str());
        bool rejected = false;
        try {
            (void)readIdentity(doc);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        app->Close(doc);
        probe::require(rejected, "Malformed identity in a valid native file accepted");
    }
}
} // namespace
int main(int argc, char** argv) {
    try {
        probe::require(argc >= 2, "Usage: create FILE | read FILE [REWRITE] | negative");
        const std::string mode = argv[1];
        if (mode == "negative") {
            probe::require(argc == 3, "Negative identity test requires its fixture directory");
            negative(argv[2]);
        } else {
            probe::require(argc >= 3, "Missing native identity file");
            auto app = probe::application();
            Handle(TDocStd_Document) doc;
            if (mode == "create") {
                app->NewDocument("BinOcaf", doc);
                doc->ChangeStorageFormatVersion(TDocStd_FormatVersion_VERSION_12);
                setIdentity(doc, identity);
                checkHistory(doc);
                probe::save(app, doc, argv[2]);
            } else {
                probe::require(mode == "read", "Unknown identity probe mode");
                doc = load(app, argv[2]);
                checkHistory(doc);
                if (argc == 4)
                    probe::save(app, doc, argv[3]);
            }
            app->Close(doc);
        }
        std::cout << "PASS native 128-bit document identity, format 12, history and field codec\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
