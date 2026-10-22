// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CadSessionEvaluationDetail.hpp"
#include <algorithm>
#include <fstream>
#include <limits>
#include <sstream>
#include <type_traits>
#include <utility>
#include <voelund/application/CadSession.hpp>
#include <voelund/document/DocumentSerializer.hpp>
#include <voelund/features/SketchRegionIdentity.hpp>
#include <voelund/platform/AtomicFileStore.hpp>

namespace voelund::application {
namespace {
struct PreparedLoad {
    document::ModelDocument document;
    features::FeatureGraphResult evaluated;
    std::shared_ptr<const features::FeatureGraphEvaluation> evaluation;
    std::vector<FeatureRuntimeState> states;
    std::optional<document::DocumentSnapshot> clean;
    std::optional<document::MigrationReport> report;
};

// R09 documented mapping of a legacy minimum-element profile to its filled regions.
std::optional<std::vector<document::SketchRegionId>>
mapLegacyProfile(const document::SketchParameters& sketch, document::SketchProfileId profile) {
    try {
        const auto identities = features::identifySketchRegions(sketch.elements.values);
        return features::legacyProfileRegions(identities, sketch.elements.values, profile);
    } catch (const std::exception&) {
        return std::nullopt; // Unmappable selections stay legacy and need repair.
    }
}

PreparedLoad prepareLoad(std::istream& input) {
    auto imported = document::DocumentSerializer::readWithReport(input);
    imported.snapshot =
        imported.snapshot.markLegacySketchReferencesForRepair().migrateLegacyExtrusionProfiles(
            mapLegacyProfile);
    auto document = imported.toDocument();
    auto evaluation =
        features::FeatureGraph{}.evaluateDetailed(document::DocumentSnapshot::capture(document));
    auto states = detail::runtimeStates(evaluation->report);
    // Full-scene display remains unavailable on an erroneous Open; valid branches
    // are exposed separately from the revision-bound evaluation.
    auto evaluated = evaluation->valid() ? evaluation->result : features::FeatureGraphResult{};
    return {std::move(document), std::move(evaluated),         std::move(evaluation),
            std::move(states),   std::move(imported.snapshot), std::move(imported.report)};
}
} // namespace

void CadSession::saveDocument(const std::filesystem::path& path) {
    saveDocumentUsing(path, platform::AtomicFileStore::write);
}

void CadSession::saveDocumentUsing(const std::filesystem::path& path,
                                   const DocumentWriter& writer) {
    auto preparedPath = std::filesystem::absolute(path);
    auto contents = serializedDocument();
    auto clean = std::optional{document::DocumentSnapshot::capture(document_)};
    std::istringstream verification(contents);
    static_cast<void>(document::DocumentSerializer::read(verification));
    // Geometry may be broken but structurally valid, editable parameters must remain saveable.
    writer(preparedPath, contents);
    // All allocation and validation precede the irreversible filesystem commit.
    static_assert(std::is_nothrow_swappable_v<decltype(cleanDocumentState_)>);
    cleanDocumentState_.swap(clean);
    currentFile_.swap(preparedPath);
}

void CadSession::loadDocument(const std::filesystem::path& path) {
    auto preparedPath = std::filesystem::absolute(path);
    std::ifstream input(preparedPath, std::ios::binary);
    if (!input)
        throw std::runtime_error("Could not open the document for reading");
    loadFrom(input, std::move(preparedPath), true);
}

std::optional<std::size_t> CadSession::savedFeatureCount() const {
    if (!cleanDocumentState_ || currentFile_.empty())
        return std::nullopt;
    return cleanDocumentState_->features().size();
}

void CadSession::restoreDocument(std::string_view bytes, document::DocumentId expected) {
    std::istringstream input{std::string(bytes)};
    loadFrom(input, {}, false, expected);
}

void CadSession::loadFrom(std::istream& input, std::filesystem::path preparedPath,
                          bool keepCleanState, std::optional<document::DocumentId> expected) {
    if (generation_ == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("Document generation exhausted");
    auto candidate = prepareLoad(input);
    if (expected && candidate.document.documentId() != *expected)
        throw std::runtime_error("The recovery snapshot belongs to another document");
    if (!keepCleanState)
        candidate.clean.reset(); // A restored snapshot is unsaved work.
    // The prepared document already has its persisted native ID or a fresh legacy ID.
    const bool valid = std::ranges::all_of(candidate.states, [](const auto& state) {
        return state.status == FeatureRuntimeStatus::current ||
               state.status == FeatureRuntimeStatus::suppressed || // M14
               state.status == FeatureRuntimeStatus::rolledBack;
    });
    const auto selection = candidate.document.features().empty()
                               ? std::optional<document::FeatureId>{}
                               : candidate.document.features().back().id;
    static_assert(std::is_nothrow_move_assignable_v<document::ModelDocument>);
    static_assert(std::is_nothrow_move_assignable_v<TopoDS_Shape>);
    static_assert(std::is_nothrow_move_assignable_v<decltype(importReport_)>);
    document_ = std::move(candidate.document);
    evaluation_ = std::move(candidate.evaluation);
    importReport_ = std::move(candidate.report);
    ++generation_;
    geometryStamp_ = valid ? std::optional<DocumentStamp>{sceneStatus().document} : std::nullopt;
    currentShape_ = std::move(candidate.evaluated.resultShape);
    currentBodies_.swap(candidate.evaluated.bodies);
    currentFeatures_.swap(candidate.evaluated.features);
    featureStates_.swap(candidate.states);
    cleanDocumentState_.swap(candidate.clean);
    currentFile_.swap(preparedPath);
    selectedFeature_ = selection;
    selectedSketchElement_.reset();
    selectedSketchProfiles_.clear();
}
} // namespace voelund::application
