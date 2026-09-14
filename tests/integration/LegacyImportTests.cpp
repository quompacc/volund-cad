#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <voelund/application/CadSession.hpp>
#include <voelund/document/DocumentSerializer.hpp>

namespace {
using namespace voelund::document;
void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
std::string bytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    require(bool(input), "Fixture missing");
    return {std::istreambuf_iterator<char>(input), {}};
}
void versions(const std::filesystem::path& directory) {
    for (std::uint64_t version = 1; version <= 16; ++version) {
        std::ostringstream name;
        name << 'v' << std::setw(2) << std::setfill('0') << version << ".volund";
        const auto path = directory / name.str();
        const auto original = bytes(path);
        std::istringstream direct(original), product(original), repeat(original);
        const auto imported = LegacyDocumentImporter::read(direct);
        const auto dispatched = DocumentSerializer::readWithReport(product);
        const auto repeated = LegacyDocumentImporter::read(repeat);
        require(imported.snapshot == dispatched.snapshot && imported.snapshot == repeated.snapshot,
                "Import snapshot differs by entry point or repeat");
        require(imported.report == dispatched.report && imported.report == repeated.report,
                "Migration report is not deterministic");
        require(imported.report.source ==
                    DocumentSourceFormat{DocumentEncoding::legacyText, version},
                "Wrong source version");
        const auto& issues = imported.report.referenceIssues;
        require(issues.size() == 2, "Expected support-plane and profile migration issues");
        require(issues[0].feature == FeatureId{3} && issues[0].sourceFeature == FeatureId{1} &&
                    issues[0].kind == LegacyReferenceKind::supportPlaneOnly &&
                    issues[0].profileIds.empty(),
                "Support reference was guessed or lost");
        const auto& extrusion = std::get<ExtrusionParameters>(imported.snapshot.parameters()[3]);
        require(issues[1].feature == FeatureId{4} && issues[1].sourceFeature == FeatureId{3} &&
                    issues[1].profileIds == extrusion.profiles &&
                    issues[1].kind == (extrusion.profiles.empty()
                                           ? LegacyReferenceKind::implicitAllProfiles
                                           : LegacyReferenceKind::legacyProfileIds),
                "Profile migration lost source identity");
        auto document = imported.snapshot.toDocument();
        require(document.revision() == 0 && !document.canUndo() && !document.canRedo(),
                "Import leaked session history");
        require(document.addBoxFeature("After import", {1, 2, 3}) == FeatureId{5},
                "NEXT_ID was not preserved");
        require(imported.snapshot.features().size() == 4 && imported.snapshot.nextFeatureId() == 5,
                "Editing materialized document mutated snapshot");
        require(bytes(path) == original, "Import modified source fixture");
    }
}
void formats() {
    for (const auto* header :
         {"VOLUND 17", "VOLUND 0", "VOLUND 01", "VOLUND 1junk", "BINFILE", "volund 16"}) {
        std::istringstream input(std::string(header) + "\nnot a valid payload\n");
        bool rejected = false;
        try {
            static_cast<void>(DocumentSerializer::readWithReport(input));
        } catch (const std::runtime_error& error) {
            const std::string message = error.what();
            rejected = message.find("schema version") != std::string::npos ||
                       message.find("unknown document encoding") != std::string::npos;
        }
        require(rejected, "Unknown format reached legacy payload parser");
    }
    std::istringstream empty("VOLUND 16\nNEXT_ID 1\nFEATURES 0\n");
    const auto result = DocumentSerializer::readWithReport(empty);
    require(result.snapshot.features().empty() && result.report.referenceIssues.empty(),
            "Empty import fabricated references");
}
void session(const std::filesystem::path& fixtures, const std::filesystem::path& work) {
    voelund::application::CadSession session;
    require(!session.lastImportReport(), "New session has an import report");
    session.loadDocument(fixtures / "v01.volund");
    const auto report = session.lastImportReport();
    require(report && report->source.version == 1 && report->referenceIssues.size() == 2,
            "Product load dropped report");
    const auto file = session.currentFile();
    const auto stamp = session.sceneStatus().document;
    const auto bad = work / "d09-future.volund";
    {
        std::ofstream output(bad);
        output << "VOLUND 17\nNEXT_ID 1\nFEATURES 0\n";
    }
    bool rejected = false;
    try {
        session.loadDocument(bad);
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    require(rejected && session.lastImportReport() == report && session.currentFile() == file &&
                session.sceneStatus().document == stamp,
            "Failed load changed document or report");
    session.saveDocument(work / "d09-resaved.volund");
    require(session.lastImportReport() == report, "Save discarded unresolved import provenance");
    session.loadDocument(fixtures / "v16.volund");
    require(session.lastImportReport()->source.version == 16,
            "Successful load retained old report");
    session.newDocument();
    require(!session.lastImportReport(), "New document retained another document's report");
}
} // namespace
int main(int argc, char** argv) {
    try {
        require(argc == 3, "Pass fixture and writable test directories");
        versions(argv[1]);
        formats();
        session(argv[1], argv[2]);
        std::cout << "PASS 16 immutable imports, deterministic reports, strict dispatch, atomic "
                     "report lifecycle\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
