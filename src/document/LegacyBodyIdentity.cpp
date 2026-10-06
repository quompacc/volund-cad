#include <voelund/document/LegacyBodyIdentity.hpp>

#include <map>
#include <stdexcept>

namespace voelund::document {
namespace {

void validateKind(const FeatureDescriptor& feature, const FeatureParameters& parameters) {
    bool matches = false;
    switch (feature.kind) {
    case FeatureKind::box:
        matches = std::holds_alternative<BoxParameters>(parameters);
        break;
    case FeatureKind::cylinder:
        matches = std::holds_alternative<CylinderParameters>(parameters);
        if (matches)
            switch (std::get<CylinderParameters>(parameters).resultMode) {
            case CylinderResultMode::join:
            case CylinderResultMode::cut:
            case CylinderResultMode::intersect:
            case CylinderResultMode::separate:
                break;
            default:
                matches = false;
            }
        break;
    case FeatureKind::sketch:
        matches = std::holds_alternative<SketchParameters>(parameters);
        break;
    case FeatureKind::extrusion:
        matches = std::holds_alternative<ExtrusionParameters>(parameters);
        break;
    }
    if (!matches)
        throw std::invalid_argument("Legacy body identity: feature parameter kind mismatch");
    // Boxes and base-plane (datum) sketches stand alone; every other feature has an input.
    const bool standalone =
        feature.kind == FeatureKind::box ||
        (feature.kind == FeatureKind::sketch && std::get<SketchParameters>(parameters).datum);
    if (standalone == feature.inputFeature.has_value())
        throw std::invalid_argument("Legacy body identity: invalid input presence");
}

} // namespace

std::vector<std::optional<BodyId>>
featureBodyContexts(std::span<const FeatureDescriptor> features,
                    std::span<const FeatureParameters> parameters) {
    if (features.size() != parameters.size())
        throw std::invalid_argument("Legacy body identity: parameter count mismatch");
    std::map<std::uint64_t, std::size_t> indices;
    for (std::size_t index = 0; index < features.size(); ++index) {
        const auto& feature = features[index];
        if (!hasIdCapacity(feature.id.value(), 1) ||
            !indices.emplace(feature.id.value(), index).second)
            throw std::invalid_argument("Legacy body identity: invalid or duplicate feature ID");
        validateKind(feature, parameters[index]);
    }
    std::vector<std::optional<std::size_t>> inputs(features.size());
    for (std::size_t index = 0; index < features.size(); ++index) {
        if (!features[index].inputFeature)
            continue;
        const auto input = indices.find(features[index].inputFeature->value());
        if (input == indices.end())
            throw std::invalid_argument("Legacy body identity: missing input feature");
        inputs[index] = input->second;
        const bool sketchInput = features[input->second].kind == FeatureKind::sketch;
        if ((features[index].kind == FeatureKind::extrusion) != sketchInput)
            throw std::invalid_argument("Legacy body identity: incompatible input kind");
    }
    const auto extrusion = [&](std::size_t index) -> const ExtrusionParameters* {
        return features[index].kind == FeatureKind::extrusion
                   ? &std::get<ExtrusionParameters>(parameters[index])
                   : nullptr;
    };
    // A base-plane sketch opens a body; T02: so does a new-body prism of a face sketch.
    const auto createsBody = [&](std::size_t index) {
        const auto& feature = features[index];
        if (const auto* prism = extrusion(index))
            return inputs[*inputs[index]].has_value() &&
                   effectiveOperation(*prism, false) == ExtrusionOperation::newBody;
        return feature.kind == FeatureKind::box ||
               (feature.kind == FeatureKind::sketch && !inputs[index]) ||
               (feature.kind == FeatureKind::cylinder &&
                std::get<CylinderParameters>(parameters[index]).resultMode ==
                    CylinderResultMode::separate);
    };
    // T02: a join or cut of a base-plane sketch names a body that some feature creates.
    for (std::size_t index = 0; index < features.size(); ++index) {
        const auto* prism = extrusion(index);
        if (prism == nullptr || !prism->target)
            continue;
        const auto creator = indices.find(prism->target->value());
        if (inputs[*inputs[index]] || creator == indices.end() || !createsBody(creator->second))
            throw std::invalid_argument("Extrusion target is not a body of this document");
    }
    // Iterative traversal also handles long legacy chains without recursive stack growth.
    std::vector<unsigned char> state(features.size(), 0);
    std::vector<std::optional<BodyId>> context(features.size());
    std::vector<std::size_t> path;
    for (std::size_t start = 0; start < features.size(); ++start) {
        auto current = start;
        while (state[current] != 2) {
            if (state[current] == 1)
                throw std::invalid_argument("Legacy body identity: dependency cycle");
            state[current] = 1;
            path.push_back(current);
            if (!inputs[current])
                break;
            current = *inputs[current];
        }
        while (!path.empty()) {
            const auto index = path.back();
            path.pop_back();
            const auto& feature = features[index];
            // A datum sketch opens a body; its extrusion produces the body's first solid.
            if (createsBody(index))
                context[index] = BodyId{feature.id.value()};
            else if (const auto* prism = extrusion(index); prism && prism->target)
                context[index] = prism->target;
            else
                context[index] = context[*inputs[index]];
            state[index] = 2;
        }
    }
    // A base-plane sketch whose prisms all work on one named body belongs to that body.
    for (std::size_t index = 0; index < features.size(); ++index) {
        if (features[index].kind != FeatureKind::sketch || inputs[index])
            continue;
        std::optional<BodyId> target;
        bool shared = true;
        for (std::size_t user = 0; user < features.size(); ++user)
            if (inputs[user] == index) {
                const auto named = extrusion(user)->target;
                shared = shared && named && (!target || target == named);
                target = named;
            }
        if (shared && target)
            context[index] = target;
    }
    return context;
}

std::vector<FeatureBodyIdentity>
legacyBodyIdentities(std::span<const FeatureDescriptor> features,
                     std::span<const FeatureParameters> parameters) {
    const auto context = featureBodyContexts(features, parameters);
    std::vector<FeatureBodyIdentity> result;
    result.reserve(features.size());
    for (std::size_t index = 0; index < features.size(); ++index)
        result.push_back({features[index].id, features[index].kind == FeatureKind::sketch
                                                  ? std::nullopt
                                                  : context[index]});
    return result;
}

} // namespace voelund::document
