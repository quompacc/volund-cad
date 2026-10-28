// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "ModelDocumentImpl.hpp"
#include <voelund/document/LegacyBodyIdentity.hpp>

#include <algorithm>
#include <iterator>
#include <set>
#include <stdexcept>
#include <utility>

// M16 (user decision 2026-10-16): reorder and delete features. Body inputs follow explicitly
// (stored, never guessed during evaluation); a feature never stands before what it needs.
namespace voelund::document {
namespace {

void addOrigin(std::vector<FeatureId>& ids, const SubshapeOrigin& origin) {
    ids.push_back(origin.feature);
    for (const auto& instance : origin.instances)
        ids.push_back(instance.feature);
}
void addEdge(std::vector<FeatureId>& ids, const FilletEdge& edge) {
    addOrigin(ids, edge.first);
    addOrigin(ids, edge.second);
}
// M10: a pattern's direction: a construction axis, or a body and its edge.
void addAxisReferences(FeatureReferences& references, const DatumAxisParameters& axis,
                       const std::optional<FeatureId>& datumAxis) {
    if (datumAxis)
        references.structural.push_back(*datumAxis);
    if (axis.body)
        references.structural.push_back(FeatureId{axis.body->value()});
    if (axis.edge)
        addEdge(references.geometric, *axis.edge);
}

void addFace(FeatureReferences& references, const BodyFaceReference& face) {
    references.structural.push_back(FeatureId{face.body.value()});
    addOrigin(references.geometric, face.origin);
}

std::size_t indexOf(std::span<const FeatureDescriptor> features, FeatureId id) {
    const auto found = std::ranges::find(features, id, &FeatureDescriptor::id);
    if (found == features.end())
        throw std::invalid_argument("Unknown feature");
    return static_cast<std::size_t>(found - features.begin());
}

// The latest feature before `index` that makes body `body` (sketches make none).
std::optional<FeatureId> producerBefore(std::span<const FeatureDescriptor> features,
                                        std::span<const std::optional<BodyId>> contexts,
                                        std::size_t index, BodyId body) {
    for (auto earlier = index; earlier-- > 0;)
        if (contexts[earlier] == body && features[earlier].kind != FeatureKind::sketch)
            return features[earlier].id;
    return std::nullopt;
}

// Body inputs of the features `rewire` names take the body as it stands right before them.
template <class Rewire>
void rewireBodyInputs(std::vector<FeatureDescriptor>& features,
                      const std::vector<FeatureParameters>& parameters,
                      std::span<const std::optional<BodyId>> inputBodies, Rewire rewire) {
    const auto contexts = featureBodyContexts(features, parameters);
    for (std::size_t index = 0; index < features.size(); ++index) {
        const auto body = inputBodies[index];
        if (!body || !rewire(index))
            continue;
        if (const auto producer = producerBefore(features, contexts, index, *body))
            features[index].inputFeature = producer;
    }
}

// The body each feature's input works on, or none (no input, or a sketch input).
std::vector<std::optional<BodyId>> inputBodies(std::span<const FeatureDescriptor> features,
                                               std::span<const FeatureParameters> parameters) {
    const auto contexts = featureBodyContexts(features, parameters);
    std::vector<std::optional<BodyId>> result(features.size());
    for (std::size_t index = 0; index < features.size(); ++index) {
        const auto& input = features[index].inputFeature;
        if (!input)
            continue;
        const auto found = std::ranges::find(features, *input, &FeatureDescriptor::id);
        if (found == features.end() || found->kind == FeatureKind::sketch)
            continue;
        result[index] = contexts[static_cast<std::size_t>(found - features.begin())];
    }
    return result;
}

// Every feature stands after its input and after everything it needs.
void requireNeedsEarlier(std::span<const FeatureDescriptor> features,
                         std::span<const FeatureParameters> parameters) {
    for (std::size_t index = 0; index < features.size(); ++index) {
        const auto references = featureReferences(features[index], parameters[index]);
        for (const auto id : references.structural) {
            const auto found = std::ranges::find(features, id, &FeatureDescriptor::id);
            if (found == features.end())
                throw std::invalid_argument("A feature needs a feature that is gone");
            if (static_cast<std::size_t>(found - features.begin()) >= index &&
                id != features[index].id)
                throw std::invalid_argument("A feature cannot stand before what it needs");
        }
    }
}

} // namespace

FeatureReferences featureReferences(const FeatureDescriptor& feature,
                                    const FeatureParameters& parameters) {
    FeatureReferences references;
    if (feature.inputFeature)
        references.structural.push_back(*feature.inputFeature);
    auto& structural = references.structural;
    auto& geometric = references.geometric;
    const auto body = [&](BodyId id) { structural.push_back(FeatureId{id.value()}); };
    std::visit(
        [&](const auto& values) {
            using Type = std::decay_t<decltype(values)>;
            if constexpr (std::is_same_v<Type, SketchParameters>) {
                if (values.constructionPlane)
                    structural.push_back(*values.constructionPlane);
                if (values.supportBinding) {
                    body(values.supportBinding->reference.body);
                    addOrigin(geometric, values.supportBinding->reference.origin);
                }
                for (const auto& projection : values.projections) {
                    body(projection.body);
                    addEdge(geometric, projection.edge);
                }
            } else if constexpr (std::is_same_v<Type, ExtrusionParameters>) {
                if (values.target)
                    body(*values.target);
                if (values.upToFace)
                    addFace(references, *values.upToFace);
                if (values.faceProfile)
                    addFace(references, *values.faceProfile);
                if (values.revolve && values.revolve->axis.datumAxis)
                    structural.push_back(*values.revolve->axis.datumAxis);
                if (values.sweep)
                    structural.push_back(values.sweep->sketch);
                if (values.loft)
                    for (const auto& section : values.loft->sections)
                        structural.push_back(section.sketch);
            } else if constexpr (std::is_same_v<Type, FilletParameters> ||
                                 std::is_same_v<Type, ChamferParameters>) {
                body(values.body);
                for (const auto& edge : values.edges)
                    addEdge(geometric, edge);
            } else if constexpr (std::is_same_v<Type, HoleParameters>) {
                addFace(references, values.face);
            } else if constexpr (std::is_same_v<Type, CombineParameters>) {
                body(values.target);
                for (const auto tool : values.tools)
                    body(tool);
            } else if constexpr (std::is_same_v<Type, DatumPlaneParameters>) {
                if (values.face)
                    addFace(references, *values.face);
            } else if constexpr (std::is_same_v<Type, DatumAxisParameters>) {
                if (values.body)
                    body(*values.body);
                if (values.edge)
                    addEdge(geometric, *values.edge);
            } else if constexpr (std::is_same_v<Type, ShellParameters>) {
                body(values.body);
                for (const auto& face : values.openFaces)
                    addOrigin(geometric, face);
            } else if constexpr (std::is_same_v<Type, DraftParameters>) {
                body(values.body);
                for (const auto& face : values.faces)
                    addOrigin(geometric, face);
                addOrigin(geometric, values.neutral);
            } else if constexpr (std::is_same_v<Type, MirrorParameters>) {
                body(values.body);
                if (values.plane.constructionPlane)
                    structural.push_back(*values.plane.constructionPlane);
                if (values.plane.face)
                    addFace(references, *values.plane.face);
            } else if constexpr (std::is_same_v<Type, PatternParameters>) {
                body(values.body);
                addAxisReferences(references, values.axis, values.datumAxis);
                if (values.second) // N11a
                    addAxisReferences(references, values.second->axis, values.second->datumAxis);
            } else if constexpr (std::is_same_v<Type, PatternCopyParameters>) {
                structural.push_back(values.pattern);
            } else if constexpr (std::is_same_v<Type, ImportPartParameters>) {
                structural.push_back(values.import);
            } else if constexpr (std::is_same_v<Type, MoveParameters>) {
                body(values.body);
            }
        },
        parameters);
    // A feature that names its own body (a body creator modified later) needs nothing for it.
    std::erase(structural, feature.id);
    std::erase(geometric, feature.id);
    return references;
}

bool ModelDocument::moveFeature(FeatureId featureId, std::optional<FeatureId> after) {
    auto candidate = currentState();
    const auto from = indexOf(candidate.features, featureId);
    if (isCompanion(candidate.features[from].kind))
        throw std::invalid_argument("A pattern copy or import part moves with its feature");
    // The feature and its companions (a pattern's copies, an import's parts) move as one block.
    std::set<std::uint64_t> block{featureId.value()};
    for (std::size_t index = 0; index < candidate.parameters.size(); ++index)
        if (companionOwner(candidate.parameters[index]) == featureId)
            block.insert(candidate.features[index].id.value());
    if (after && block.contains(after->value()))
        throw std::invalid_argument("A feature cannot move after itself");
    const auto bodies = inputBodies(candidate.features, candidate.parameters);
    std::vector<FeatureDescriptor> moved;
    std::vector<FeatureParameters> movedParameters;
    std::vector<std::optional<BodyId>> movedBodies;
    std::vector<FeatureDescriptor> kept;
    std::vector<FeatureParameters> keptParameters;
    std::vector<std::optional<BodyId>> keptBodies;
    for (std::size_t index = 0; index < candidate.features.size(); ++index) {
        const bool inBlock = block.contains(candidate.features[index].id.value());
        (inBlock ? moved : kept).push_back(std::move(candidate.features[index]));
        (inBlock ? movedParameters : keptParameters)
            .push_back(std::move(candidate.parameters[index]));
        (inBlock ? movedBodies : keptBodies).push_back(bodies[index]);
    }
    const std::size_t at = after ? indexOf(kept, *after) + 1 : 0;
    kept.insert(kept.begin() + static_cast<std::ptrdiff_t>(at), moved.begin(), moved.end());
    keptParameters.insert(keptParameters.begin() + static_cast<std::ptrdiff_t>(at),
                          std::make_move_iterator(movedParameters.begin()),
                          std::make_move_iterator(movedParameters.end()));
    keptBodies.insert(keptBodies.begin() + static_cast<std::ptrdiff_t>(at), movedBodies.begin(),
                      movedBodies.end());
    if (std::ranges::equal(kept, impl_->view().features()))
        return false;
    // The moved feature takes its body where it now stands; a feature whose input moved, or
    // past which the moved feature now changes the same body, takes the body before it.
    const auto contexts = featureBodyContexts(kept, keptParameters);
    const auto movedContext = contexts[at];
    rewireBodyInputs(kept, keptParameters, keptBodies, [&](std::size_t index) {
        if (block.contains(kept[index].id.value()))
            return true;
        const auto input = kept[index].inputFeature;
        if (!input)
            return false;
        const auto inputIndex = indexOf(kept, *input);
        return inputIndex > index ||
               (inputIndex < at && at < index && movedContext == keptBodies[index]) ||
               block.contains(input->value());
    });
    requireNeedsEarlier(kept, keptParameters);
    candidate.features = std::move(kept);
    candidate.parameters = std::move(keptParameters);
    static_cast<void>(featureBodyContexts(candidate.features, candidate.parameters));
    commitState(std::move(candidate));
    return true;
}

bool ModelDocument::deleteFeatures(std::span<const FeatureId> featureIds) {
    if (featureIds.empty())
        return false;
    auto candidate = currentState();
    std::set<std::uint64_t> removed;
    for (const auto id : featureIds)
        removed.insert(candidate.features[indexOf(candidate.features, id)].id.value());
    // A pattern's copies and an import's parts go with it.
    for (std::size_t index = 0; index < candidate.parameters.size(); ++index)
        if (const auto owner = companionOwner(candidate.parameters[index]);
            owner && removed.contains(owner->value()))
            removed.insert(candidate.features[index].id.value());
    const auto bodies = inputBodies(candidate.features, candidate.parameters);
    std::vector<FeatureDescriptor> kept;
    std::vector<FeatureParameters> keptParameters;
    std::vector<std::optional<BodyId>> keptBodies;
    for (std::size_t index = 0; index < candidate.features.size(); ++index) {
        if (removed.contains(candidate.features[index].id.value()))
            continue;
        kept.push_back(std::move(candidate.features[index]));
        keptParameters.push_back(std::move(candidate.parameters[index]));
        keptBodies.push_back(bodies[index]);
    }
    // A feature whose body input is gone takes the body as it now stands before it.
    std::vector<bool> lostInput(kept.size(), false);
    for (std::size_t index = 0; index < kept.size(); ++index)
        lostInput[index] = kept[index].inputFeature &&
                           removed.contains(kept[index].inputFeature->value()) && keptBodies[index];
    // In order, so the features before each one already form a valid list.
    for (std::size_t index = 0; index < kept.size(); ++index) {
        if (!lostInput[index])
            continue;
        const auto contexts = featureBodyContexts(std::span{kept}.first(index),
                                                  std::span{keptParameters}.first(index));
        const auto producer = producerBefore(kept, contexts, index, *keptBodies[index]);
        if (!producer)
            throw std::invalid_argument("A feature needs a feature that is gone");
        kept[index].inputFeature = producer;
    }
    requireNeedsEarlier(kept, keptParameters);
    if (candidate.rollbackAfter && removed.contains(candidate.rollbackAfter->value()))
        candidate.rollbackAfter.reset();
    candidate.features = std::move(kept);
    candidate.parameters = std::move(keptParameters);
    static_cast<void>(featureBodyContexts(candidate.features, candidate.parameters));
    // Names and visibility of bodies that are gone go too.
    BodyAttributeTable attributes;
    const auto contexts = featureBodyContexts(candidate.features, candidate.parameters);
    for (const auto& entry : candidate.bodyAttributes)
        if (std::ranges::find(contexts, std::optional{BodyId{entry.body}}) != contexts.end())
            setBodyAttributes(attributes, entry.body, entry.attributes);
    candidate.bodyAttributes = std::move(attributes);
    commitState(std::move(candidate));
    return true;
}

} // namespace voelund::document
