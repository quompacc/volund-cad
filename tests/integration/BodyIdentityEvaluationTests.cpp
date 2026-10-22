// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <BRepCheck_Analyzer.hxx>
#include <voelund/application/CadSession.hpp>
#include <voelund/document/DocumentSerializer.hpp>
#include <voelund/document/LegacyBodyIdentity.hpp>
#include <voelund/features/FeatureGraph.hpp>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <random>
#include <set>
#include <sstream>

namespace {
using namespace voelund;
using namespace document;

void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
std::string bytes(const ModelDocument& document) {
    std::ostringstream output;
    DocumentSerializer::write(document, output);
    return output.str();
}
auto identities(const features::FeatureGraphResult& result) {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> ids;
    for (const auto& body : result.bodies)
        ids.emplace_back(body.bodyId.value(), body.sourceFeature.value());
    std::ranges::sort(ids);
    return ids;
}
features::FeatureGraphResult checked(const ModelDocument& document) {
    const auto snapshot = DocumentSnapshot::capture(document);
    const auto expected = legacyBodyIdentities(snapshot.features(), snapshot.parameters());
    auto result = features::FeatureGraph{}.evaluate(document);
    require(result.features.size() == expected.size(), "Missing feature output identity");
    for (std::size_t index = 0; index < expected.size(); ++index)
        require(result.features[index].id == expected[index].feature &&
                    result.features[index].bodyId == expected[index].body,
                "Evaluation and migration disagree on body identity");
    std::set<std::uint64_t> bodies;
    for (const auto& body : result.bodies) {
        require(bodies.insert(body.bodyId.value()).second, "Independent bodies share an identity");
        const auto producer =
            std::ranges::find(result.features, body.sourceFeature, &features::EvaluatedFeature::id);
        require(producer != result.features.end() && producer->bodyId == body.bodyId &&
                    producer->shape.IsSame(body.shape) && BRepCheck_Analyzer(body.shape).IsValid(),
                "Body producer, identity and current geometry disagree");
    }
    return result;
}
void modifiersAndModeChanges() {
    ModelDocument document;
    const auto initial = checked(document);
    require(initial.bodies.size() == 1 && initial.bodies.front().bodyId == BodyId{1} &&
                initial.bodies.front().sourceFeature == FeatureId{2},
            "Boolean output replaced body identity with producer ID");
    auto cylinder = document.cylinderParameters(FeatureId{2});
    for (auto mode :
         {CylinderResultMode::separate, CylinderResultMode::cut, CylinderResultMode::intersect,
          CylinderResultMode::join, CylinderResultMode::separate}) {
        cylinder.resultMode = mode;
        require(document.setCylinderParameters(FeatureId{2}, cylinder), "Mode change was a no-op");
        const auto result = checked(document);
        const bool separate = mode == CylinderResultMode::separate;
        require(result.bodies.size() == (separate ? 2U : 1U) &&
                    result.features.back().bodyId == BodyId{separate ? 2U : 1U},
                "Mode change lost target/origin body distinction");
        std::istringstream input(bytes(document));
        require(identities(checked(DocumentSerializer::read(input))) == identities(result),
                "Save/open lost mode-dependent body assignment");
        require(document.undo() && document.redo() &&
                    identities(checked(document)) == identities(result),
                "Mode history changed body identities");
    }
}
void namesAndOrder() {
    auto document = ModelDocument::empty();
    const auto first = document.addBoxFeature("Same", {40, 30, 20});
    const auto second = document.addBoxFeature("Same", {3, 4, 5});
    const auto separate =
        document.addCylinderFeature("Same", first, {2, 5, {}, CylinderResultMode::separate});
    const auto expected = identities(checked(document));
    require(expected ==
                std::vector<std::pair<std::uint64_t, std::uint64_t>>{
                    {first.value(), first.value()},
                    {second.value(), second.value()},
                    {separate.value(), separate.value()}},
            "New body origin assignment");
    // Explicit legacy fixture: renamed independent roots in reversed order.
    // Product output is binary; never edit it as if it were legacy text.
    std::ostringstream reordered;
    reordered << "VOLUND 16\nNEXT_ID 4\nFEATURES 3\n"
              << "BOX " << second.value() << " \"Renamed\" 3 4 5 0 0 0 0 0 0\n"
              << "BOX " << first.value() << " \"Renamed\" 40 30 20 0 0 0 0 0 0\n"
              << "CYLINDER " << separate.value() << " \"Renamed\" " << first.value()
              << " 2 5 0 0 0 0 0 0 SEPARATE\n";
    std::istringstream input(reordered.str());
    const auto loaded = DocumentSerializer::read(input);
    require(loaded.features().front().id == second && identities(checked(loaded)) == expected,
            "Rename/reorder changed body identity or merged bodies");
    std::istringstream native(bytes(loaded));
    const auto reopened = DocumentSerializer::read(native);
    require(DocumentSnapshot::capture(reopened) == DocumentSnapshot::capture(loaded) &&
                reopened.documentId() == loaded.documentId() &&
                identities(checked(reopened)) == expected,
            "Native roundtrip lost reordered content, document or body identities");
}
void newBranchDoesNotReuseBody() {
    auto document = ModelDocument::empty();
    const auto old = document.addBoxFeature("Old", {2, 3, 4});
    const auto oldBody = checked(document).bodies.front().bodyId;
    require(document.undo(), "Undo old body");
    const auto replacement = document.addBoxFeature("New", {2, 3, 4});
    require(replacement != old && checked(document).bodies.front().bodyId != oldBody,
            "Replacement body reused the discarded origin identity");
}
void historicalInputsRemainExplicitlyUnsupported() {
    ModelDocument document;
    const auto input = document.addCylinderFeature("Historical input", FeatureId{1},
                                                   {2, 5, {}, CylinderResultMode::join});
    const auto before = DocumentSnapshot::capture(document);
    bool rejected = false;
    try {
        (void)features::FeatureGraph{}.evaluate(document);
    } catch (const features::FeatureEvaluationError& error) {
        rejected = error.featureId() == input &&
                   std::string(error.what()).find("earlier feature result") != std::string::npos;
    }
    require(rejected && before == DocumentSnapshot::capture(document),
            "Body identity silently selected historical geometry or mutated the document");
}
void applicationSaveAs() {
    struct Files {
        std::filesystem::path first =
            std::filesystem::temp_directory_path() /
            ("d11c-" + std::to_string(std::random_device{}()) + ".volund");
        std::filesystem::path second = first.string() + ".copy.volund";
        ~Files() {
            std::error_code error;
            std::filesystem::remove(first, error);
            std::filesystem::remove(second, error);
        }
    } files;
    application::CadSession session;
    const auto originalBody = session.currentBodies().front().bodyId;
    const auto created = session.createBoxFeature();
    require(created.status() == application::CommandStatus::committed && created.feature(),
            "Application body creation failed");
    const auto addedBody = session.currentBodies().back().bodyId;
    require(addedBody != originalBody, "Application merged independent body IDs");
    session.saveDocument(files.first);
    session.saveDocument(files.second);
    session.loadDocument(files.second);
    require(session.currentBodies().size() == 2 &&
                session.currentBodies().front().bodyId == originalBody &&
                session.currentBodies().back().bodyId == addedBody &&
                session.currentBodies().back().sourceFeature == *created.feature() &&
                !session.isModified(),
            "Application Save-As/open changed body identity");
}
} // namespace
int main() {
    try {
        modifiersAndModeChanges();
        namesAndOrder();
        newBranchDoesNotReuseBody();
        historicalInputsRemainExplicitlyUnsupported();
        applicationSaveAs();
        std::cout << "Body identity evaluation tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
