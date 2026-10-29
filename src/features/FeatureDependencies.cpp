// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <voelund/document/DocumentSnapshot.hpp>
#include <voelund/document/LegacyBodyIdentity.hpp>
#include <voelund/features/FeatureDependencies.hpp>

namespace voelund::features {
std::optional<document::FeatureId> bodyProducerBefore(const document::DocumentSnapshot& snapshot,
                                                      document::FeatureId feature,
                                                      document::BodyId body) {
    const auto features = snapshot.features();
    const auto found = std::ranges::find(features, feature, &document::FeatureDescriptor::id);
    if (found == features.end())
        return std::nullopt;
    const auto contexts = document::featureBodyContexts(features, snapshot.parameters());
    // M14: a suppressed feature passes the body on as it got it.
    for (auto index = static_cast<std::size_t>(found - features.begin()); index-- > 0;)
        if (features[index].kind != document::FeatureKind::sketch && contexts[index] == body &&
            !features[index].suppressed)
            return features[index].id;
    return std::nullopt;
}
std::optional<document::FeatureId> effectiveInput(const document::DocumentSnapshot& snapshot,
                                                  document::FeatureId feature) {
    const auto features = snapshot.features();
    const auto found = std::ranges::find(features, feature, &document::FeatureDescriptor::id);
    if (found == features.end() || !found->inputFeature)
        return std::nullopt;
    const auto input =
        std::ranges::find(features, *found->inputFeature, &document::FeatureDescriptor::id);
    if (input == features.end() || !input->suppressed)
        return found->inputFeature;
    const auto contexts = document::featureBodyContexts(features, snapshot.parameters());
    const auto body = contexts[static_cast<std::size_t>(input - features.begin())];
    if (!body || input->kind == document::FeatureKind::sketch)
        return found->inputFeature;
    if (const auto before = bodyProducerBefore(snapshot, input->id, *body))
        return before;
    return found->inputFeature;
}
std::optional<document::FeatureId> targetProducer(const document::DocumentSnapshot& snapshot,
                                                  document::FeatureId extrusion) {
    const auto features = snapshot.features();
    const auto found = std::ranges::find(features, extrusion, &document::FeatureDescriptor::id);
    if (found == features.end() || found->kind != document::FeatureKind::extrusion)
        return std::nullopt;
    const auto target = snapshot.extrusionParameters(extrusion).target;
    return target ? bodyProducerBefore(snapshot, extrusion, *target) : std::nullopt;
}
FeatureDependencies::FeatureDependencies(std::vector<FeatureInputs> nodes)
    : nodes_(std::move(nodes)) {
    std::map<std::uint64_t, std::size_t> indices;
    for (std::size_t i = 0; i < nodes_.size(); ++i)
        if (nodes_[i].feature.value() == 0 || !indices.emplace(nodes_[i].feature.value(), i).second)
            throw std::invalid_argument("Dependency graph has zero or duplicate feature ID");
    std::vector<std::vector<std::size_t>> successors(nodes_.size());
    std::vector<std::size_t> remaining(nodes_.size());
    std::set<std::size_t> ready;
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        std::set<std::uint64_t> unique;
        for (const auto input : nodes_[i].inputs) {
            const auto found = indices.find(input.value());
            if (found == indices.end() || !unique.insert(input.value()).second)
                throw std::invalid_argument("Dependency graph has missing or duplicate input");
            successors[found->second].push_back(i);
            ++remaining[i];
        }
        if (remaining[i] == 0)
            ready.insert(i);
    }
    while (!ready.empty()) {
        const auto index = *ready.begin();
        ready.erase(ready.begin());
        order_.push_back(index);
        for (const auto next : successors[index])
            if (--remaining[next] == 0)
                ready.insert(next);
    }
    if (order_.size() != nodes_.size())
        throw std::invalid_argument("Cyclic feature dependencies");
}
namespace {
// M14: the producer a feature depends on; when only suppressed features made the body (its
// creator is suppressed), that one, so the feature is blocked rather than failing.
std::optional<document::FeatureId> inputProducer(const document::DocumentSnapshot& snapshot,
                                                 document::FeatureId feature,
                                                 document::BodyId body) {
    if (const auto producer = bodyProducerBefore(snapshot, feature, body))
        return producer;
    const auto features = snapshot.features();
    const auto found = std::ranges::find(features, feature, &document::FeatureDescriptor::id);
    const auto contexts = document::featureBodyContexts(features, snapshot.parameters());
    for (auto index = static_cast<std::size_t>(found - features.begin()); index-- > 0;)
        if (features[index].kind != document::FeatureKind::sketch && contexts[index] == body &&
            features[index].suppressed)
            return features[index].id;
    return std::nullopt;
}
// The producers of `bodies` as they were before the node's feature, each once.
void addProducers(FeatureInputs& node, const document::DocumentSnapshot& snapshot,
                  std::span<const document::BodyId> bodies) {
    for (const auto body : bodies)
        if (const auto producer = inputProducer(snapshot, node.feature, body);
            producer && std::ranges::find(node.inputs, *producer) == node.inputs.end())
            node.inputs.push_back(*producer);
}

// M09/N12a: a mirror or split reads its body, and the body of its face or its construction
// plane, as they were before it.
void addPlaneUser(FeatureInputs& node, const document::DocumentSnapshot& snapshot,
                  document::BodyId body, const document::MirrorPlane& plane) {
    std::vector<document::BodyId> bodies{body};
    if (plane.face)
        bodies.push_back(plane.face->body);
    addProducers(node, snapshot, bodies);
    if (plane.constructionPlane)
        node.inputs.push_back(*plane.constructionPlane);
}

// M10: a pattern reads its body, the body of its edge or its construction axis as they were
// before it; N11a: and its rows' axis; N11c: its path sketch.
void addPattern(FeatureInputs& node, const document::DocumentSnapshot& snapshot) {
    const auto& parameters = snapshot.patternParameters(node.feature);
    std::vector<document::BodyId> bodies{parameters.body};
    for (const auto* axis :
         {&parameters.axis, parameters.second ? &parameters.second->axis : nullptr})
        if (axis != nullptr && axis->body)
            bodies.push_back(*axis->body);
    addProducers(node, snapshot, bodies);
    for (const auto& datumAxis :
         {parameters.datumAxis,
          parameters.second ? parameters.second->datumAxis : std::optional<document::FeatureId>{}})
        if (datumAxis)
            node.inputs.push_back(*datumAxis);
    if (parameters.path)
        node.inputs.push_back(parameters.path->sketch);
}
} // namespace
FeatureDependencies FeatureDependencies::from(const document::DocumentSnapshot& snapshot) {
    std::vector<FeatureInputs> nodes;
    for (const auto& feature : snapshot.features()) {
        FeatureInputs node{feature.id, {}};
        if (feature.inputFeature)
            node.inputs.push_back(*effectiveInput(snapshot, feature.id));
        // T04: a face extrusion works on the bodies of its profile face (and end face) as
        // they were before it.
        if (feature.kind == document::FeatureKind::extrusion && !feature.inputFeature) {
            const auto& parameters = snapshot.extrusionParameters(feature.id);
            for (const auto& face : {parameters.faceProfile, parameters.upToFace})
                if (face)
                    if (const auto producer = inputProducer(snapshot, feature.id, face->body);
                        producer && std::ranges::find(node.inputs, *producer) == node.inputs.end())
                        node.inputs.push_back(*producer);
        }
        // M05/M07: a fillet or chamfer works on its body as it was before it.
        if (feature.kind == document::FeatureKind::fillet ||
            feature.kind == document::FeatureKind::chamfer ||
            feature.kind == document::FeatureKind::hole ||
            feature.kind == document::FeatureKind::shell ||
            feature.kind == document::FeatureKind::move) {
            const auto body = feature.kind == document::FeatureKind::fillet
                                  ? snapshot.filletParameters(feature.id).body
                              : feature.kind == document::FeatureKind::chamfer
                                  ? snapshot.chamferParameters(feature.id).body
                              : feature.kind == document::FeatureKind::shell // M22
                                  ? snapshot.shellParameters(feature.id).body
                                  : feature.kind == document::FeatureKind::move // M11
                                        ? snapshot.moveParameters(feature.id).body
                                        : snapshot.holeParameters(feature.id).face.body;
            if (const auto producer = inputProducer(snapshot, feature.id, body))
                node.inputs.push_back(*producer);
        }
        // M19: a construction plane or axis on a body reads it as it was before; a sketch
        // on a construction plane follows the plane.
        std::optional<document::BodyId> datumBody;
        if (feature.kind == document::FeatureKind::datumPlane &&
            snapshot.datumPlaneParameters(feature.id).face)
            datumBody = snapshot.datumPlaneParameters(feature.id).face->body;
        if (feature.kind == document::FeatureKind::datumAxis)
            datumBody = snapshot.datumAxisParameters(feature.id).body;
        if (datumBody)
            if (const auto producer = inputProducer(snapshot, feature.id, *datumBody))
                node.inputs.push_back(*producer);
        if (feature.kind == document::FeatureKind::sketch) {
            if (const auto plane = snapshot.sketchParameters(feature.id).constructionPlane)
                node.inputs.push_back(*plane);
            // S17: projected edges follow their bodies as they were before the sketch.
            for (const auto& projection : snapshot.sketchParameters(feature.id).projections)
                if (const auto producer = inputProducer(snapshot, feature.id, projection.body);
                    producer && std::ranges::find(node.inputs, *producer) == node.inputs.end())
                    node.inputs.push_back(*producer);
        }
        if (const auto* mirror =
                std::get_if<document::MirrorParameters>(&snapshot.featureParameters(feature.id)))
            addPlaneUser(node, snapshot, mirror->body, mirror->plane);
        if (const auto* split = std::get_if<document::SplitParameters>(
                &snapshot.featureParameters(feature.id))) // N12a
            addPlaneUser(node, snapshot, split->body, split->plane);
        if (const auto* edit = std::get_if<document::FaceEditParameters>(
                &snapshot.featureParameters(feature.id))) // N13a: its body before it
            addProducers(node, snapshot, std::span(&edit->body, 1));
        if (feature.kind == document::FeatureKind::pattern)
            addPattern(node, snapshot);
        // M10/M17/N12a: a pattern copy, import part or split part follows its owner.
        if (const auto owner = document::companionOwner(snapshot.featureParameters(feature.id)))
            node.inputs.push_back(*owner);
        // M21: a combination works on its target and tool bodies as they were before it.
        if (feature.kind == document::FeatureKind::combine) {
            const auto& parameters = snapshot.combineParameters(feature.id);
            std::vector<document::BodyId> bodies{parameters.target};
            bodies.insert(bodies.end(), parameters.tools.begin(), parameters.tools.end());
            for (const auto body : bodies)
                if (const auto producer = inputProducer(snapshot, feature.id, body);
                    producer && std::ranges::find(node.inputs, *producer) == node.inputs.end())
                    node.inputs.push_back(*producer);
        }
        if (feature.kind == document::FeatureKind::extrusion && feature.inputFeature) {
            const auto features = snapshot.features();
            const auto sketch = std::ranges::find(features, *feature.inputFeature,
                                                  &document::FeatureDescriptor::id);
            if (sketch == features.end())
                throw std::invalid_argument("Missing extrusion sketch");
            if (sketch->inputFeature) {
                const auto support = *effectiveInput(snapshot, sketch->id);
                if (std::ranges::find(node.inputs, support) == node.inputs.end())
                    node.inputs.push_back(support);
            }
            // M20: a revolve follows its construction axis.
            if (const auto& revolve = snapshot.extrusionParameters(feature.id).revolve;
                revolve && revolve->axis.datumAxis)
                node.inputs.push_back(*revolve->axis.datumAxis);
            else if (const auto producer = targetProducer(snapshot, feature.id))
                node.inputs.push_back(*producer);
            // M23: a sweep follows its path sketch.
            if (const auto& sweep = snapshot.extrusionParameters(feature.id).sweep)
                node.inputs.push_back(sweep->sketch);
            // M24: a loft follows its section sketches.
            if (const auto& loft = snapshot.extrusionParameters(feature.id).loft)
                for (const auto& section : loft->sections)
                    node.inputs.push_back(section.sketch);
            // T03: the body of the end face, as it was before this extrusion.
            if (const auto face = snapshot.extrusionParameters(feature.id).upToFace)
                if (const auto producer = inputProducer(snapshot, feature.id, face->body);
                    producer && std::ranges::find(node.inputs, *producer) == node.inputs.end())
                    node.inputs.push_back(*producer);
        }
        nodes.push_back(std::move(node));
    }
    return FeatureDependencies(std::move(nodes));
}
std::vector<bool>
FeatureDependencies::affected(std::span<const document::FeatureId> changed) const {
    std::map<std::uint64_t, bool> dirty;
    for (const auto id : changed) {
        if (std::ranges::find(nodes_, id, &FeatureInputs::feature) == nodes_.end())
            throw std::invalid_argument("Changed feature is absent from dependency graph");
        dirty[id.value()] = true;
    }
    std::vector<bool> result(nodes_.size());
    for (const auto index : order_) {
        const auto& node = nodes_[index];
        bool value = dirty[node.feature.value()];
        for (const auto input : node.inputs)
            value = value || dirty[input.value()];
        result[index] = value;
        dirty[node.feature.value()] = value;
    }
    return result;
}
} // namespace voelund::features
