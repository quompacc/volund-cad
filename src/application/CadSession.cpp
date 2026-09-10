#include <voelund/application/CadSession.hpp>

#include <voelund/document/DocumentSerializer.hpp>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace voelund::application {
namespace {

std::size_t featureCount(
    std::span<const document::FeatureDescriptor> features,
    document::FeatureKind kind)
{
    std::size_t count = 0;
    for (const document::FeatureDescriptor& feature : features) {
        if (feature.kind == kind) {
            ++count;
        }
    }
    return count;
}

} // namespace

CadSession::CadSession()
{
    if (!recompute()) {
        throw std::runtime_error("Initial feature graph evaluation failed");
    }
    cleanDocumentState_ = serializedDocument();
}

document::FeatureId CadSession::addBoxFeature()
{
    const std::size_t sequence = featureCount(document_.features(), document::FeatureKind::box) + 1;
    const document::FeatureId id = document_.addBoxFeature(
        "Box " + std::to_string(sequence),
        kernel::BoxDimensions{20.0, 20.0, 40.0});
    selectedFeature_ = id;
    static_cast<void>(recompute());
    return id;
}

document::FeatureId CadSession::addCylinderFeature()
{
    if (!canAddCylinder()) {
        throw std::logic_error("A cylinder requires an input feature");
    }
    const std::size_t sequence =
        featureCount(document_.features(), document::FeatureKind::cylinder) + 1;
    const document::FeatureId inputFeature = document_.features().back().id;
    const document::CylinderParameters parameters{
        8.0, 25.0,
        kernel::RigidTransform{0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        document::CylinderResultMode::separate};
    const document::FeatureId id = document_.addCylinderFeature(
        "Cylinder " + std::to_string(sequence), inputFeature, parameters);
    selectedFeature_ = id;
    static_cast<void>(recompute());
    return id;
}

document::FeatureId CadSession::addSketchExtrusion(
    document::FeatureId supportFeature,
    const document::SketchPlane& plane,
    const document::SketchRectangle& rectangle,
    double distance)
{
    if (!document_.contains(supportFeature)) {
        throw std::invalid_argument("Sketch support is not in the document");
    }
    const std::size_t sketchSequence =
        featureCount(document_.features(), document::FeatureKind::sketch) + 1;
    const document::FeatureId sketchId = document_.addSketchFeature(
        "Sketch " + std::to_string(sketchSequence),
        supportFeature,
        document::SketchParameters{plane, rectangle});
    const std::size_t extrusionSequence =
        featureCount(document_.features(), document::FeatureKind::extrusion) + 1;
    const document::FeatureId extrusionId = document_.addExtrusionFeature(
        "Extrusion " + std::to_string(extrusionSequence),
        sketchId,
        document::ExtrusionParameters{distance});
    selectedFeature_ = extrusionId;
    static_cast<void>(recompute());
    return extrusionId;
}

bool CadSession::canAddCylinder() const noexcept
{
    return !document_.features().empty();
}

TopoDS_Shape CadSession::currentShape() const
{
    return currentShape_;
}

std::span<const features::EvaluatedBody> CadSession::currentBodies() const noexcept
{
    return currentBodies_;
}

std::span<const document::FeatureDescriptor> CadSession::features() const noexcept
{
    return document_.features();
}

std::optional<document::FeatureId> CadSession::selectedFeature() const noexcept
{
    return selectedFeature_;
}

bool CadSession::selectFeature(document::FeatureId featureId) noexcept
{
    if (!document_.contains(featureId)) {
        return false;
    }
    selectedFeature_ = featureId;
    return true;
}

bool CadSession::canTransformFeature(document::FeatureId featureId) const noexcept
{
    const auto descriptors = document_.features();
    const auto feature = std::ranges::find(descriptors, featureId, &document::FeatureDescriptor::id);
    if (feature == descriptors.end()) {
        return false;
    }
    if (feature->kind == document::FeatureKind::sketch
        || feature->kind == document::FeatureKind::extrusion) {
        return false;
    }
    if (feature->kind == document::FeatureKind::cylinder
        && document_.cylinderParameters(featureId).resultMode
            != document::CylinderResultMode::separate) {
        return false;
    }
    const auto body = std::ranges::find(
        currentBodies_, featureId, &features::EvaluatedBody::sourceFeature);
    if (body == currentBodies_.end()) {
        return false;
    }
    for (const FeatureRuntimeState& state : featureStates_) {
        if (state.id == featureId) {
            return state.status == FeatureRuntimeStatus::current;
        }
    }
    return false;
}

const document::FeatureDescriptor& CadSession::selectedFeatureDescriptor() const
{
    for (const document::FeatureDescriptor& feature : document_.features()) {
        if (feature.id == selectedFeature_.value()) {
            return feature;
        }
    }
    throw std::logic_error("Selected feature is not in the document");
}

const kernel::BoxDimensions& CadSession::selectedBoxDimensions() const
{
    return document_.boxDimensions(selectedFeature_.value());
}

const document::BoxParameters& CadSession::selectedBoxParameters() const
{
    return document_.boxParameters(selectedFeature_.value());
}

bool CadSession::setSelectedBoxDimensions(const kernel::BoxDimensions& dimensions)
{
    if (!document_.setBoxDimensions(selectedFeature_.value(), dimensions)) {
        return false;
    }
    static_cast<void>(recompute());
    return true;
}

bool CadSession::setSelectedBoxParameters(const document::BoxParameters& parameters)
{
    if (!document_.setBoxParameters(selectedFeature_.value(), parameters)) {
        return false;
    }
    static_cast<void>(recompute());
    return true;
}

const document::CylinderParameters& CadSession::selectedCylinderParameters() const
{
    return document_.cylinderParameters(selectedFeature_.value());
}

bool CadSession::setSelectedCylinderParameters(const document::CylinderParameters& parameters)
{
    if (!document_.setCylinderParameters(selectedFeature_.value(), parameters)) {
        return false;
    }
    static_cast<void>(recompute());
    return true;
}

const document::ExtrusionParameters& CadSession::selectedExtrusionParameters() const
{
    return document_.extrusionParameters(selectedFeature_.value());
}

bool CadSession::setSelectedExtrusionParameters(
    const document::ExtrusionParameters& parameters)
{
    if (!document_.setExtrusionParameters(selectedFeature_.value(), parameters)) {
        return false;
    }
    static_cast<void>(recompute());
    return true;
}

const kernel::RigidTransform& CadSession::selectedFeaturePlacement() const
{
    return document_.featurePlacement(selectedFeature_.value());
}

bool CadSession::setSelectedFeaturePlacement(const kernel::RigidTransform& placement)
{
    if (!document_.setFeaturePlacement(selectedFeature_.value(), placement)) {
        return false;
    }
    static_cast<void>(recompute());
    return true;
}

std::span<const FeatureRuntimeState> CadSession::featureStates() const noexcept
{
    return featureStates_;
}

bool CadSession::canUndo() const noexcept
{
    return document_.canUndo();
}

bool CadSession::canRedo() const noexcept
{
    return document_.canRedo();
}

bool CadSession::undo()
{
    if (!document_.undo()) {
        return false;
    }
    normalizeSelection();
    static_cast<void>(recompute());
    return true;
}

bool CadSession::redo()
{
    if (!document_.redo()) {
        return false;
    }
    normalizeSelection();
    static_cast<void>(recompute());
    return true;
}

bool CadSession::isModified() const
{
    return serializedDocument() != cleanDocumentState_;
}

void CadSession::newDocument()
{
    document_ = document::ModelDocument::empty();
    currentShape_.Nullify();
    currentBodies_.clear();
    selectedFeature_.reset();
    markFeaturesCurrent();
    cleanDocumentState_ = serializedDocument();
}

void CadSession::saveDocument(const std::filesystem::path& path)
{
    if (!document_.features().empty()) {
        static_cast<void>(featureGraph_.evaluate(document_));
    }
    const std::string contents = serializedDocument();
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Could not open the document for writing");
    }
    output << contents;
    if (!output) {
        throw std::runtime_error("Failed to write Völund document");
    }
    cleanDocumentState_ = contents;
}

