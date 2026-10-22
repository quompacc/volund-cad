// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "../CommandResultAssertions.hpp"
#include "../TestPaths.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <voelund/application/CadSession.hpp>
#include <voelund/document/DocumentSerializer.hpp>

namespace voelund::application {
class CadSessionPersistenceTestAccess {
  public:
    static void failSave(CadSession& session, const std::filesystem::path& path) {
        session.saveDocumentUsing(
            path, [](const auto&, auto) { throw std::runtime_error("Injected store failure"); });
    }
};
} // namespace voelund::application
namespace {
using namespace voelund;
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class Action> void fails(Action action) {
    try {
        action();
    } catch (const std::exception&) {
        return;
    }
    throw std::runtime_error("Expected failure missing");
}
std::string readBytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    require(bool(input), "Missing saved file");
    return {std::istreambuf_iterator<char>(input), {}};
}
void put(const std::filesystem::path& path, const std::string& bytes) {
    std::ofstream output(path, std::ios::binary);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    require(bool(output), "Fixture write failed");
}
void lifecycle(const std::filesystem::path& root) {
    application::CadSession session;
    const auto path = root / "native.volund";
    session.saveDocument(path);
    const auto saved = session.identifiedSnapshot();
    const auto bytes = readBytes(path);
    require(bytes.starts_with("BINFILE"), "Product still writes legacy format");
    std::istringstream input(bytes);
    const auto loaded = document::DocumentSerializer::readWithReport(input);
    require(!loaded.report && loaded.id == saved.id() && loaded.snapshot == saved.snapshot(),
            "Native dispatch lost identity/content or fabricated migration report");
    (void)test::requireCommittedFeature(session.createBoxFeature());
    require(session.isModified(), "Edit did not dirty session");
    const auto edited = session.identifiedSnapshot();
    const auto stamp = session.sceneStatus().document;
    const auto selected = session.selectedFeature();
    const auto shape = session.currentShape();
    fails([&] { application::CadSessionPersistenceTestAccess::failSave(session, path); });
    require(readBytes(path) == bytes && session.identifiedSnapshot() == edited &&
                session.sceneStatus().document == stamp && session.isModified() &&
                session.canUndo(),
            "Failed save changed destination, document or dirty/history state");
    for (const auto& malformed : {bytes.substr(0, bytes.size() - 1), bytes + "trailing",
                                  std::string("BINFILE\nVOLUND 16\n")}) {
        put(root / "bad.volund", malformed);
        fails([&] { session.loadDocument(root / "bad.volund"); });
        require(session.identifiedSnapshot() == edited && session.sceneStatus().document == stamp &&
                    session.selectedFeature() == selected && session.currentShape().IsSame(shape) &&
                    session.currentFile() == std::filesystem::absolute(path) &&
                    session.isModified() && session.canUndo(),
                "Malformed native open changed live session");
    }
    session.loadDocument(path);
    require(session.identifiedSnapshot() == saved && !session.lastImportReport() &&
                !session.isModified() && !session.canUndo() && !session.canRedo(),
            "Native open did not restore clean state");
    session.saveDocument(root / "save-as.volund");
    application::CadSession another;
    another.loadDocument(root / "save-as.volund");
    require(another.identifiedSnapshot() == saved, "Native Save-As changed identity");
    put(root / "legacy.volund",
        "VOLUND 16\nNEXT_ID 2\nFEATURES 1\nBOX 1 \"Legacy\" 1 2 3 0 0 0 0 0 0\n");
    session.loadDocument(root / "legacy.volund");
    const auto first = session.identifiedSnapshot();
    require(session.lastImportReport() && session.lastImportReport()->source.version == 16,
            "Legacy open lost provenance");
    session.loadDocument(root / "legacy.volund");
    require(session.documentId() != first.id() &&
                session.identifiedSnapshot().snapshot() == first.snapshot(),
            "Repeated legacy open did not create a fresh namespace");
    const auto migrated = session.identifiedSnapshot();
    session.saveDocument(path);
    session.loadDocument(path);
    require(session.identifiedSnapshot() == migrated && !session.lastImportReport(),
            "Legacy-to-native save/open changed identity or fabricated migration report");
}
class FailedOutput final : public std::streambuf {
    std::streamsize xsputn(const char*, std::streamsize) override { return 0; }
};
void outputFailure() {
    auto document = document::ModelDocument::empty();
    const auto id = document.documentId();
    FailedOutput buffer;
    std::ostream output(&buffer);
    fails([&] { document::DocumentSerializer::write(document, output); });
    require(document.documentId() == id && document.features().empty(),
            "Failed output changed source");
}
} // namespace
int main() {
    const auto root = test::temporaryFilePath(
        "native-product-" + document::documentIdToHex(document::generateDocumentId()));
    try {
        std::filesystem::create_directory(root);
        lifecycle(root);
        outputFailure();
        std::filesystem::remove_all(root);
        std::cout << "PASS native product identity, legacy migration, Save-As, failed "
                     "input/output/store\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
        return 1;
    }
}
