#include "../../src/document/DocumentIdGeneration.hpp"
#include <array>
#include <future>
#include <iostream>
#include <set>
#include <voelund/document/IdentifiedDocumentSnapshot.hpp>
#include <voelund/document/LegacyBodyIdentity.hpp>

namespace {
using namespace voelund::document;
using Bytes = std::array<std::uint8_t, 16>;
void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
void deterministicGeneration() {
    Bytes ordered{};
    for (unsigned index = 0; index < ordered.size(); ++index)
        ordered[index] = static_cast<std::uint8_t>(index * 17);
    require(documentIdToHex(detail::generateDocumentIdUsing([&] { return ordered; })) ==
                "00112233445566778899aabbccddeeff",
            "Entropy byte order changed");
    Bytes maximum{};
    maximum.fill(255);
    require(detail::generateDocumentIdUsing([&] { return maximum; }) ==
                DocumentId{UINT64_MAX, UINT64_MAX},
            "Generation masked identity bits");
    unsigned calls = 0;
    const auto afterZero =
        detail::generateDocumentIdUsing([&] { return ++calls == 1 ? Bytes{} : ordered; });
    require(calls == 2 && afterZero == documentIdFromHex("00112233445566778899aabbccddeeff"),
            "Null identity was accepted or fallback substituted");
    calls = 0;
    const auto afterCollision = detail::generateDocumentIdUsing(
        [&] { return ++calls == 1 ? ordered : maximum; }, afterZero);
    require(calls == 2 && afterCollision != afterZero, "Duplicate retained source namespace");
    for (const auto candidate : {Bytes{}, ordered}) {
        calls = 0;
        bool rejected = false;
        try {
            (void)detail::generateDocumentIdUsing(
                [&] {
                    ++calls;
                    return candidate;
                },
                afterZero);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        require(rejected && calls == 16, "Invalid generator did not terminate without fallback");
    }
    calls = 0;
    bool propagated = false;
    try {
        (void)detail::generateDocumentIdUsing([&]() -> Bytes {
            ++calls;
            throw std::logic_error("Injected entropy failure");
        });
    } catch (const std::logic_error&) {
        propagated = true;
    }
    require(propagated && calls == 1, "Entropy failure retried or swallowed");
}
void nativeGeneration() {
    // Smoke check of real OS entropy and concurrent access, not a uniqueness proof.
    std::array<std::future<std::set<std::string>>, 4> workers;
    for (auto& worker : workers) {
        worker = std::async(std::launch::async, [] {
            std::set<std::string> ids;
            for (unsigned index = 0; index < 512; ++index) {
                const auto id = generateDocumentId();
                const auto encoded = documentIdToHex(id);
                require(documentIdFromHex(encoded) == id && ids.insert(encoded).second,
                        "Generated identities repeated or failed canonical round trip");
            }
            return ids;
        });
    }
    std::set<std::string> ids;
    for (auto& worker : workers) {
        const auto batch = worker.get();
        ids.insert(batch.begin(), batch.end());
    }
    require(ids.size() == 2048, "Concurrent generators repeated identities");
}
void lifecycle() {
    auto model = ModelDocument::empty();
    const auto first = model.addBoxFeature("Same name", {1, 2, 3});
    const auto second = model.addBoxFeature("Same name", {3, 4, 5});
    require(model.undo(), "Setup undo failed");
    const auto payload = DocumentSnapshot::capture(model);
    const auto original = IdentifiedDocumentSnapshot::create(payload);
    const auto independent = IdentifiedDocumentSnapshot::create(payload);
    require(original.id() != independent.id(), "Equal new documents share a namespace");
    const auto copy = original;
    require(copy == original, "Value copy changed document identity");
    const auto restored = IdentifiedDocumentSnapshot::restore(
        documentIdFromHex(documentIdToHex(original.id())), payload);
    require(restored == original, "Restore generated an identity or changed payload");
    const auto duplicate = original.duplicate();
    require(duplicate.id() != original.id() && duplicate.snapshot() == payload && original == copy,
            "Explicit duplicate changed source or local identities");
    const auto originalBodies = legacyBodyIdentities(payload.features(), payload.parameters());
    const auto duplicateBodies =
        legacyBodyIdentities(duplicate.snapshot().features(), duplicate.snapshot().parameters());
    require(originalBodies == duplicateBodies, "Duplicate renumbered document-local bodies");
    auto duplicateModel = duplicate.snapshot().toDocument();
    require(!duplicateModel.canUndo() && !duplicateModel.canRedo() &&
                duplicateModel.features().front().id == first,
            "Snapshot copied transient history or changed feature identity");
    const auto third = duplicateModel.addBoxFeature("New", {2, 3, 4});
    require(third.value() > second.value(), "Duplicate reused reserved local feature ID");
    const auto edited = duplicate.revised(DocumentSnapshot::capture(duplicateModel));
    require(edited.id() == duplicate.id() && edited.snapshot() != payload && original == copy,
            "Revision changed namespace or source snapshot");
    require(duplicateModel.undo(), "Revision undo failed");
    const auto undone = edited.revised(DocumentSnapshot::capture(duplicateModel));
    require(undone.id() == duplicate.id() && undone.snapshot().matchesContent(model) &&
                undone.snapshot().nextFeatureId() > payload.nextFeatureId(),
            "Undo lost identity, content or allocation watermark");
    bool rejected = false;
    try {
        (void)edited.revised(payload);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected && original == copy &&
                edited.snapshot().nextFeatureId() > payload.nextFeatureId(),
            "Stale revision lowered watermark or changed source");
    require(IdentifiedDocumentSnapshot::restore(DocumentId{UINT64_MAX, UINT64_MAX}, payload).id() ==
                DocumentId{UINT64_MAX, UINT64_MAX},
            "Restore narrowed stored identity");
}
} // namespace
int main(int argc, char** argv) {
    try {
        require(argc == 2, "Expected generator or lifecycle test");
        const std::string mode = argv[1];
        if (mode == "generator") {
            deterministicGeneration();
            nativeGeneration();
        } else {
            require(mode == "lifecycle", "Unknown identity test mode");
            lifecycle();
        }
        std::cout << "PASS document identity " << mode << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
