#include "CommandInput.hpp"
#include "FieldBindingDetail.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/document/DocumentSnapshot.hpp>
#include <voelund/document/FeatureFields.hpp>

#include <algorithm>
#include <stdexcept>
#include <utility>

// N01b (user decision 2026-10-21): measure fields of every feature bound to parameters through
// one list in the parameter table; an extrusion's distance and offset keep their own bindings.
namespace voelund::application {
namespace detail {
namespace {
bool isExtrusionLength(const document::FeatureParameters& parameters, const std::string& field) {
    return std::holds_alternative<document::ExtrusionParameters>(parameters) &&
           (field == "distance" || field == "offset");
}

// An extrusion's distance or offset through its own binding (PAR01), the value following.
bool bindExtrusionLength(document::ModelDocument& candidate, document::FeatureId feature,
                         const std::string& field, const document::DocumentParameter* parameter) {
    auto changed = candidate.extrusionParameters(feature);
    const bool offset = field == "offset";
    (offset ? changed.offsetParameter : changed.distanceParameter) =
        parameter != nullptr ? std::optional{parameter->id} : std::nullopt;
    if (parameter != nullptr)
        (offset ? changed.offset : changed.distance) = parameter->value;
    return candidate.setExtrusionParameters(feature, changed);
}

// The feature `candidate` has and `before` has not (no companion).
std::optional<document::FeatureId> createdFeature(const document::ModelDocument& candidate,
                                                  const document::ModelDocument& before) {
    for (const auto& feature : candidate.features())
        if (!before.contains(feature.id) && !document::isCompanion(feature.kind))
            return feature.id;
    return std::nullopt;
}

bool hasField(const document::ModelDocument& candidate, document::FeatureId feature,
              const std::string& field) {
    if (!candidate.contains(feature))
        return false;
    const auto parameters =
        document::DocumentSnapshot::capture(candidate).featureParameters(feature);
    return isExtrusionLength(parameters, field) ||
           document::featureField(parameters, field).has_value();
}
} // namespace

bool applyFieldRequest(document::ModelDocument& candidate, const FieldBindingRequest& request,
                       document::FeatureId feature) {
    const auto parameters =
        document::DocumentSnapshot::capture(candidate).featureParameters(feature);
    const auto* parameter =
        request.parameter ? document::findParameter(candidate.parameterTable(), *request.parameter)
                          : nullptr;
    if (request.parameter && parameter == nullptr)
        throw std::invalid_argument("Unknown parameter");
    if (isExtrusionLength(parameters, request.field))
        return bindExtrusionLength(candidate, feature, request.field, parameter);
    if (!document::featureField(parameters, request.field))
        throw std::invalid_argument("The feature has no field " + request.field);
    auto table = candidate.parameterTable();
    std::erase_if(table.bindings, [&](const document::FieldBinding& binding) {
        return binding.feature == feature && binding.field == request.field;
    });
    if (request.parameter)
        table.bindings.push_back({feature, request.field, *request.parameter});
    // The document gives the field its parameter's value (document::applyFieldBindings).
    return candidate.setParameterTable(table);
}

void applyPendingFields(document::ModelDocument& candidate, const document::ModelDocument& before,
                        const std::vector<FieldBindingRequest>& pending) {
    const auto created = createdFeature(candidate, before);
    // A field the feature does not have (any more), e.g. a pattern's angle after switching to
    // linear, is left out.
    for (const auto& request : pending)
        if (const auto feature = request.feature ? request.feature : created;
            feature && hasField(candidate, *feature, request.field))
            static_cast<void>(applyFieldRequest(candidate, request, *feature));
}

std::optional<std::string> takePendingFields(document::ModelDocument& candidate,
                                             const document::ModelDocument& before,
                                             std::vector<FieldBindingRequest>& pending) {
    const auto requests = std::exchange(pending, {});
    if (requests.empty())
        return std::nullopt;
    const auto applied = prepareInput([&] {
        applyPendingFields(candidate, before, requests);
        return true;
    });
    return applied.value ? std::nullopt : std::optional{applied.diagnostic};
}

} // namespace detail

CommandResult CadSession::bindFeatureField(document::FeatureId feature, std::string field,
                                           std::optional<document::ParameterId> parameter) {
    pendingFields_.clear();
    return commitParameterEdit(
        [&](document::ModelDocument& candidate) {
            return detail::applyFieldRequest(candidate, {feature, std::move(field), parameter},
                                             feature);
        },
        ErrorDisposition::keepEditableErrors);
}

} // namespace voelund::application
