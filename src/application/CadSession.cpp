// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CadSessionEvaluationDetail.hpp"
#include "CadSessionGeometryDetail.hpp"
#include "CommandInput.hpp"
#include <voelund/application/CadSession.hpp>

#include <voelund/document/DocumentSerializer.hpp>
#include <voelund/document/LegacyBodyIdentity.hpp>
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
        if (descriptor->kind != document::FeatureKind::cylinder &&
            descriptor->kind != document::FeatureKind::primitive) {
            return std::nullopt;
        }
        // M27: a primitive is placed like a cylinder.
        if ((descriptor->kind == document::FeatureKind::cylinder
                 ? document.cylinderParameters(featureId).resultMode
                 : document.primitiveParameters(featureId).resultMode) ==
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

// M01: on the selected body or as a new body, while the geometry is current.
bool CadSession::canAddCylinder() const noexcept { return sceneStatus().isCurrent(); }

std::optional<document::FeatureId> CadSession::selectedBodyProducer() const {
    if (!selectedFeature_ || !evaluation_)
        return std::nullopt;
    const auto& outputs = evaluation_->result.features;
    const auto selected =
        std::ranges::find(outputs, *selectedFeature_, &features::EvaluatedFeature::id);
    if (selected == outputs.end() || !selected->bodyId)
        return std::nullopt;
    for (const auto& body : evaluation_->result.bodies)
        if (body.bodyId == *selected->bodyId)
            return body.sourceFeature;
    return std::nullopt;
}

bool CadSession::canAddCylinder(document::FeatureId inputFeature) const noexcept {
    return featureShape(inputFeature).has_value();
}

TopoDS_Shape CadSession::currentShape() const {
    if (!sceneStatus().isCurrent())
        throw std::logic_error(
            "Current geometry is unavailable; retained geometry is display-only");
    return currentShape_;
}

std::span<const features::EvaluatedBody> CadSession::validBodies() const noexcept {
    return evaluation_ ? std::span<const features::EvaluatedBody>{evaluation_->result.bodies}
                       : std::span<const features::EvaluatedBody>{};
}
std::optional<TopoDS_Shape> CadSession::featureShape(document::FeatureId featureId) const {
    if (!evaluation_)
        return std::nullopt;
    const auto& outputs = evaluation_->result.features;
    const auto found = std::ranges::find(outputs, featureId, &features::EvaluatedFeature::id);
    if (found == outputs.end())
        return std::nullopt;
    if (found->bodyId && std::ranges::find(validBodies(), *found->bodyId,
                                           &features::EvaluatedBody::bodyId) == validBodies().end())
        return std::nullopt;
    return found->shape;
}
std::span<const features::EvaluatedBody> CadSession::currentBodies() const {
    if (!sceneStatus().isCurrent())
        throw std::logic_error("Current bodies are unavailable; retained geometry is display-only");
    return currentBodies_;
}

std::vector<document::FeatureDescriptor> CadSession::features() const {
    const auto descriptors = document_.features();
    return {descriptors.begin(), descriptors.end()};
}

ModelOutline CadSession::modelOutline() const {
    const auto snapshot = document::DocumentSnapshot::capture(document_);
    ModelOutline outline;
    std::vector<std::optional<document::BodyId>> contexts;
    try {
        contexts = document::featureBodyContexts(snapshot.features(), snapshot.parameters());
    } catch (const std::invalid_argument&) {
        contexts.assign(snapshot.features().size(), std::nullopt);
    }
    for (std::size_t index = 0; index < contexts.size(); ++index) {
        const document::FeatureId feature = snapshot.features()[index].id;
        // M19: construction features belong to no body.
        const auto kind = snapshot.features()[index].kind;
        if (!contexts[index] || document::isConstruction(kind)) {
            outline.loose.push_back(feature);
            continue;
        }
        auto body = std::ranges::find(outline.bodies, *contexts[index], &OutlineBody::id);
        if (body == outline.bodies.end())
            body = outline.bodies.insert(outline.bodies.end(), {*contexts[index], {}});
        body->features.push_back(feature);
    }
    for (auto& body : outline.bodies) {
        const auto attributes =
            document::bodyAttributes(snapshot.bodyAttributes(), body.id.value());
        body.name = attributes.name;
        body.hidden = attributes.hidden;
        body.color = attributes.color;       // N06
        body.material = attributes.material; // N07
    }
    for (std::size_t index = 0; index < contexts.size(); ++index)
        if (const auto* combine =
                std::get_if<document::CombineParameters>(&snapshot.parameters()[index]);
            combine && !combine->keepTools)
            for (auto& body : outline.bodies)
                if (std::ranges::find(combine->tools, body.id) != combine->tools.end())
                    body.consumed = true;
    return outline;
}

std::optional<document::FeatureId> CadSession::selectedFeature() const noexcept {
    return selectedFeature_;
}

void CadSession::clearSelection() noexcept {
    selectedFeature_.reset();
    selectedSketchElement_.reset();
    selectedSketchProfiles_.clear();
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

document::SketchParameters CadSession::selectedSketchParameters() const {
    return document_.sketchParameters(selectedFeature_.value());
}

document::SketchParameters CadSession::sketchParameters(document::FeatureId featureId) const {
    return document_.sketchParameters(featureId);
}

std::vector<features::EvaluatedSketchElement> CadSession::selectedSketchElements() const {
    if (!selectedFeature_ || !evaluation_)
        return {};
    const auto& outputs = evaluation_->result.features;
    const auto found =
        std::ranges::find(outputs, *selectedFeature_, &features::EvaluatedFeature::id);
    if (found == outputs.end() || !found->resolvedSketch)
        return {};
    return features::evaluateSketchElements(*found->resolvedSketch);
}

bool CadSession::canTransformFeature(document::FeatureId featureId) const noexcept {
    if (!featureShape(featureId))
        return false;
    const auto descriptors = document_.features();
    const auto feature =
        std::ranges::find(descriptors, featureId, &document::FeatureDescriptor::id);
    if (feature == descriptors.end()) {
        return false;
    }
    // M11: every body moves; sketches and construction features have none.
    if (feature->kind == document::FeatureKind::sketch || document::isConstruction(feature->kind)) {
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
    // M11: the current producer of the feature's body.
    if (!featureShape(featureId) || !evaluation_)
        return std::nullopt;
    const auto& outputs = evaluation_->result.features;
    const auto output = std::ranges::find(outputs, featureId, &features::EvaluatedFeature::id);
    if (output == outputs.end() || !output->bodyId)
        return std::nullopt;
    for (const features::EvaluatedBody& body : validBodies())
        if (body.bodyId == *output->bodyId)
            return body.sourceFeature;
    return std::nullopt;
}

document::FeatureDescriptor CadSession::selectedFeatureDescriptor() const {
    for (const document::FeatureDescriptor& feature : document_.features()) {
        if (feature.id == selectedFeature_.value()) {
            return feature;
        }
    }
    throw std::logic_error("Selected feature is not in the document");
}

kernel::BoxDimensions CadSession::selectedBoxDimensions() const {
    return document_.boxDimensions(selectedFeature_.value());
}

document::BoxParameters CadSession::selectedBoxParameters() const {
    return document_.boxParameters(selectedFeature_.value());
}

CommandResult CadSession::editSelectedBoxDimensions(const kernel::BoxDimensions& dimensions,
                                                    ErrorDisposition disposition) {
    if (!selectedFeature_)
        return CommandResult::invalidInput("Select a feature before editing parameters");
    return editBoxDimensions(selectedFeature_.value(), dimensions, disposition);
}

CommandResult CadSession::editSelectedBoxParameters(const document::BoxParameters& parameters,
                                                    ErrorDisposition disposition) {
    if (!selectedFeature_)
        return CommandResult::invalidInput("Select a feature before editing parameters");
    return editBoxParameters(selectedFeature_.value(), parameters, disposition);
}

document::CylinderParameters CadSession::selectedCylinderParameters() const {
    return cylinderParameters(selectedFeature_.value());
}

document::CylinderParameters CadSession::cylinderParameters(document::FeatureId featureId) const {
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
    const document::CylinderParameters& current = cylinderParameters(featureId);
    if (!descriptor->inputFeature.has_value() &&
        parameters.resultMode != document::CylinderResultMode::separate)
        return CommandResult::invalidInput(
            "A new-body cylinder has no body to join, cut or intersect");
    document::CylinderParameters adjusted = parameters;
    const bool wasSeparate = current.resultMode == document::CylinderResultMode::separate;
    const bool becomesSeparate = adjusted.resultMode == document::CylinderResultMode::separate;
    if (wasSeparate != becomesSeparate && descriptor->inputFeature &&
        !featureShape(*descriptor->inputFeature)) {
        features::EvaluationReport report;
        report.features.push_back(
            {featureId,
             features::EvaluationStatus::failed,
             "stale-input",
             "Repair current geometry before changing the cylinder connection mode",
             {}});
        return {CommandStatus::rejected, RejectionReason::incompleteEvaluation, std::move(report)};
    }
    if (wasSeparate != becomesSeparate && descriptor->inputFeature) {
        translateCylinderBetweenInputAndReference(
            adjusted, evaluatedFeature(evaluation_->result.features, *descriptor->inputFeature),
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

document::ExtrusionParameters CadSession::selectedExtrusionParameters() const {
    return document_.extrusionParameters(selectedFeature_.value());
}

CommandResult
CadSession::editSelectedExtrusionParameters(const document::ExtrusionParameters& parameters,
                                            ErrorDisposition disposition) {
    if (!selectedFeature_)
        return CommandResult::invalidInput("Select a feature before editing parameters");
    return editExtrusionParameters(selectedFeature_.value(), parameters, disposition);
}

document::ExtrusionParameters CadSession::extrusionParameters(document::FeatureId featureId) const {
    return document_.extrusionParameters(featureId);
}

CommandResult CadSession::editExtrusionParameters(document::FeatureId featureId,
                                                  const document::ExtrusionParameters& parameters,
                                                  ErrorDisposition disposition) {
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto input = detail::prepareInput(
        [&] { return candidate.setExtrusionParameters(featureId, parameters); });
    if (!input.value)
        return CommandResult::invalidInput(input.diagnostic);
    if (!*input.value)
        return {CommandStatus::rejected, RejectionReason::noChange};
    return finishCandidate(transaction, selectedFeature_, CommandKind::editFeature, disposition,
                           false);
}

kernel::RigidTransform CadSession::selectedFeaturePlacement() const {
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
    return editFeaturePlacement(selectedFeature_.value(), placement, disposition);
}

std::optional<document::FeatureId>
CadSession::placementFeature(document::FeatureId featureId) const {
    return placementOwner(document_, featureId);
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
    const auto identity =
        document::IdentifiedDocumentSnapshot::restore(documentId(), *clean).duplicate().id();
    empty = clean->toDocument(identity);
    // Empty document and clean baseline are prepared before publication.
    document_ = std::move(empty);
    evaluation_.reset();
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
    auto evaluation =
        featureGraph_.evaluateDetailed(document::DocumentSnapshot::capture(document_), evaluation_);
    auto states = detail::runtimeStates(evaluation->report);
    if (evaluation->valid()) {
        auto result = evaluation->result;
        currentShape_ = std::move(result.resultShape);
        currentBodies_.swap(result.bodies);
        currentFeatures_.swap(result.features);
        geometryStamp_ = sceneStatus().document;
    }
    featureStates_.swap(states);
    evaluation_ = std::move(evaluation);
    return evaluation_->valid();
}
std::string CadSession::serializedDocument() const {
    std::ostringstream output;
    document::DocumentSerializer::write(document_, output);
    return output.str();
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
