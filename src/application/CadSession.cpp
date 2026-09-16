#include "CadSessionGeometryDetail.hpp"
#include "CommandInput.hpp"
#include <voelund/application/CadSession.hpp>

#include <voelund/document/DocumentSerializer.hpp>
#include <voelund/features/CylinderFeature.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace voelund::application {
using namespace detail;
namespace {

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
    cleanDocumentState_ = document::DocumentSnapshot::capture(document_);
}

bool CadSession::canAddCylinder() const noexcept {
    return sceneStatus().isCurrent() && !document_.features().empty();
}

TopoDS_Shape CadSession::currentShape() const {
    if (!sceneStatus().isCurrent())
        throw std::logic_error(
            "Current geometry is unavailable; retained geometry is display-only");
    return currentShape_;
}

std::optional<TopoDS_Shape> CadSession::featureShape(document::FeatureId featureId) const {
    if (!sceneStatus().isCurrent())
        return std::nullopt;
    const auto match =
        std::ranges::find(currentFeatures_, featureId, &features::EvaluatedFeature::id);
    return match == currentFeatures_.end() ? std::nullopt
                                           : std::optional<TopoDS_Shape>{match->shape};
}

std::span<const features::EvaluatedBody> CadSession::currentBodies() const {
    if (!sceneStatus().isCurrent())
        throw std::logic_error("Current bodies are unavailable; retained geometry is display-only");
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

std::vector<features::EvaluatedSketchElement> CadSession::selectedSketchElements() const {
    return features::evaluateSketchElements(selectedSketchParameters());
}

bool CadSession::canTransformFeature(document::FeatureId featureId) const noexcept {
    if (!sceneStatus().isCurrent())
        return false;
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
    if (!sceneStatus().isCurrent())
        return std::nullopt;
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

CommandResult CadSession::editSelectedBoxDimensions(const kernel::BoxDimensions& dimensions,
                                                    ErrorDisposition disposition) {
    if (!selectedFeature_)
        return CommandResult::invalidInput("Select a feature before editing parameters");
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto input = detail::prepareInput(
        [&] { return candidate.setBoxDimensions(selectedFeature_.value(), dimensions); });
    if (!input.value)
        return CommandResult::invalidInput(input.diagnostic);
    if (!*input.value)
        return {CommandStatus::rejected, RejectionReason::noChange};
    return finishCandidate(transaction, selectedFeature_, CommandKind::editFeature, disposition,
                           false);
}

CommandResult CadSession::editSelectedBoxParameters(const document::BoxParameters& parameters,
                                                    ErrorDisposition disposition) {
    if (!selectedFeature_)
        return CommandResult::invalidInput("Select a feature before editing parameters");
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto input = detail::prepareInput(
        [&] { return candidate.setBoxParameters(selectedFeature_.value(), parameters); });
    if (!input.value)
        return CommandResult::invalidInput(input.diagnostic);
    if (!*input.value)
        return {CommandStatus::rejected, RejectionReason::noChange};
    return finishCandidate(transaction, selectedFeature_, CommandKind::editFeature, disposition,
                           false);
}

const document::CylinderParameters& CadSession::selectedCylinderParameters() const {
    return cylinderParameters(selectedFeature_.value());
}

const document::CylinderParameters&
CadSession::cylinderParameters(document::FeatureId featureId) const {
    return document_.cylinderParameters(featureId);
}

CommandResult
CadSession::editSelectedCylinderParameters(const document::CylinderParameters& parameters,
                                           ErrorDisposition disposition) {
    if (!selectedFeature_)
        return CommandResult::invalidInput("Select a feature before editing parameters");
    return editCylinderParameters(selectedFeature_.value(), parameters, disposition);
}

CommandResult CadSession::editCylinderParameters(document::FeatureId featureId,
                                                 const document::CylinderParameters& parameters,
                                                 ErrorDisposition disposition) {
    const auto validation = detail::prepareInput([&] {
        document::validateCylinderParameters(parameters);
        return true;
    });
    if (!validation.value)
        return CommandResult::invalidInput(validation.diagnostic);
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto descriptors = document_.features();
    const auto descriptor =
        std::ranges::find(descriptors, featureId, &document::FeatureDescriptor::id);
    if (descriptor == descriptors.end() || descriptor->kind != document::FeatureKind::cylinder) {
        return CommandResult::invalidInput("Feature is not a cylinder");
    }
    if (!descriptor->inputFeature.has_value()) {
        throw std::logic_error("Cylinder feature has no input");
    }
    const document::CylinderParameters& current = cylinderParameters(featureId);
    document::CylinderParameters adjusted = parameters;
    const bool wasSeparate = current.resultMode == document::CylinderResultMode::separate;
    const bool becomesSeparate = adjusted.resultMode == document::CylinderResultMode::separate;
    if (wasSeparate != becomesSeparate && !sceneStatus().isCurrent()) {
        features::EvaluationReport report;
        report.features.push_back(
            {featureId,
             features::EvaluationStatus::failed,
             "stale-input",
             "Repair current geometry before changing the cylinder connection mode",
             {}});
        return {CommandStatus::rejected, RejectionReason::incompleteEvaluation, std::move(report)};
    }
    if (wasSeparate != becomesSeparate) {
        translateCylinderBetweenInputAndReference(
            adjusted, evaluatedFeature(currentFeatures_, *descriptor->inputFeature),
            becomesSeparate ? 1.0 : -1.0);
    }
    const auto input =
        detail::prepareInput([&] { return candidate.setCylinderParameters(featureId, adjusted); });
    if (!input.value)
        return CommandResult::invalidInput(input.diagnostic);
    if (!*input.value)
        return {CommandStatus::rejected, RejectionReason::noChange};
    return finishCandidate(transaction, selectedFeature_, CommandKind::editFeature, disposition,
                           false);
}

const document::ExtrusionParameters& CadSession::selectedExtrusionParameters() const {
    return document_.extrusionParameters(selectedFeature_.value());
}

CommandResult
CadSession::editSelectedExtrusionParameters(const document::ExtrusionParameters& parameters,
                                            ErrorDisposition disposition) {
    if (!selectedFeature_)
        return CommandResult::invalidInput("Select a feature before editing parameters");
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto input = detail::prepareInput(
        [&] { return candidate.setExtrusionParameters(selectedFeature_.value(), parameters); });
    if (!input.value)
        return CommandResult::invalidInput(input.diagnostic);
    if (!*input.value)
        return {CommandStatus::rejected, RejectionReason::noChange};
    return finishCandidate(transaction, selectedFeature_, CommandKind::editFeature, disposition,
                           false);
}

const kernel::RigidTransform& CadSession::selectedFeaturePlacement() const {
    const std::optional<document::FeatureId> owner =
        placementOwner(document_, selectedFeature_.value());
    if (!owner.has_value()) {
        throw std::out_of_range("Feature result has no rigid placement");
    }
    return document_.featurePlacement(*owner);
}

CommandResult CadSession::editSelectedFeaturePlacement(const kernel::RigidTransform& placement,
                                                       ErrorDisposition disposition) {
    if (!selectedFeature_)
        return CommandResult::invalidInput("Select a feature before editing parameters");
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const std::optional<document::FeatureId> owner =
        placementOwner(document_, selectedFeature_.value());
    if (!owner.has_value()) {
        return CommandResult::invalidInput("Feature result has no rigid placement");
    }
    const auto input =
        detail::prepareInput([&] { return candidate.setFeaturePlacement(*owner, placement); });
    if (!input.value)
        return CommandResult::invalidInput(input.diagnostic);
    if (!*input.value)
        return {CommandStatus::rejected, RejectionReason::noChange};
    return finishCandidate(transaction, selectedFeature_, CommandKind::editFeature, disposition,
                           false);
}

std::span<const FeatureRuntimeState> CadSession::featureStates() const noexcept {
    return featureStates_;
}

bool CadSession::canUndo() const noexcept { return document_.canUndo(); }

bool CadSession::canRedo() const noexcept { return document_.canRedo(); }

bool CadSession::isModified() const {
    return !cleanDocumentState_ || !cleanDocumentState_->matchesContent(document_);
}

void CadSession::newDocument() {
    if (generation_ == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("Document generation exhausted");
    auto empty = document::ModelDocument::empty();
    auto clean = std::optional{document::DocumentSnapshot::capture(empty)};
    const auto identity = document::IdentifiedDocumentSnapshot::restore(documentId_, *clean).duplicate().id();
    // Empty document and clean baseline are prepared before publication.
    document_ = std::move(empty);
    documentId_ = identity;
    ++generation_;
    currentFile_.clear();
    importReport_.reset();
    currentShape_.Nullify();
    currentBodies_.clear();
    currentFeatures_.clear();
    selectedFeature_.reset();
    selectedSketchElement_.reset();
    selectedSketchProfiles_.clear();
    featureStates_.clear();
    cleanDocumentState_.swap(clean);
    geometryStamp_ = sceneStatus().document;
}
bool CadSession::recompute() {
    if (document_.features().empty()) {
        currentShape_.Nullify();
        currentBodies_.clear();
        currentFeatures_.clear();
        markFeaturesCurrent();
        geometryStamp_ = sceneStatus().document;
        return true;
    }
    try {
        features::FeatureGraphResult candidate = featureGraph_.evaluate(document_);
        currentShape_ = candidate.resultShape;
        currentBodies_ = std::move(candidate.bodies);
        currentFeatures_ = std::move(candidate.features);
        markFeaturesCurrent();
        geometryStamp_ = sceneStatus().document;
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
            state.status = FeatureRuntimeStatus::blocked;
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
