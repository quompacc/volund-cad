// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "../CommandResultAssertions.hpp"
#include "CadSessionTestSupport.hpp"
#include <voelund/document/DocumentSerializer.hpp>
#include <voelund/document/DocumentTransaction.hpp>

#include <fstream>
#include <limits>
#include <random>
#include <sstream>

namespace {
using namespace voelund;
using namespace document;

void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
void committed(const application::CommandResult& result) {
    require(result.status() == application::CommandStatus::committed, "Expected command commit");
}
std::string bytes(const ModelDocument& document) {
    std::ostringstream output;
    DocumentSerializer::write(document, output);
    return output.str();
}
void historyAndSnapshot() {
    auto document = ModelDocument::empty();
    const auto empty = DocumentSnapshot::capture(document);
    const auto first = document.addBoxFeature("First", {1, 2, 3});
    const auto second = document.addBoxFeature("Second", {2, 3, 4});
    require(document.undo() && document.undo(), "Undo chain");
    require(empty.matchesContent(document) && DocumentSnapshot::capture(document) != empty,
            "Content comparison confused allocation state with model content");
    require(document.redo() && document.features().back().id == first && document.redo() &&
                document.features().back().id == second,
            "Redo changed existing identities");
    require(document.undo() && document.undo(), "Undo before branch");
    const auto captured = DocumentSnapshot::capture(document);
    auto restored = captured.toDocument();
    require(captured == DocumentSnapshot::capture(restored), "Snapshot lost allocation state");
    const auto third = restored.addBoxFeature("Branch", {3, 4, 5});
    require(third.value() > second.value() && !restored.contains(first) &&
                !restored.contains(second),
            "Snapshot branch reused published ID");
    const auto branch = document.addBoxFeature("Branch", {3, 4, 5});
    require(branch == third && !document.canRedo(), "Live branch counter differs from snapshot");
    require(document.undo(), "Undo branch");
    std::istringstream input(bytes(document));
    auto reopened = DocumentSerializer::read(input);
    require(reopened.addBoxFeature("After reopen", {4, 5, 6}).value() > branch.value(),
            "Save/open reused discarded branch ID");
}
void candidateIsolation() {
    auto document = ModelDocument::empty();
    const auto first = document.addBoxFeature("First", {1, 2, 3});
    require(document.undo(), "Undo first");
    const auto before = DocumentSnapshot::capture(document);
    const auto revision = document.revision();
    FeatureId provisional{0};
    {
        DocumentTransaction transaction(document);
        provisional = transaction.edit().addBoxFeature("Cancelled", {1, 2, 3});
        require(provisional.value() > first.value() && transaction.prepare(), "Prepare candidate");
        transaction.rollback();
    }
    require(DocumentSnapshot::capture(document) == before && document.revision() == revision &&
                document.canRedo(),
            "Rollback consumed live IDs or history");
    DocumentTransaction winner(document);
    DocumentTransaction stale(document);
    const auto winnerId = winner.edit().addBoxFeature("Winner", {2, 3, 4});
    require(stale.edit().addBoxFeature("Stale", {3, 4, 5}) == winnerId && winnerId == provisional,
            "Unpublished candidate IDs were globally reserved");
    require(stale.prepare() && winner.commit(), "Commit winner");
    const auto committed = DocumentSnapshot::capture(document);
    bool rejected = false;
    try {
        (void)stale.commit();
    } catch (const std::logic_error&) {
        rejected = true;
    }
    require(rejected && committed == DocumentSnapshot::capture(document),
            "Stale commit changed ID allocation");
    require(document.undo() &&
                document.addBoxFeature("After winner", {3, 4, 5}).value() > winnerId.value(),
            "Committed candidate ID was reused");
}
void exhaustedAfterUndo() {
    const auto limit = std::numeric_limits<std::uint64_t>::max();
    std::istringstream input("VOLUND 16\nNEXT_ID " + std::to_string(limit - 2) + "\nFEATURES 0\n");
    auto document = DocumentSerializer::read(input);
    const auto finalId = document.addBoxFeature("Last", {1, 2, 3});
    require(finalId.value() == limit - 2 && document.undo(), "Final valid ID setup");
    const auto before = bytes(document);
    const auto revision = document.revision();
    bool rejected = false;
    try {
        (void)document.addBoxFeature("Overflow", {1, 2, 3});
    } catch (const std::overflow_error&) {
        rejected = true;
    }
    require(rejected && bytes(document) == before && document.revision() == revision &&
                document.canRedo(),
            "Undo reopened exhausted ID space or rejection mutated state");
    require(document.redo() && document.features().back().id == finalId,
            "Exhaustion prevented restoration of an existing ID");
}
struct Files {
    std::filesystem::path first =
        test::temporaryFilePath("d11b-" + std::to_string(std::random_device{}()) + ".volund");
    std::filesystem::path second = first.string() + ".copy.volund";
    ~Files() {
        std::error_code error;
        std::filesystem::remove(first, error);
        std::filesystem::remove(second, error);
    }
};
void sessionSaveAndSelection() {
    Files files;
    application::CadSession session;
    session.newDocument();
    session.saveDocument(files.first);
    const auto oldId = test::requireCommittedFeature(session.createBoxFeature());
    require(session.isModified(), "Creation remained clean");
    committed(session.undo());
    require(!session.isModified() && !session.selectFeature(oldId), "Undo content/selection state");
    // Explicit Save-As persists the watermark even when the content is clean.
    session.saveDocument(files.second);
    require(!session.isModified() && session.canRedo(), "Save-As changed content/history");
    session.loadDocument(files.second);
    const auto nextId = test::requireCommittedFeature(session.createBoxFeature());
    require(nextId.value() > oldId.value() && !session.selectFeature(oldId) &&
                session.selectedFeature() == nextId,
            "Old selection aliased a replacement feature");
    session.saveDocument(files.second);
    require(session.selectFeature(nextId), "Select saved box");
    committed(session.editSelectedBoxDimensions({2, 3, 4}));
    require(session.isModified(), "Parameter edit remained clean");
    committed(session.undo());
    require(!session.isModified(), "Parameter undo did not restore saved content");
}
} // namespace
int main() {
    try {
        historyAndSnapshot();
        candidateIsolation();
        exhaustedAfterUndo();
        sessionSaveAndSelection();
        std::cout << "Feature ID lifetime tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
