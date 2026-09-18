#include <voelund/features/FeatureDependencies.hpp>
#include <voelund/document/DocumentSnapshot.hpp>
#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>

namespace voelund::features {
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
        if (feature.kind == document::FeatureKind::extrusion && feature.inputFeature) {
            const auto features = snapshot.features();
            const auto sketch = std::ranges::find(features, *feature.inputFeature, &document::FeatureDescriptor::id);
            if (sketch == features.end()) throw std::invalid_argument("Missing extrusion sketch");
            if (sketch->inputFeature) node.inputs.push_back(*sketch->inputFeature);
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
