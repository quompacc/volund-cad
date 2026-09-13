#include <voelund/application/CadSession.hpp>

#include <voelund/document/DocumentSerializer.hpp>
#include <voelund/features/CylinderFeature.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace voelund::application {
namespace {

std::size_t featureCount(std::span<const document::FeatureDescriptor> features,
                         document::FeatureKind kind) {
    std::size_t count = 0;
    for (const document::FeatureDescriptor& feature : features) {
        if (feature.kind == kind) {
            ++count;
        }
    }
    return count;
}

const features::EvaluatedFeature&
evaluatedFeature(std::span<const features::EvaluatedFeature> features, document::FeatureId id) {
    const auto feature = std::ranges::find(features, id, &features::EvaluatedFeature::id);
    if (feature == features.end()) {
        throw std::logic_error("Cylinder input has not been evaluated");
    }
    return *feature;
}

void translateCylinderBetweenInputAndReference(document::CylinderParameters& parameters,
                                               const features::EvaluatedFeature& input,
                                               double direction) {
    const gp_Pnt inputOrigin =
        features::CylinderFeature::attachmentOrigin(input.shape, parameters.height);
    const gp_Pnt referenceOrigin =
        features::CylinderFeature::attachmentOrigin(input.referenceShape, parameters.height);
    parameters.placement.translationX += direction * (inputOrigin.X() - referenceOrigin.X());
    parameters.placement.translationY += direction * (inputOrigin.Y() - referenceOrigin.Y());
    parameters.placement.translationZ += direction * (inputOrigin.Z() - referenceOrigin.Z());
}

std::optional<document::FeatureId> placementOwner(const document::ModelDocument& document,
                                                  document::FeatureId featureId) {
    while (true) {
        const auto descriptors = document.features();
        const auto descriptor =
            std::ranges::find(descriptors, featureId, &document::FeatureDescriptor::id);
        if (descriptor == descriptors.end()) {
            return std::nullopt;
        }
        if (descriptor->kind == document::FeatureKind::box) {
            return featureId;
        }
        if (descriptor->kind != document::FeatureKind::cylinder) {
            return std::nullopt;
        }
        if (document.cylinderParameters(featureId).resultMode ==
            document::CylinderResultMode::separate) {
            return featureId;
        }
        if (!descriptor->inputFeature.has_value()) {
            return std::nullopt;
        }
        featureId = *descriptor->inputFeature;
    }
}

bool isFeatureKind(const document::ModelDocument& document, document::FeatureId featureId,
                   document::FeatureKind kind) noexcept {
    const auto descriptors = document.features();
    const auto descriptor =
        std::ranges::find(descriptors, featureId, &document::FeatureDescriptor::id);
    return descriptor != descriptors.end() && descriptor->kind == kind;
}

} // namespace

CadSession::CadSession() {
    if (!recompute()) {
        throw std::runtime_error("Initial feature graph evaluation failed");
    }
    cleanDocumentState_ = serializedDocument();
}

document::FeatureId CadSession::addBoxFeature() {
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const std::size_t sequence = featureCount(document_.features(), document::FeatureKind::box) + 1;
    const document::FeatureId id = candidate.addBoxFeature("Box " + std::to_string(sequence),
                                                           kernel::BoxDimensions{20.0, 20.0, 40.0});
    static_cast<void>(commitCandidate(transaction, id, true));
    return id;
}

document::FeatureId CadSession::addCylinderFeature(document::CylinderResultMode resultMode) {
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    if (!canAddCylinder()) {
        throw std::logic_error("A cylinder requires an input feature");
    }
    const std::size_t sequence =
        featureCount(document_.features(), document::FeatureKind::cylinder) + 1;
    const document::FeatureId inputFeature = document_.features().back().id;
    document::CylinderParameters parameters{
        8.0, 25.0, kernel::RigidTransform{0.0, 0.0, 0.0, 0.0, 0.0, 0.0}, resultMode};
    if (resultMode == document::CylinderResultMode::separate) {
        translateCylinderBetweenInputAndReference(
            parameters, evaluatedFeature(currentFeatures_, inputFeature), 1.0);
    }
    const document::FeatureId id = candidate.addCylinderFeature(
        "Cylinder " + std::to_string(sequence), inputFeature, parameters);
    static_cast<void>(commitCandidate(transaction, id, true));
    return id;
}

