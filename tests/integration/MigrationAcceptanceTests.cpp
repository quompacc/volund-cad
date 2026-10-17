#include "../TestPaths.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <voelund/application/CadSession.hpp>
#include <voelund/document/DocumentSerializer.hpp>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {
using namespace voelund;
namespace fs = std::filesystem;
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
std::string bytes(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    require(bool(input), "Missing fixture");
    return {std::istreambuf_iterator<char>(input), {}};
}
void put(const fs::path& path, const std::string& value) {
    std::ofstream output(path, std::ios::binary);
    output << value;
    output.close();
    require(!output.fail(), "Fixture write failed");
}
template <class Action> void fails(Action action) {
    bool failed = false;
    try {
        action();
    } catch (const std::exception& error) {
        failed = *error.what() != '\0';
    }
    require(failed, "Expected diagnostic failure missing");
}
void committed(const application::CommandResult& result) {
    require(result.status() != application::CommandStatus::rejected,
            "Migration edit/history rejected");
}
void versions(const fs::path& fixtures, const fs::path& work) {
    for (int version = 1; version <= 16; ++version) {
        std::ostringstream name;
        name << 'v' << std::setw(2) << std::setfill('0') << version << ".volund";
        const auto source = fixtures / name.str();
        const auto original = bytes(source);
        application::CadSession session;
        session.newDocument();
        session.loadDocument(source);
        require(session.lastImportReport() && session.lastImportReport()->source.version ==
                                                  static_cast<std::uint64_t>(version),
                "Source version lost");
        const auto imported = session.identifiedSnapshot();
        require(imported.snapshot().features().size() == 4 &&
                    imported.snapshot().nextFeatureId() == 5,
                "Legacy feature identities lost");
        require(session.selectFeature(document::FeatureId{1}) &&
                    session.selectedBoxDimensions().width == 20 &&
                    session.selectedBoxDimensions().height == 30,
                "Independent legacy parameter expectation failed");
        const auto native = work / fs::path(u8"Teil äöü.volund");
        session.saveDocument(native);
        require(bytes(native).starts_with("BINFILE"),
                "Migration did not use native product writer");
        session.loadDocument(native);
        require(session.identifiedSnapshot() == imported && !session.lastImportReport() &&
                    !session.isModified() && !session.canUndo() && !session.canRedo(),
                "Native migration changed snapshot/history");
        require(session.selectFeature(document::FeatureId{1}), "Missing migrated box");
        auto dimensions = session.selectedBoxDimensions();
        dimensions.width = 21;
        committed(session.editSelectedBoxDimensions(
            dimensions, application::ErrorDisposition::keepEditableErrors));
        const auto edited = session.identifiedSnapshot();
        require(session.isModified() && session.documentId() == imported.id() &&
                    session.selectedBoxDimensions().width == 21,
                "Edit lost identity or parameter");
        committed(session.undo());
        require(session.identifiedSnapshot() == imported && !session.isModified() &&
                    !session.canUndo(),
                "Undo did not restore complete migrated saved state");
        committed(session.redo());
        require(session.identifiedSnapshot() == edited && session.isModified(),
                "Redo changed migration content");
        const auto stamp = session.sceneStatus().document;
        const auto selection = session.selectedFeature();
        // A matching invalid case for every legacy version, through the live application.
        put(work / "bad.volund", original.substr(0, original.find("EXTRUSION")));
        fails([&] { session.loadDocument(work / "bad.volund"); });
        require(session.identifiedSnapshot() == edited && session.sceneStatus().document == stamp &&
                    session.selectedFeature() == selection && session.isModified() &&
                    session.canUndo() && session.currentFile() == fs::absolute(native),
                "Invalid legacy load changed active migrated document");
        session.saveDocument(native);
        session.loadDocument(native);
        require(session.identifiedSnapshot() == edited && !session.isModified(),
                "Edited migration did not persist");
        require(bytes(source) == original, "Migration changed legacy source");
        std::cout << "PASS VOLUND " << version
                  << " import/native/open/edit/undo/redo/invalid/source-preserved\n";
    }
}
void emptyAndBroken(const fs::path& fixtures, const fs::path& work) {
    application::CadSession session;
    session.newDocument();
    const auto empty = session.identifiedSnapshot();
    for (const auto& text : {std::string("VOLUND 17\nNEXT_ID 1\nFEATURES 0\n"),
                             std::string("VOLUND 16\nNEXT_ID 2\nFEATURES 1\nCYLINDER 1 \"Missing\" "
                                         "99 2 5 0 0 0 0 0 0 JOIN\n")}) {
        put(work / "bad.volund", text);
        fails([&] { session.loadDocument(work / "bad.volund"); });
        require(session.identifiedSnapshot() == empty && session.currentFile().empty() &&
                    !session.isModified(),
                "Failed migration changed empty document");
    }
    put(work / "empty.volund", "VOLUND 16\nNEXT_ID 1\nFEATURES 0\n");
    session.loadDocument(work / "empty.volund");
    require(session.features().empty() && session.lastImportReport()->referenceIssues.empty(),
            "Empty import fabricated content");
    session.saveDocument(work / "empty-native.volund");
    const auto migrated = session.identifiedSnapshot();
    session.loadDocument(work / "empty-native.volund");
    require(session.identifiedSnapshot() == migrated && !session.lastImportReport(),
            "Empty native migration failed");
    auto broken = bytes(fixtures / "v05.volund");
    const auto at = broken.find("EXTRUSION 4 \"Extrusion\" 3 5 1 1");
    require(at != std::string::npos, "Profile fixture mismatch");
    broken.replace(at, std::string("EXTRUSION 4 \"Extrusion\" 3 5 1 1").size(),
                   "EXTRUSION 4 \"Extrusion\" 3 5 1 999");
    put(work / "missing-profile.volund", broken);
    session.loadDocument(work / "missing-profile.volund");
    require(!session.sceneStatus().isCurrent() && session.lastImportReport() &&
                session.lastImportReport()->referenceIssues.back().profileIds ==
                    std::vector<document::SketchProfileId>{document::SketchProfileId{999}},
            "Legacy missing profile guessed or lost");
    const auto failed = session.identifiedSnapshot();
    session.saveDocument(work / "missing-native.volund");
    session.loadDocument(work / "missing-native.volund");
    require(session.identifiedSnapshot() == failed && !session.sceneStatus().isCurrent(),
            "Broken migration not retained");
}
void lockedMigration(const fs::path& fixtures, const fs::path& work) {
    const auto source = work / "locked-legacy.volund";
    const auto original = bytes(fixtures / "v01.volund");
    put(source, original);
    application::CadSession session;
    session.loadDocument(source);
    require(session.selectFeature(document::FeatureId{1}), "Missing legacy box");
    auto dimensions = session.selectedBoxDimensions();
    dimensions.width = 21;
    committed(session.editSelectedBoxDimensions(dimensions,
                                                application::ErrorDisposition::keepEditableErrors));
    const auto before = session.identifiedSnapshot();
    const auto stamp = session.sceneStatus().document;
    const auto report = session.lastImportReport();
    bool failed = false;
#ifdef _WIN32
    const HANDLE lock = CreateFileW(source.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    require(lock != INVALID_HANDLE_VALUE, "Could not lock legacy target");
    try {
        session.saveDocument(source);
    } catch (const std::exception&) {
        failed = true;
    }
    CloseHandle(lock);
#else
    require(geteuid() != 0 && ::chmod(work.c_str(), 0500) == 0,
            "Could not restrict migration directory");
    try {
        session.saveDocument(source);
    } catch (const std::exception&) {
        failed = true;
    }
    require(::chmod(work.c_str(), 0700) == 0, "Could not restore migration directory");
#endif
    require(failed && bytes(source) == original && session.identifiedSnapshot() == before &&
                session.sceneStatus().document == stamp && session.lastImportReport() == report &&
                session.currentFile() == fs::absolute(source) && session.isModified() &&
                session.canUndo(),
            "Locked migration changed legacy bytes or session");
    session.saveDocument(work / "unlocked-native.volund");
    session.loadDocument(work / "unlocked-native.volund");
    require(session.identifiedSnapshot() == before && bytes(source) == original,
            "Save-As after lock lost migration/source");
}
} // namespace
int main(int argc, char** argv) {
    const auto work =
        test::temporaryFilePath("d14-" + document::documentIdToHex(document::generateDocumentId()));
    try {
        require(argc == 2, "Fixture directory required");
        fs::create_directory(work);
        versions(argv[1], work);
        emptyAndBroken(argv[1], work);
        lockedMigration(argv[1], work);
        fs::remove_all(work);
        std::cout << "PASS empty/invalid/broken-reference/locked migrations\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        std::error_code ignored;
        fs::remove_all(work, ignored);
        return 1;
    }
}
