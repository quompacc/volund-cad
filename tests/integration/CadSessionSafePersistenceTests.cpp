#include "../../src/platform/AtomicFileStoreDetail.hpp"
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <voelund/application/CadSession.hpp>
#include <voelund/platform/AtomicFileStore.hpp>

namespace voelund::application {
class CadSessionPersistenceTestAccess {
  public:
    static void save(CadSession& session, const std::filesystem::path& path,
                     platform::FileStoreStage failure) {
        session.saveDocumentUsing(path, [failure](const auto& target, auto bytes) {
            platform::detail::writeAtomic(target, bytes, [failure](auto stage) {
                if (stage == failure)
                    throw platform::FileStoreError(stage,
                                                   std::make_error_code(std::errc::io_error));
            });
        });
    }
};
} // namespace voelund::application

namespace {
using namespace voelund;
namespace fs = std::filesystem;
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
struct Fixture {
    fs::path root;
    explicit Fixture(const fs::path& parent) {
        root = parent / ("d03-" + std::to_string(std::random_device{}()));
        require(fs::create_directory(root), "exclusive fixture creation");
    }
    ~Fixture() {
        std::error_code error;
        fs::remove_all(root, error);
    }
};
std::string bytes(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
template <class Action> void fails(Action action) {
    bool failed = false;
    try {
        action();
    } catch (const std::exception&) {
        failed = true;
    }
    require(failed, "operation unexpectedly succeeded");
}
void saveFailures(const fs::path& root) {
    application::CadSession session;
    const auto original = root / "original.volund";
    const auto target = root / "save-as.volund";
    session.saveDocument(original);
    platform::AtomicFileStore::write(target, "previous target bytes");
    auto dimensions = session.selectedBoxDimensions();
    dimensions.width += 1;
    static_cast<void>(session.setSelectedBoxDimensions(dimensions));
    const auto shape = session.currentShape();
    const auto selected = session.selectedFeature();
    const auto originalBytes = bytes(original);
    for (const auto stage : {platform::FileStoreStage::create, platform::FileStoreStage::write,
                             platform::FileStoreStage::flush, platform::FileStoreStage::close,
                             platform::FileStoreStage::replace}) {
        bool caught = false;
        try {
            application::CadSessionPersistenceTestAccess::save(session, target, stage);
        } catch (const platform::FileStoreError& error) {
            caught = error.stage() == stage;
        }
        require(caught, "missing phase error");
        require(bytes(target) == "previous target bytes" && bytes(original) == originalBytes,
                "failed save changed bytes");
        require(session.currentFile() == fs::absolute(original) && session.isModified() &&
                    session.canUndo() && !session.canRedo() &&
                    session.selectedFeature() == selected && session.currentShape().IsSame(shape),
                "failed save changed session");
    }
    fails([&] { session.saveDocument(root); });
    require(session.isModified() && session.currentFile() == fs::absolute(original),
            "real filesystem failure changed state");
    session.saveDocument(target);
    require(!session.isModified() && session.currentFile() == fs::absolute(target),
            "save commit state");
    require(session.undo() && session.isModified() && session.redo() && !session.isModified(),
            "saved state does not follow history");
    for (const auto& entry : fs::directory_iterator(root))
        require(entry.path().filename().string().find(".voelund-save-") != 0,
                "temporary file leaked");
}
void openFailuresAndUnicode(const fs::path& root) {
    const auto folder = root / fs::path(u8"Prüfung mit Leerzeichen");
    fs::create_directory(folder);
    const auto path = folder / fs::path(u8"Teil äöü.volund");
    application::CadSession session;
    session.saveDocument(path);
    const auto validBytes = bytes(path);
    static_cast<void>(session.addBoxFeature());
    const auto selected = session.selectedFeature();
    const auto shape = session.currentShape();
    const auto count = session.features().size();
    const auto bad = root / "bad.volund";
    for (const auto& corrupt :
         {std::string("VOLUND 999\n"), validBytes.substr(0, validBytes.size() / 2),
          std::string(65537, 'x')}) {
        platform::AtomicFileStore::write(bad, corrupt);
        fails([&] { session.loadDocument(bad); });
        require(session.currentFile() == fs::absolute(path) && session.isModified() &&
                    session.canUndo() && !session.canRedo() &&
                    session.selectedFeature() == selected && session.features().size() == count &&
                    session.currentShape().IsSame(shape),
                "failed open changed active document");
    }
    session.loadDocument(path);
    require(!session.isModified() && !session.canUndo() && !session.canRedo() &&
                session.features().size() == 2 && session.currentFile() == fs::absolute(path),
            "Unicode open failed");
    session.newDocument();
    require(session.currentFile().empty() && !session.isModified(), "new document retained file");
}
void brokenGeometryRoundtrip(const fs::path& root) {
    application::CadSession session;
    require(session.selectFeature(document::ModelDocument::initialCylinderId()), "select cylinder");
    auto parameters = session.selectedCylinderParameters();
    parameters.placement.translationZ = 10;
    static_cast<void>(session.setSelectedCylinderParameters(parameters));
    require(session.featureStates().back().status == application::FeatureRuntimeStatus::error,
            "expected disconnected join error");
    const auto path = root / "editable-error.volund";
    session.saveDocument(path);
    application::CadSession loaded;
    loaded.loadDocument(path);
    require(!loaded.isModified() && loaded.currentShape().IsNull() &&
                loaded.featureStates().back().status == application::FeatureRuntimeStatus::error &&
                loaded.selectedCylinderParameters().placement.translationZ == 10,
            "error document not restored safely");
    parameters.placement.translationZ = 0;
    static_cast<void>(loaded.setSelectedCylinderParameters(parameters));
    require(!loaded.currentShape().IsNull() && loaded.isModified() &&
                loaded.featureStates().back().status == application::FeatureRuntimeStatus::current,
            "loaded error not editable");
    loaded.saveDocument(path);
    session.loadDocument(path);
    require(!session.currentShape().IsNull() && !session.isModified(), "repaired file failed");
}
} // namespace
int main(int argc, char** argv) {
    try {
        require(argc == 2, "fixture parent required");
        Fixture fixture(fs::absolute(argv[1]));
        saveFailures(fixture.root);
        openFailuresAndUnicode(fixture.root);
        brokenGeometryRoundtrip(fixture.root);
        std::cout << "Safe save/open, all commit phases, Unicode and editable errors passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