document::FeatureId CadSession::addSketchExtrusion(document::FeatureId supportFeature,
                                                   const document::SketchPlane& plane,
                                                   const document::SketchRectangle& rectangle,
                                                   double distance) {
    const document::SketchElement rectangleElement{document::SketchElementId{1}, rectangle};
    return addSketchExtrusion(
        supportFeature,
        document::SketchParameters{plane, document::SketchElements{{rectangleElement}, 2}},
        distance);
}

document::FeatureId CadSession::addSketchFeature(document::FeatureId supportFeature,
                                                 const document::SketchParameters& sketch) {
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    if (!document_.contains(supportFeature)) {
        throw std::invalid_argument("Sketch support is not in the document");
    }
    const std::size_t sequence =
        featureCount(document_.features(), document::FeatureKind::sketch) + 1;
    const document::FeatureId id =
        candidate.addSketchFeature("Sketch " + std::to_string(sequence), supportFeature, sketch);
    static_cast<void>(commitCandidate(transaction, id, true));
    return id;
}

document::FeatureId CadSession::addSketchExtrusion(document::FeatureId supportFeature,
                                                   const document::SketchParameters& sketch,
                                                   double distance) {
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    if (!document_.contains(supportFeature)) {
        throw std::invalid_argument("Sketch support is not in the document");
    }
    if (!std::isfinite(distance) || distance <= 0.0) {
        throw std::invalid_argument("Sketch extrusion distance must be positive and finite");
    }
    const std::size_t sketchSequence =
        featureCount(document_.features(), document::FeatureKind::sketch) + 1;
    const document::FeatureId sketchId = candidate.addSketchFeature(
        "Sketch " + std::to_string(sketchSequence), supportFeature, sketch);
    const std::size_t extrusionSequence =
        featureCount(document_.features(), document::FeatureKind::extrusion) + 1;
    const document::FeatureId extrusionId =
        candidate.addExtrusionFeature("Extrusion " + std::to_string(extrusionSequence), sketchId,
                                      document::ExtrusionParameters{distance, {}});
    static_cast<void>(commitCandidate(transaction, extrusionId, true));
    return extrusionId;
}

bool CadSession::canAddCylinder() const noexcept { return !document_.features().empty(); }

TopoDS_Shape CadSession::currentShape() const { return currentShape_; }

std::optional<TopoDS_Shape> CadSession::featureShape(document::FeatureId featureId) const {
    const auto match =
        std::ranges::find(currentFeatures_, featureId, &features::EvaluatedFeature::id);
    return match == currentFeatures_.end() ? std::nullopt
                                           : std::optional<TopoDS_Shape>{match->shape};
}

std::span<const features::EvaluatedBody> CadSession::currentBodies() const noexcept {
    return currentBodies_;
}

std::span<const document::FeatureDescriptor> CadSession::features() const noexcept {
    return document_.features();
}

std::optional<document::FeatureId> CadSession::selectedFeature() const noexcept {
    return selectedFeature_;
}

bool CadSession::selectFeature(document::FeatureId featureId) noexcept {
    if (!document_.contains(featureId)) {
        return false;
    }
    if (selectedFeature_ != featureId) {
        selectedSketchElement_.reset();
        selectedSketchProfiles_.clear();
    }
    selectedFeature_ = featureId;
    return true;
}

std::optional<document::SketchElementId> CadSession::selectedSketchElement() const noexcept {
    return selectedSketchElement_;
}

bool CadSession::selectSketchElement(document::SketchElementId elementId) noexcept {
    if (!selectedFeature_.has_value() ||
        !isFeatureKind(document_, *selectedFeature_, document::FeatureKind::sketch)) {
        return false;
    }
    const auto& elements = document_.sketchParameters(*selectedFeature_).elements.values;
    if (std::ranges::find(elements, elementId, &document::SketchElement::id) == elements.end()) {
        return false;
    }
    selectedSketchElement_ = elementId;
    selectedSketchProfiles_.clear();
    return true;
}

const document::SketchParameters& CadSession::selectedSketchParameters() const {
    return document_.sketchParameters(selectedFeature_.value());
}

const document::SketchParameters&
CadSession::sketchParameters(document::FeatureId featureId) const {
    return document_.sketchParameters(featureId);
}

bool CadSession::setSketchParameters(document::FeatureId featureId,
                                     const document::SketchParameters& parameters) {
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    if (!candidate.setSketchParameters(featureId, parameters)) {
        return false;
    }
    return commitCandidate(transaction, featureId, true);
}