void CadSession::loadDocument(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Could not open the document for reading");
    }
    document::ModelDocument candidate = document::DocumentSerializer::read(input);
    features::FeatureGraphResult result;
    if (!candidate.features().empty()) {
        result = featureGraph_.evaluate(candidate);
    }
    document_ = std::move(candidate);
    currentShape_ = document_.features().empty()
        ? TopoDS_Shape{}
        : std::move(result.resultShape);
    currentBodies_ = std::move(result.bodies);
    selectedFeature_ = document_.features().empty()
        ? std::optional<document::FeatureId>{}
        : document_.features().back().id;
    markFeaturesCurrent();
    cleanDocumentState_ = serializedDocument();
}

bool CadSession::recompute()
{
    if (document_.features().empty()) {
        currentShape_.Nullify();
        currentBodies_.clear();
        markFeaturesCurrent();
        return true;
    }
    try {
        features::FeatureGraphResult candidate = featureGraph_.evaluate(document_);
        currentShape_ = candidate.resultShape;
        currentBodies_ = std::move(candidate.bodies);
        markFeaturesCurrent();
        return true;
    } catch (const features::FeatureEvaluationError& error) {
        markFeaturesCurrent();
        markFeatureError(error.featureId(), error.what());
        return false;
    }
}

std::string CadSession::serializedDocument() const
{
    std::ostringstream output;
    document::DocumentSerializer::write(document_, output);
    return output.str();
}

void CadSession::markFeaturesCurrent()
{
    featureStates_.clear();
    featureStates_.reserve(document_.features().size());
    for (const document::FeatureDescriptor& feature : document_.features()) {
        featureStates_.push_back(
            FeatureRuntimeState{feature.id, FeatureRuntimeStatus::current, {}});
    }
}

void CadSession::markFeatureError(
    document::FeatureId featureId,
    const std::string& message)
{
    bool failureReached = false;
    for (FeatureRuntimeState& state : featureStates_) {
        if (state.id == featureId) {
            state.status = FeatureRuntimeStatus::error;
            state.message = message;
            failureReached = true;
        } else if (failureReached) {
            state.status = FeatureRuntimeStatus::error;
            state.message = "Blocked by an earlier feature error";
        }
    }
}

void CadSession::normalizeSelection() noexcept
{
    if (selectedFeature_.has_value() && document_.contains(*selectedFeature_)) {
        return;
    }
    selectedFeature_ = document_.features().empty()
        ? std::optional<document::FeatureId>{}
        : document_.features().back().id;
}

} // namespace voelund::application
