#include "../TestPaths.hpp"

#include <voelund/application/CadSession.hpp>

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

// M01 (user decision 2026-10-10): body names and visibility are stored with the document.
// Renaming is one undo step; hiding marks the document modified without an undo step, keeps
// the redo history, and undo/redo keep the current visibility. Save/Open keeps both; a name
// for a body the document does not have is refused.
namespace {
using namespace voelund;
using application::CommandStatus;
using document::BodyId;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
const application::OutlineBody& body(const application::CadSession& session, BodyId id) {
    static application::ModelOutline outline;
    outline = session.modelOutline();
    for (const auto& item : outline.bodies)
        if (item.id == id)
            return item;
    throw std::runtime_error("Body missing");
}

void run() {
    application::CadSession session;
    session.newDocument();
    require(session.createBoxFeature({10, 10, 10}).status() == CommandStatus::committed, "A");
    const BodyId a{session.features().back().id.value()};
    require(session.createBoxFeature({10, 10, 10}).status() == CommandStatus::committed, "B");
    const BodyId b{session.features().back().id.value()};

    // Rename: one undo step, also back and forth.
    require(session.renameBody(a, "Grundplatte").status() == CommandStatus::committed &&
                body(session, a).name == "Grundplatte" && body(session, b).name.empty(),
            "A renamed, B untouched");
    require(session.renameBody(a, "Grundplatte").status() != CommandStatus::committed,
            "The same name is no change");
    require(session.undo().status() == CommandStatus::committed && body(session, a).name.empty(),
            "Undo restores the numbered default");
    require(session.redo().status() == CommandStatus::committed &&
                body(session, a).name == "Grundplatte",
            "Redo renames again");
    require(session.renameBody(BodyId{999}, "Fremd").status() != CommandStatus::committed,
            "A body the document does not have cannot be named");
    require(session.renameBody(a, std::string(300, 'x')).status() != CommandStatus::committed,
            "Names longer than 256 bytes are refused");

    // Hide: stored and modified, no undo step; redo history and undo keep it.
    const auto path = test::temporaryFilePath("voelund-m01-bodies.volund");
    session.saveDocument(path);
    require(!session.isModified(), "Saved");
    require(session.undo().status() == CommandStatus::committed && session.canRedo(),
            "An undone rename waits for redo");
    require(session.setBodyHidden(b, true) && body(session, b).hidden && session.isModified() &&
                session.canRedo() && session.sceneStatus().isCurrent(),
            "Hiding B: modified, no new step, redo kept, geometry current");
    require(session.redo().status() == CommandStatus::committed && body(session, b).hidden &&
                body(session, a).name == "Grundplatte",
            "Redo keeps B hidden");
    require(session.undo().status() == CommandStatus::committed && body(session, b).hidden,
            "Undo keeps B hidden");
    require(session.redo().status() == CommandStatus::committed, "Redo again");
    require(session.setBodyHidden(b, false) && !body(session, b).hidden &&
                !session.setBodyHidden(b, false),
            "Showing B again; showing a shown body is no change");
    require(!session.isModified(), "Back to the saved state: not modified");

    require(session.setBodyHidden(b, true), "Hide B");
    session.saveDocument(path);
    application::CadSession loaded;
    loaded.loadDocument(path);
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    require(body(loaded, a).name == "Grundplatte" && body(loaded, b).hidden &&
                !body(loaded, a).hidden && loaded.validBodies().size() == 2 && !loaded.isModified(),
            "Save/Open keeps the name, the visibility and both bodies");
}
} // namespace

int main() {
    try {
        run();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "PASS M01 body names and visibility\n";
    return 0;
}