std::vector<features::EvaluatedSketchElement> CadSession::selectedSketchElements() const {
    return features::evaluateSketchElements(selectedSketchParameters());
}

bool CadSession::canTransformFeature(document::FeatureId featureId) const noexcept {
    const auto descriptors = document_.features();
    const auto feature =
        std::ranges::find(descriptors, featureId, &document::FeatureDescriptor::id);
    if (feature == descriptors.end()) {
        return false;
    }
    if (feature->kind == document::FeatureKind::sketch ||
        feature->kind == document::FeatureKind::extrusion) {
        return false;
    }
    if (!transformBodyForFeature(featureId).has_value()) {
        return false;
    }
    for (const FeatureRuntimeState& state : featureStates_) {
        if (state.id == featureId) {
            return state.status == FeatureRuntimeStatus::current;
        }
    }
    return false;
}

std::optional<document::FeatureId>
CadSession::transformBodyForFeature(document::FeatureId featureId) const noexcept {
    const std::optional<document::FeatureId> owner = placementOwner(document_, featureId);
    if (!owner.has_value()) {
        return std::nullopt;
    }
    for (const features::EvaluatedBody& body : currentBodies_) {
        if (placementOwner(document_, body.sourceFeature) == owner) {
            return body.sourceFeature;
        }
    }
    return std::nullopt;
}

const document::FeatureDescriptor& CadSession::selectedFeatureDescriptor() const {
    for (const document::FeatureDescriptor& feature : document_.features()) {
        if (feature.id == selectedFeature_.value()) {
            return feature;
        }
    }
    throw std::logic_error("Selected feature is not in the document");
}

const kernel::BoxDimensions& CadSession::selectedBoxDimensions() const {
    return document_.boxDimensions(selectedFeature_.value());
}

const document::BoxParameters& CadSession::selectedBoxParameters() const {
    return document_.boxParameters(selectedFeature_.value());
}

bool CadSession::setSelectedBoxDimensions(const kernel::BoxDimensions& dimensions) {
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    if (!candidate.setBoxDimensions(selectedFeature_.value(), dimensions)) {
        return false;
    }
    static_cast<void>(commitCandidate(transaction, selectedFeature_));
    return true;
}

bool CadSession::setSelectedBoxParameters(const document::BoxParameters& parameters) {
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    if (!candidate.setBoxParameters(selectedFeature_.value(), parameters)) {
        return false;
    }
    static_cast<void>(commitCandidate(transaction, selectedFeature_));
    return true;
}

const document::CylinderParameters& CadSession::selectedCylinderParameters() const {
    return cylinderParameters(selectedFeature_.value());
}

const document::CylinderParameters&
CadSession::cylinderParameters(document::FeatureId featureId) const {
    return document_.cylinderParameters(featureId);
}

bool CadSession::setSelectedCylinderParameters(const document::CylinderParameters& parameters) {
    return setCylinderParameters(selectedFeature_.value(), parameters);
}

bool CadSession::setCylinderParameters(document::FeatureId featureId,
                                       const document::CylinderParameters& parameters) {
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto descriptors = document_.features();
    const auto descriptor =
        std::ranges::find(descriptors, featureId, &document::FeatureDescriptor::id);
    if (descriptor == descriptors.end() || descriptor->kind != document::FeatureKind::cylinder) {
        throw std::out_of_range("Feature is not a cylinder");
    }
    if (!descriptor->inputFeature.has_value()) {
        throw std::logic_error("Cylinder feature has no input");
    }
    const document::CylinderParameters& current = cylinderParameters(featureId);
    document::CylinderParameters adjusted = parameters;
    const bool wasSeparate = current.resultMode == document::CylinderResultMode::separate;
    const bool becomesSeparate = adjusted.resultMode == document::CylinderResultMode::separate;
    if (wasSeparate != becomesSeparate) {
        translateCylinderBetweenInputAndReference(
            adjusted, evaluatedFeature(currentFeatures_, *descriptor->inputFeature),
            becomesSeparate ? 1.0 : -1.0);
    }
    if (!candidate.setCylinderParameters(featureId, adjusted)) {
        return false;
    }
    static_cast<void>(commitCandidate(transaction, selectedFeature_));
    return true;
}

