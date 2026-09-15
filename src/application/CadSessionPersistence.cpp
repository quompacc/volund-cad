#include <algorithm>
#include <fstream>
#include <limits>
#include <sstream>
#include <type_traits>
#include <utility>
#include <voelund/application/CadSession.hpp>
#include <voelund/document/DocumentSerializer.hpp>
#include <voelund/platform/AtomicFileStore.hpp>

namespace voelund::application {
namespace {
struct PreparedLoad {
    document::ModelDocument document;
    features::FeatureGraphResult evaluated;
    std::vector<FeatureRuntimeState> states;
    std::optional<document::DocumentSnapshot> clean;
    std::optional<document::MigrationReport> report;
};

PreparedLoad prepareLoad(std::istream& input) {
    auto imported = document::DocumentSerializer::readWithReport(input);
    auto document = imported.snapshot.toDocument();
    std::vector<FeatureRuntimeState> states;
    for (const auto& feature : document.features())
        states.push_back({feature.id, FeatureRuntimeStatus::current, {}});
    features::FeatureGraphResult evaluated;
    try {
        if (!document.features().empty())
            evaluated = features::FeatureGraph{}.evaluate(document);
    } catch (const features::FeatureEvaluationError& error) {
        bool failed = false;
        for (auto& state : states) {
            if (state.id == error.featureId()) {
                failed = true;
                state.status = FeatureRuntimeStatus::error;
                state.message = error.what();
            } else if (failed) {
                state.status = FeatureRuntimeStatus::blocked;
                state.message = "Blocked by an earlier feature error";
            }
        }
        // No shape from the previously open document may leak into this failed model.
        evaluated = {};
    }
    return {std::move(document), std::move(evaluated), std::move(states),
            std::move(imported.snapshot), std::move(imported.report)};
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
    if (generation_ == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("Document generation exhausted");
    auto preparedPath = std::filesystem::absolute(path);
    std::ifstream input(preparedPath, std::ios::binary);
    if (!input)
        throw std::runtime_error("Could not open the document for reading");
    auto candidate = prepareLoad(input);
    const bool valid = std::ranges::all_of(candidate.states, [](const auto& state) {
        return state.status == FeatureRuntimeStatus::current;
    });
    const auto selection = candidate.document.features().empty()
                               ? std::optional<document::FeatureId>{}
                               : candidate.document.features().back().id;
    static_assert(std::is_nothrow_move_assignable_v<document::ModelDocument>);
    static_assert(std::is_nothrow_move_assignable_v<TopoDS_Shape>);
    static_assert(std::is_nothrow_move_assignable_v<decltype(importReport_)>);
    document_ = std::move(candidate.document);
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
