// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "AllocationFailure.hpp"
#ifdef _MSC_VER
#include <crtdbg.h>
#include <cstdlib>
#endif
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <voelund/document/DocumentSerializer.hpp>
#include <voelund/document/DocumentTransaction.hpp>

namespace {
using namespace voelund;
using namespace document;
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
std::string snapshot(const ModelDocument& document) {
    std::ostringstream stream;
    DocumentSerializer::write(document, stream);
    return stream.str();
}
std::string fingerprint(const ModelDocument& document) {
    auto copy = document;
    std::string result = std::to_string(document.revision()) + snapshot(copy);
    while (copy.undo())
        result += "undo:" + snapshot(copy);
    copy = document;
    while (copy.redo())
        result += "redo:" + snapshot(copy);
    return result;
}
template <class Action> void mustThrow(Action action) {
    bool caught = false;
    try {
        action();
    } catch (const std::exception&) {
        caught = true;
    }
    require(caught, "expected exception");
}
SketchParameters sketch() {
    return {{0, 0, 20, 0, 0, 1, 1, 0, 0},
            SketchElements{{{SketchElementId{1}, SketchRectangle{1, 1, 5, 5}}}, 2}};
}
void compoundAndHistory() {
    auto document = ModelDocument::empty();
    require(document.addBoxFeature("Base", {40, 30, 20}) == ModelDocument::initialBoxId(),
            "base ID");
    std::istringstream savedBase(snapshot(document));
    document =
        DocumentSerializer::read(savedBase); // Loaded one-body start, no preexisting history.
    const auto original = snapshot(document);
    const auto originalContent = DocumentSnapshot::capture(document);
    const auto revision = document.revision();
    DocumentTransaction transaction(document);
    auto& candidate = transaction.edit();
    const auto sketchId =
        candidate.addSketchFeature("Sketch", ModelDocument::initialBoxId(), sketch());
    const auto extrusionId = candidate.addExtrusionFeature("Extrude", sketchId, {10, {}});
    require(!candidate.canUndo() && snapshot(document) == original,
            "candidate history/publication leaked");
    const auto expected = snapshot(candidate);
    require(transaction.prepare() && snapshot(document) == original, "prepare published early");
    require(transaction.commit() && snapshot(document) == expected &&
                document.revision() == revision + 1,
            "compound commit incorrect");
    mustThrow([&] { static_cast<void>(transaction.commit()); });
    require(document.undo() && originalContent.matchesContent(document) && !document.canUndo(),
            "compound requires two undos");
    require(document.redo() && snapshot(document) == expected && !document.canRedo() &&
                document.contains(sketchId) && document.contains(extrusionId),
            "redo lost IDs/parameters");
    require(document.revision() == revision + 3,
            "publication revision did not advance on history traversal");
    require(document.undo(), "prepare redo branch");
    const auto before = fingerprint(document);
    {
        DocumentTransaction cancelled(document);
        require(cancelled.edit().addBoxFeature("Cancelled", {1, 2, 3}) ==
                    FeatureId{extrusionId.value() + 1},
                "candidate reused published ID");
        cancelled.rollback();
        mustThrow([&] { static_cast<void>(cancelled.commit()); });
    }
    require(fingerprint(document) == before, "cancel changed redo/IDs/revision");
    bool partialFailure = false;
    try {
        DocumentTransaction failed(document);
        static_cast<void>(failed.edit().addBoxFeature("Temporary", {2, 3, 4}));
        static_cast<void>(
            failed.edit().addExtrusionFeature("Invalid", ModelDocument::initialBoxId(), {10, {}}));
    } catch (const std::invalid_argument&) {
        partialFailure = true;
    }
    require(partialFailure && fingerprint(document) == before,
            "exception after first partial edit leaked");
    DocumentTransaction branch(document);
    require(branch.edit().addBoxFeature("Replacement", {3, 4, 5}) ==
                FeatureId{extrusionId.value() + 1},
            "aborted IDs not restored");
    require(branch.commit() && !document.canRedo(), "branch did not discard redo");
}
void independentCopiesAndMoves() {
    ModelDocument original;
    const auto id = original.addBoxFeature("History", {2, 3, 4});
    require(original.undo(), "prepare copy with redo history");
    const auto before = fingerprint(original);
    auto copy = original;
    require(fingerprint(copy) == before, "copy lost history or revision");
    require(copy.redo() && copy.contains(id), "copy cannot independently redo");
    require(fingerprint(original) == before, "copy redo changed original");
    ModelDocument assigned;
    assigned = original;
    require(fingerprint(assigned) == before, "copy assignment lost state");
    require(assigned.setBoxDimensions(ModelDocument::initialBoxId(), {6, 7, 8}),
            "copy assignment edit failed");
    require(fingerprint(original) == before, "assigned edit changed original");
    auto& same = original;
    original = same;
    require(fingerprint(original) == before, "self assignment changed history");
    const auto copied = fingerprint(copy);
    ModelDocument moved(std::move(copy));
    require(fingerprint(moved) == copied, "move constructor lost history");
    copy = original; // Reassign a moved-from object before using it.
    assigned = std::move(moved);
    require(fingerprint(assigned) == copied && fingerprint(copy) == before,
            "move assignment or moved-from reassignment failed");
    moved = ModelDocument::empty();
    require(moved.features().empty(), "moved-from value cannot be replaced");
    DocumentTransaction transaction(original);
    auto candidateCopy = transaction.candidate();
    static_cast<void>(candidateCopy.addBoxFeature("Isolated candidate copy", {1, 2, 3}));
    require(snapshot(transaction.candidate()) == snapshot(original) &&
                fingerprint(original) == before,
            "candidate value copy shares model storage");
    static_cast<void>(transaction.edit().addBoxFeature("Abandoned candidate", {3, 4, 5}));
    require(transaction.prepare(), "prepare isolated candidate");
    transaction.rollback();
    require(fingerprint(original) == before, "prepared rollback changed original");
}
void noChangeAndConflicts() {
    ModelDocument document;
    const auto before = fingerprint(document);
    DocumentTransaction unchanged(document);
    auto& edit = unchanged.edit();
    const auto dimensions = edit.boxDimensions(ModelDocument::initialBoxId());
    static_cast<void>(edit.setBoxDimensions(ModelDocument::initialBoxId(), {7, 8, 9}));
    static_cast<void>(edit.setBoxDimensions(ModelDocument::initialBoxId(), dimensions));
    require(!unchanged.commit() && fingerprint(document) == before, "net no-op generated history");
    DocumentTransaction first(document);
    DocumentTransaction stale(document);
    static_cast<void>(first.edit().addBoxFeature("First", {1, 2, 3}));
    static_cast<void>(stale.edit().addBoxFeature("Stale", {3, 4, 5}));
    require(stale.prepare() && first.commit(), "prepare conflict fixture");
    const auto committed = fingerprint(document);
    mustThrow([&] { static_cast<void>(stale.commit()); });
    require(fingerprint(document) == committed, "stale transaction overwrote commit");
    DocumentTransaction frozen(document);
    auto& retained = frozen.edit();
    static_cast<void>(retained.addBoxFeature("Frozen", {2, 2, 2}));
    require(frozen.prepare(), "prepare frozen");
    static_cast<void>(retained.addBoxFeature("Late", {2, 2, 2}));
    mustThrow([&] { static_cast<void>(frozen.commit()); });
    require(fingerprint(document) == committed, "retained reference bypassed freeze");
    DocumentTransaction outer(document);
    mustThrow([&] { DocumentTransaction nested(outer.edit()); });
}

template <class Setup, class Operation>
void allocationSweep(const char* name, Setup setup, Operation operation) {
    int failures = 0;
    bool completed = false;
    for (int allocation = 0; allocation < 2000; ++allocation) {
        ModelDocument document = setup();
        const auto before = fingerprint(document);
        bool failed = false;
        test::failAllocationAfter(allocation);
        try {
            operation(document);
        } catch (const std::bad_alloc&) {
            failed = true;
        } catch (...) {
            test::disableAllocationFailure();
            throw;
        }
        test::disableAllocationFailure();
        if (!failed) {
            completed = true;
            break;
        }
        ++failures;
        require(fingerprint(document) == before,
                "allocation failure changed document/history/revision");
    }
    require(completed && failures > 0, "allocation sweep did not exercise failure and success");
    std::cout << name << ": " << failures << " allocation failures preserved state\n";
}
[[maybe_unused]] void allocationFailures() {
    const auto initial = [] {
        ModelDocument document;
        static_cast<void>(document.addBoxFeature("Redo preserved on failure", {2, 3, 4}));
        static_cast<void>(document.undo());
        return document;
    };
    {
        auto document = initial();
        DocumentTransaction prepared(document);
        static_cast<void>(prepared.edit().addBoxFeature("Prepared", {2, 3, 4}));
        require(prepared.prepare(), "prepare allocation-free commit");
        test::failAllocationAfter(0);
        bool committed = false;
        try {
            committed = prepared.commit();
        } catch (...) {
            test::disableAllocationFailure();
            throw;
        }
        test::disableAllocationFailure();
        require(committed, "prepared commit failed");
        std::cout << "Prepared commit requires no allocation\n";
    }
    allocationSweep(
        "sketch edit",
        [] {
            ModelDocument document;
            static_cast<void>(
                document.addSketchFeature("Sketch", ModelDocument::initialBoxId(), sketch()));
            return document;
        },
        [](auto& document) {
            auto changed = document.sketchParameters(FeatureId{3});
            changed.elements.values.push_back({SketchElementId{2}, SketchCircle{{8, 8}, 2}});
            changed.elements.nextId = 3;
            static_cast<void>(document.setSketchParameters(FeatureId{3}, changed));
        });
    ModelDocument copySource;
    static_cast<void>(copySource.addBoxFeature("Copy source history", {9, 8, 7}));
    const auto sourceBefore = fingerprint(copySource);
    allocationSweep("copy assignment", initial, [&](auto& document) { document = copySource; });
    require(fingerprint(copySource) == sourceBefore, "copy assignment changed its source");
    allocationSweep("single mutation", initial, [](auto& document) {
        static_cast<void>(document.addBoxFeature(std::string(200, 'b'), {3, 4, 5}));
    });
    allocationSweep("compound transaction", initial, [](auto& document) {
        DocumentTransaction transaction(document);
        const auto id =
            transaction.edit().addSketchFeature("Sketch", ModelDocument::initialBoxId(), sketch());
        static_cast<void>(transaction.edit().addExtrusionFeature("Extrude", id, {10, {}}));
        static_cast<void>(transaction.commit());
    });
    allocationSweep(
        "undo",
        [] {
            ModelDocument document;
            static_cast<void>(document.addBoxFeature("Undo", {3, 4, 5}));
            return document;
        },
        [](auto& document) { static_cast<void>(document.undo()); });
    allocationSweep(
        "redo",
        [] {
            ModelDocument document;
            static_cast<void>(document.addBoxFeature("Redo", {3, 4, 5}));
            static_cast<void>(document.undo());
            return document;
        },
        [](auto& document) { static_cast<void>(document.redo()); });
}
} // namespace
int main(int argc, char** argv) {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--allocations") {
#if defined(_MSC_VER) && _ITERATOR_DEBUG_LEVEL != 0
            throw std::runtime_error(
                "Allocation sweep requires MSVC Release (iterator debug disabled)");
#else
            allocationFailures();
#endif
        } else {
            require(argc == 1, "unknown arguments");
            independentCopiesAndMoves();
            compoundAndHistory();
            noChangeAndConflicts();
        }
        std::cout << "Document transaction tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        test::disableAllocationFailure();
        std::cerr << error.what() << '\n';
        return 1;
    }
}