const document::ExtrusionParameters& CadSession::selectedExtrusionParameters() const {
    return document_.extrusionParameters(selectedFeature_.value());
}

bool CadSession::setSelectedExtrusionParameters(const document::ExtrusionParameters& parameters) {
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    if (!candidate.setExtrusionParameters(selectedFeature_.value(), parameters)) {
        return false;
    }
    static_cast<void>(commitCandidate(transaction, selectedFeature_));
    return true;
}

const kernel::RigidTransform& CadSession::selectedFeaturePlacement() const {
    const std::optional<document::FeatureId> owner =
        placementOwner(document_, selectedFeature_.value());
    if (!owner.has_value()) {
        throw std::out_of_range("Feature result has no rigid placement");
    }
    return document_.featurePlacement(*owner);
}

bool CadSession::setSelectedFeaturePlacement(const kernel::RigidTransform& placement) {
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const std::optional<document::FeatureId> owner =
        placementOwner(document_, selectedFeature_.value());
    if (!owner.has_value()) {
        throw std::out_of_range("Feature result has no rigid placement");
    }
    if (!candidate.setFeaturePlacement(*owner, placement)) {
        return false;
    }
    static_cast<void>(commitCandidate(transaction, selectedFeature_));
    return true;
}

std::span<const FeatureRuntimeState> CadSession::featureStates() const noexcept {
    return featureStates_;
}

bool CadSession::canUndo() const noexcept { return document_.canUndo(); }

bool CadSession::canRedo() const noexcept { return document_.canRedo(); }

bool CadSession::undo() {
    if (!document_.undo()) {
        return false;
    }
    normalizeSelection();
    static_cast<void>(recompute());
    return true;
}

bool CadSession::redo() {
    if (!document_.redo()) {
        return false;
    }
    normalizeSelection();
    static_cast<void>(recompute());
    return true;
}

bool CadSession::isModified() const { return serializedDocument() != cleanDocumentState_; }

void CadSession::newDocument() {
    currentFile_.clear();
    document_ = document::ModelDocument::empty();
    currentShape_.Nullify();
    currentBodies_.clear();
    currentFeatures_.clear();
    selectedFeature_.reset();
    selectedSketchElement_.reset();
    selectedSketchProfiles_.clear();
    markFeaturesCurrent();
    cleanDocumentState_ = serializedDocument();
}

bool CadSession::recompute() {
    if (document_.features().empty()) {
        currentShape_.Nullify();
        currentBodies_.clear();
        currentFeatures_.clear();
        markFeaturesCurrent();
        return true;
    }
    try {
        features::FeatureGraphResult candidate = featureGraph_.evaluate(document_);
        currentShape_ = candidate.resultShape;
        currentBodies_ = std::move(candidate.bodies);
        currentFeatures_ = std::move(candidate.features);
        markFeaturesCurrent();
        return true;
    } catch (const features::FeatureEvaluationError& error) {
        markFeaturesCurrent();
        markFeatureError(error.featureId(), error.what());
        return false;
    }
}

std::string CadSession::serializedDocument() const {
    std::ostringstream output;
    document::DocumentSerializer::write(document_, output);
    return output.str();
}

void CadSession::markFeaturesCurrent() {
    featureStates_.clear();
    featureStates_.reserve(document_.features().size());
    for (const document::FeatureDescriptor& feature : document_.features()) {
        featureStates_.push_back(
            FeatureRuntimeState{feature.id, FeatureRuntimeStatus::current, {}});
    }
}

void CadSession::markFeatureError(document::FeatureId featureId, const std::string& message) {
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

void CadSession::normalizeSelection() noexcept {
    if (selectedFeature_.has_value() && document_.contains(*selectedFeature_)) {
        if (!selectedSketchElement_.has_value()) {
            return;
        }
        if (isFeatureKind(document_, *selectedFeature_, document::FeatureKind::sketch)) {
            const auto& elements = document_.sketchParameters(*selectedFeature_).elements.values;
            if (std::ranges::find(elements, *selectedSketchElement_,
                                  &document::SketchElement::id) != elements.end()) {
                return;
            }
        }
        selectedSketchElement_.reset();
        selectedSketchProfiles_.clear();
        return;
    }
    selectedSketchElement_.reset();
    selectedSketchProfiles_.clear();
    selectedFeature_ = document_.features().empty() ? std::optional<document::FeatureId>{}
                                                    : document_.features().back().id;
}

} // namespace voelund::application
