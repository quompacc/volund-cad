#include <voelund/features/FeatureDependencies.hpp>
#include <voelund/document/DocumentSnapshot.hpp>
#include <voelund/document/LegacyBodyIdentity.hpp>
#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>

namespace voelund::features {
std::optional<document::FeatureId> bodyProducerBefore(const document::DocumentSnapshot& snapshot,
                                                      document::FeatureId feature,
                                                      document::BodyId body) {
    const auto features = snapshot.features();
    const auto found = std::ranges::find(features, feature, &document::FeatureDescriptor::id);
    if (found == features.end())
        return std::nullopt;
    const auto contexts = document::featureBodyContexts(features, snapshot.parameters());
    for (auto index = static_cast<std::size_t>(found - features.begin()); index-- > 0;)
        if (features[index].kind != document::FeatureKind::sketch && contexts[index] == body)
            return features[index].id;
    return std::nullopt;
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
FeatureDependencies::FeatureDependencies(std::vector<FeatureInputs> nodes) : nodes_(std::move(nodes)) {
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
        if (remaining[i] == 0) ready.insert(i);
    }
    while (!ready.empty()) {
        const auto index = *ready.begin();
        ready.erase(ready.begin());
        order_.push_back(index);
        for (const auto next : successors[index])
            if (--remaining[next] == 0) ready.insert(next);
    }
    if (order_.size() != nodes_.size()) throw std::invalid_argument("Cyclic feature dependencies");
}
FeatureDependencies FeatureDependencies::from(const document::DocumentSnapshot& snapshot) {
    std::vector<FeatureInputs> nodes;
    for (const auto& feature : snapshot.features()) {
        FeatureInputs node{feature.id, {}};
        if (feature.inputFeature) node.inputs.push_back(*feature.inputFeature);
        // T04: a face extrusion works on the bodies of its profile face (and end face) as
        // they were before it.
        if (feature.kind == document::FeatureKind::extrusion && !feature.inputFeature) {
            const auto& parameters = snapshot.extrusionParameters(feature.id);
            for (const auto& face : {parameters.faceProfile, parameters.upToFace})
                if (face)
                    if (const auto producer = bodyProducerBefore(snapshot, feature.id, face->body);
                        producer && std::ranges::find(node.inputs, *producer) == node.inputs.end())
                        node.inputs.push_back(*producer);
        }
        // M05/M07: a fillet or chamfer works on its body as it was before it.
        if (feature.kind == document::FeatureKind::fillet ||
            feature.kind == document::FeatureKind::chamfer ||
            feature.kind == document::FeatureKind::hole ||
            feature.kind == document::FeatureKind::shell) {
            const auto body = feature.kind == document::FeatureKind::fillet
                                  ? snapshot.filletParameters(feature.id).body
                              : feature.kind == document::FeatureKind::chamfer
                                  ? snapshot.chamferParameters(feature.id).body
                              : feature.kind == document::FeatureKind::shell // M22
                                  ? snapshot.shellParameters(feature.id).body
                                  : snapshot.holeParameters(feature.id).face.body;
            if (const auto producer = bodyProducerBefore(snapshot, feature.id, body))
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
            if (const auto producer = bodyProducerBefore(snapshot, feature.id, *datumBody))
                node.inputs.push_back(*producer);
        if (feature.kind == document::FeatureKind::sketch) {
            if (const auto plane = snapshot.sketchParameters(feature.id).constructionPlane)
                node.inputs.push_back(*plane);
            // S17: projected edges follow their bodies as they were before the sketch.
            for (const auto& projection : snapshot.sketchParameters(feature.id).projections)
                if (const auto producer = bodyProducerBefore(snapshot, feature.id, projection.body);
                    producer && std::ranges::find(node.inputs, *producer) == node.inputs.end())
                    node.inputs.push_back(*producer);
        }
        // M21: a combination works on its target and tool bodies as they were before it.
        if (feature.kind == document::FeatureKind::combine) {
            const auto& parameters = snapshot.combineParameters(feature.id);
            std::vector<document::BodyId> bodies{parameters.target};
            bodies.insert(bodies.end(), parameters.tools.begin(), parameters.tools.end());
            for (const auto body : bodies)
                if (const auto producer = bodyProducerBefore(snapshot, feature.id, body);
                    producer && std::ranges::find(node.inputs, *producer) == node.inputs.end())
                    node.inputs.push_back(*producer);
        }
        if (feature.kind == document::FeatureKind::extrusion && feature.inputFeature) {
            const auto features = snapshot.features();
            const auto sketch = std::ranges::find(features, *feature.inputFeature, &document::FeatureDescriptor::id);
            if (sketch == features.end()) throw std::invalid_argument("Missing extrusion sketch");
            if (sketch->inputFeature) node.inputs.push_back(*sketch->inputFeature);
            // M20: a revolve follows its construction axis.
            if (const auto& revolve = snapshot.extrusionParameters(feature.id).revolve;
                revolve && revolve->axis.datumAxis)
                node.inputs.push_back(*revolve->axis.datumAxis);
            else if (const auto producer = targetProducer(snapshot, feature.id)) node.inputs.push_back(*producer);
            // M23: a sweep follows its path sketch.
            if (const auto& sweep = snapshot.extrusionParameters(feature.id).sweep)
                node.inputs.push_back(sweep->sketch);
            // M24: a loft follows its section sketches.
            if (const auto& loft = snapshot.extrusionParameters(feature.id).loft)
                for (const auto& section : loft->sections)
                    node.inputs.push_back(section.sketch);
            // T03: the body of the end face, as it was before this extrusion.
            if (const auto face = snapshot.extrusionParameters(feature.id).upToFace)
                if (const auto producer = bodyProducerBefore(snapshot, feature.id, face->body);
                    producer && std::ranges::find(node.inputs, *producer) == node.inputs.end())
                    node.inputs.push_back(*producer);
        }
        nodes.push_back(std::move(node));
    }
    return FeatureDependencies(std::move(nodes));
}
std::vector<bool> FeatureDependencies::affected(std::span<const document::FeatureId> changed) const {
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
        for (const auto input : node.inputs) value = value || dirty[input.value()];
        result[index] = value;
        dirty[node.feature.value()] = value;
    }
    return result;
}
}
