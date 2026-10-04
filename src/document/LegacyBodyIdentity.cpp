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
            const bool createsBody = feature.kind == FeatureKind::box ||
                                     (feature.kind == FeatureKind::sketch && !inputs[index]) ||
                                     (feature.kind == FeatureKind::cylinder &&
                                      std::get<CylinderParameters>(parameters[index]).resultMode ==
                                          CylinderResultMode::separate);
            context[index] = createsBody ? BodyId{feature.id.value()} : context[*inputs[index]];
            state[index] = 2;
        }
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
