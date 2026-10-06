#include <voelund/document/ModelDocument.hpp>

#include <stdexcept>

namespace voelund::document {
namespace {
const DocumentParameter& bound(const ParameterTable& table, ParameterId id, ParameterUnit unit) {
    const auto* parameter = findParameter(table, id);
    if (parameter == nullptr)
        throw std::invalid_argument("Binding refers to an unknown parameter");
    if (parameter->unit != unit)
        throw std::invalid_argument("Parameter unit does not match the bound dimension: " +
                                    parameter->name);
    return *parameter;
}
} // namespace

void validateParameterBindings(std::span<const FeatureParameters> parameters,
                               const ParameterTable& table) {
    validateParameterTable(table);
    for (const auto& feature : parameters) {
        if (const auto* sketch = std::get_if<SketchParameters>(&feature)) {
            for (const auto& constraint : sketch->constraints.values) {
                if (!constraint.parameter)
                    continue;
                const auto unit = dimensionUnit(constraint.kind);
                if (!unit)
                    throw std::invalid_argument("Only dimensional constraints bind parameters");
                static_cast<void>(bound(table, *constraint.parameter, *unit));
            }
        } else if (const auto* extrusion = std::get_if<ExtrusionParameters>(&feature)) {
            for (const auto& parameter : {extrusion->distanceParameter, extrusion->offsetParameter})
                if (parameter)
                    static_cast<void>(bound(table, *parameter, ParameterUnit::length));
        }
    }
}

} // namespace voelund::document
