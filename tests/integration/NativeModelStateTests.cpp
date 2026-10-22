// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "../../src/document/NativeModelState.hpp"
#include "../unit/AllocationFailure.hpp"
#include <Standard_OutOfMemory.hxx>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <voelund/kernel/OcctCompatibility.hpp>

namespace voelund::document::detail {
struct NativeModelStateTestAccess {
    static void exhaust(NativeModelState& state) {
        state.state_->revision = std::numeric_limits<std::uint64_t>::max();
    }
};
} // namespace voelund::document::detail
namespace {
using namespace voelund;
using namespace document;
using detail::NativeModelState;
void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
template <class Action> void rejected(Action action) {
    bool failed = false;
    try {
        action();
    } catch (const std::exception&) {
        failed = true;
    }
    require(failed, "Expected rejected native state change");
}
IdentifiedDocumentSnapshot initial() {
    auto model = ModelDocument::empty();
    static_cast<void>(model.addBoxFeature("Base", {10, 20, 30}));
    return IdentifiedDocumentSnapshot::restore(DocumentId{42, 7}, DocumentSnapshot::capture(model));
}
IdentifiedDocumentSnapshot mixed(const IdentifiedDocumentSnapshot& base) {
    auto model = base.snapshot().toDocument();
    static_cast<void>(model.addCylinderFeature("Cylinder", FeatureId{1},
                                               {2, 5, {}, CylinderResultMode::separate}));
    const auto sketch = model.addSketchFeature(
        "Sketch", FeatureId{1},
        {{0, 0, 30, 0, 0, 1, 1, 0, 0},
         SketchElements{{{SketchElementId{1}, SketchRectangle{1, 1, 4, 4}}}, 2}});
    static_cast<void>(model.addExtrusionFeature("Extrusion", sketch, {3, {}}));
    return base.revised(DocumentSnapshot::capture(model));
}
void lifecycle() {
    const auto base = initial(), all = mixed(base);
    NativeModelState state(base);
    require(state.snapshot() == base && !state.canUndo(), "Initial parity");
    {
        auto cancelled = state.prepare(all);
    }
    require(state.snapshot() == base && state.revision() == 0, "Abandoned preparation published");
    auto prepared = state.prepare(all);
    require(state.snapshot() == base, "Prepare published early");
    require(state.commit(std::move(prepared)) && state.snapshot() == all && state.revision() == 1,
            "Compound native commit failed");
    rejected([&] { (void)state.commit(std::move(prepared)); });
    require(state.commit(state.prepareUndo()) && !state.canUndo() && state.canRedo(),
            "Compound change required multiple undos");
    const auto undone = state.snapshot();
    require(undone.id() == base.id() &&
                undone.snapshot().matchesContent(base.snapshot().toDocument()) &&
                undone.snapshot().nextFeatureId() == all.snapshot().nextFeatureId(),
            "Undo changed identity or lowered watermark");
    require(state.commit(state.prepareRedo()) && state.snapshot() == all, "Redo parity");
    require(state.commit(state.prepareUndo()), "Second undo");
    auto branch = state.snapshot().snapshot().toDocument();
    const auto replacement = branch.addBoxFeature("Branch", {3, 4, 5});
    require(replacement.value() >= all.snapshot().nextFeatureId(), "Reused published ID");
    require(state.commit(state.prepare(undone.revised(DocumentSnapshot::capture(branch)))) &&
                !state.canRedo(),
            "New branch retained redo");
    const auto before = state.snapshot();
    const auto revision = state.revision();
    require(!state.commit(state.prepare(before)) && state.revision() == revision,
            "No-op advanced revision");
    rejected([&] { (void)state.prepare(base); });
    rejected([&] { (void)state.prepare(before.duplicate()); });
    auto invalid = before.snapshot().toDocument();
    rejected([&] {
        static_cast<void>(invalid.addBoxFeature(std::string(10000, 'x'), {2, 3, 4}));
        (void)state.prepare(before.revised(DocumentSnapshot::capture(invalid)));
    });
    require(state.snapshot() == before && state.revision() == revision, "Rejection changed state");
    auto first = state.prepareUndo(), stale = state.prepareUndo();
    auto moved = std::move(first);
    rejected([&] { (void)state.commit(std::move(first)); });
    NativeModelState other(before);
    rejected([&] { (void)other.commit(std::move(moved)); });
    require(state.commit(std::move(moved)), "Moved/foreign candidate not retained");
    rejected([&] { (void)state.commit(std::move(stale)); });
    const auto atLimit = state.snapshot();
    detail::NativeModelStateTestAccess::exhaust(state);
    rejected([&] { (void)state.prepareRedo(); });
    require(state.snapshot() == atLimit, "Revision overflow changed state");
}
void allocations() {
    const auto base = initial();
    auto model = base.snapshot().toDocument();
    static_cast<void>(model.setBoxDimensions(FeatureId{1}, {11, 20, 30}));
    const auto changed = base.revised(DocumentSnapshot::capture(model));
    for (int operation = 0; operation != 3; ++operation) {
        bool completed = false;
        int failures = 0;
        for (int allocation = 0; allocation < 10000; ++allocation) {
            NativeModelState state(base);
            if (operation > 0)
                (void)state.commit(state.prepare(changed));
            if (operation > 1)
                (void)state.commit(state.prepareUndo());
            const auto before = state.snapshot();
            const auto revision = state.revision();
            const bool undo = state.canUndo(), redo = state.canRedo();
            bool failed = false;
            test::failAllocationAfter(allocation);
            try {
                auto prepared = operation == 0   ? state.prepare(changed)
                                : operation == 1 ? state.prepareUndo()
                                                 : state.prepareRedo();
                test::disableAllocationFailure();
                test::failAllocationAfter(0);
                (void)state.commit(std::move(prepared));
                test::disableAllocationFailure();
                completed = true;
            } catch (const std::bad_alloc&) {
                failed = true;
            } catch (const Standard_OutOfMemory&) {
                failed = true;
            } catch (...) {
                test::disableAllocationFailure();
                throw;
            }
            test::disableAllocationFailure();
            if (completed)
                break;
            require(failed && state.snapshot() == before && state.revision() == revision &&
                        state.canUndo() == undo && state.canRedo() == redo,
                    "Allocation failure changed native model/history");
            ++failures;
        }
        require(completed && failures > 0, "Incomplete native allocation sweep");
        std::cout << "Native operation " << operation << ": " << failures
                  << " allocation failures preserved state; commit without allocation\n";
    }
}
} // namespace
int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--allocations") {
#if defined(_MSC_VER) && _ITERATOR_DEBUG_LEVEL != 0
            throw std::runtime_error("Allocation sweep requires Release on MSVC");
#else
            allocations();
#endif
        } else {
            require(argc == 1, "Unknown arguments");
            lifecycle();
        }
        std::cout << "PASS isolated native model state\n";
        return 0;
    } catch (const Standard_Failure& error) {
        test::disableAllocationFailure();
        std::cerr << kernel::occtFailureDiagnostic(error) << '\n';
    } catch (const std::exception& error) {
        test::disableAllocationFailure();
        std::cerr << error.what() << '\n';
    }
    return 1;
}
