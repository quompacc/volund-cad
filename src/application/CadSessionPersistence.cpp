#include <fstream>
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
    std::string clean;
};

PreparedLoad prepareLoad(std::istream& input) {
    auto document = document::DocumentSerializer::read(input);
    std::ostringstream serialized;
    document::DocumentSerializer::write(document, serialized);
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
                state.status = FeatureRuntimeStatus::error;
                state.message = "Blocked by an earlier feature error";
            }
        }
        // No shape from the previously open document may leak into this failed model.
        evaluated = {};
    }
    return {std::move(document), std::move(evaluated), std::move(states), serialized.str()};
}
} // namespace

void CadSession::saveDocument(const std::filesystem::path& path) {
    saveDocumentUsing(path, platform::AtomicFileStore::write);
}

void CadSession::saveDocumentUsing(const std::filesystem::path& path,
                                   const DocumentWriter& writer) {
    auto preparedPath = std::filesystem::absolute(path);
    auto contents = serializedDocument();
    std::istringstream verification(contents);
    static_cast<void>(document::DocumentSerializer::read(verification));
    // Geometry may be broken but structurally valid, editable parameters must remain saveable.
    writer(preparedPath, contents);
    // All allocation and validation precede the irreversible filesystem commit.
    cleanDocumentState_.swap(contents);
    currentFile_.swap(preparedPath);
}

void CadSession::loadDocument(const std::filesystem::path& path) {
    auto preparedPath = std::filesystem::absolute(path);
    std::ifstream input(preparedPath, std::ios::binary);
    if (!input)
        throw std::runtime_error("Could not open the document for reading");
    auto candidate = prepareLoad(input);
    const auto selection = candidate.document.features().empty()
                               ? std::optional<document::FeatureId>{}
                               : candidate.document.features().back().id;
    static_assert(std::is_nothrow_move_assignable_v<document::ModelDocument>);
    static_assert(std::is_nothrow_move_assignable_v<TopoDS_Shape>);
    document_ = std::move(candidate.document);
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
