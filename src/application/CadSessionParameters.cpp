#include "CommandInput.hpp"
#include <algorithm>
#include <stdexcept>
#include <utility>
#include <voelund/application/CadSession.hpp>
#include <voelund/document/IdCapacity.hpp>
#include <voelund/features/SketchConstraintSolver.hpp>
#include <voelund/kernel/Angle.hpp>

namespace voelund::application {
namespace {
using document::DocumentParameter;
using document::ParameterId;
using document::ParameterTable;

DocumentParameter& parameterIn(ParameterTable& table, ParameterId id) {
    const auto found = std::ranges::find(table.values, id, &DocumentParameter::id);
    if (found == table.values.end())
        throw std::invalid_argument("Unknown parameter");
    return *found;
}

bool mirrorSketch(document::SketchParameters& sketch, const DocumentParameter& parameter) {
    bool changed = false;
    for (auto& constraint : sketch.constraints.values) {
        if (constraint.parameter != parameter.id)
            continue;
        if (constraint.kind == document::SketchConstraintKind::angle) {
            const auto angle = kernel::toRadians(kernel::Degrees(parameter.value));
            changed = changed || constraint.angleValue.value() != angle.value();
            constraint.angleValue = angle;
        } else {
            changed = changed || constraint.value != parameter.value;
            constraint.value = parameter.value;
        }
    }
    return changed;
}

bool usesParameter(const document::FeatureParameters& feature, ParameterId id) {
    if (const auto* sketch = std::get_if<document::SketchParameters>(&feature))
        return std::ranges::any_of(sketch->constraints.values,
                                   [&](const auto& c) { return c.parameter == id; });
    const auto* extrusion = std::get_if<document::ExtrusionParameters>(&feature);
    return extrusion && extrusion->distanceParameter == id;
}
} // namespace

const document::ParameterTable& CadSession::parameterTable() const noexcept {
    return document_.parameterTable();
}

std::size_t CadSession::parameterUseCount(document::ParameterId id) const {
    std::size_t uses = 0;
    const auto snapshot = document::DocumentSnapshot::capture(document_);
    for (const auto& feature : snapshot.parameters()) {
        if (const auto* sketch = std::get_if<document::SketchParameters>(&feature))
            uses += static_cast<std::size_t>(std::ranges::count_if(
                sketch->constraints.values, [&](const auto& c) { return c.parameter == id; }));
        else if (usesParameter(feature, id))
            ++uses;
    }
    return uses;
}

bool CadSession::mirrorParameter(document::ModelDocument& candidate,
                                 document::ParameterId id) const {
    const auto* parameter = document::findParameter(candidate.parameterTable(), id);
    if (parameter == nullptr)
        throw std::invalid_argument("Unknown parameter");
    const DocumentParameter value = *parameter;
    bool changed = false;
    // Copied: every set below publishes a new candidate view.
    const std::vector<document::FeatureDescriptor> features(candidate.features().begin(),
                                                            candidate.features().end());
    for (const auto& feature : features) {
        if (feature.kind == document::FeatureKind::sketch) {
            auto sketch = candidate.sketchParameters(feature.id);
            if (!mirrorSketch(sketch, value))
                continue;
            // Solve here so the stored geometry follows; if it cannot, the new values are
            // kept and evaluation reports the sketch as an error (docs/parameter-table.md).
            // Unbound legacy supports are solved by evaluation only.
            auto solved = sketch;
            try {
                if (feature.inputFeature && solved.supportBinding)
                    resolveSketchSupport(*feature.inputFeature, solved);
                if ((!feature.inputFeature || solved.supportBinding) &&
                    features::solveSketchConstraints(solved).status ==
                        features::SketchSolveStatus::success)
                    sketch = std::move(solved);
            } catch (const std::invalid_argument&) {
            }
            changed = candidate.setSketchParameters(feature.id, sketch) || changed;
        } else if (feature.kind == document::FeatureKind::extrusion) {
            auto extrusion = candidate.extrusionParameters(feature.id);
            if (extrusion.distanceParameter != id || extrusion.distance == value.value)
                continue;
            extrusion.distance = value.value;
            changed = candidate.setExtrusionParameters(feature.id, extrusion) || changed;
        }
    }
    return changed;
}

CommandResult CadSession::commitParameterEdit(
    const std::function<bool(document::ModelDocument&)>& edit, ErrorDisposition disposition) {
    document::DocumentTransaction transaction(document_);
    const auto input = detail::prepareInput([&] { return edit(transaction.edit()); });
    if (!input.value)
        return CommandResult::invalidInput(input.diagnostic);
    if (!*input.value)
        return {CommandStatus::rejected, RejectionReason::noChange};
    return finishCandidate(transaction, selectedFeature_, CommandKind::editFeature, disposition,
                           false);
}

CommandResult CadSession::createParameter(std::string name, document::ParameterUnit unit,
                                          double value, std::string comment) {
    return commitParameterEdit(
        [&](document::ModelDocument& candidate) {
            auto table = candidate.parameterTable();
            if (!document::hasIdCapacity(table.nextId, 1))
                throw std::overflow_error("Parameter IDs exhausted");
            table.values.push_back(
                {ParameterId{table.nextId++}, std::move(name), unit, value, std::move(comment)});
            return candidate.setParameterTable(table);
        },
        ErrorDisposition::reject);
}

CommandResult CadSession::setParameterValue(document::ParameterId id, double value,
                                            ErrorDisposition disposition) {
    return commitParameterEdit(
        [&](document::ModelDocument& candidate) {
            auto table = candidate.parameterTable();
            parameterIn(table, id).value = value;
            const bool changed = candidate.setParameterTable(table);
            return mirrorParameter(candidate, id) || changed;
        },
        disposition);
}

CommandResult CadSession::renameParameter(document::ParameterId id, std::string name) {
    return commitParameterEdit(
        [&](document::ModelDocument& candidate) {
            auto table = candidate.parameterTable();
            parameterIn(table, id).name = std::move(name);
            return candidate.setParameterTable(table);
        },
        ErrorDisposition::reject);
}

CommandResult CadSession::setParameterComment(document::ParameterId id, std::string comment) {
    return commitParameterEdit(
        [&](document::ModelDocument& candidate) {
            auto table = candidate.parameterTable();
            parameterIn(table, id).comment = std::move(comment);
            return candidate.setParameterTable(table);
        },
        ErrorDisposition::reject);
}

CommandResult CadSession::deleteParameter(document::ParameterId id, ParameterDeletion mode) {
    return commitParameterEdit(
        [&](document::ModelDocument& candidate) {
            auto table = candidate.parameterTable();
            const auto name = parameterIn(table, id).name;
            const std::vector<document::FeatureDescriptor> features(candidate.features().begin(),
                                                                    candidate.features().end());
            for (const auto& feature : features) {
                if (feature.kind == document::FeatureKind::sketch) {
                    auto sketch = candidate.sketchParameters(feature.id);
                    if (!usesParameter(sketch, id))
                        continue;
                    if (mode == ParameterDeletion::refuseWhenUsed)
                        throw std::invalid_argument("Parameter is still in use: " + name);
                    for (auto& constraint : sketch.constraints.values)
                        if (constraint.parameter == id)
                            constraint.parameter.reset();
                    static_cast<void>(candidate.setSketchParameters(feature.id, sketch));
                } else if (feature.kind == document::FeatureKind::extrusion) {
                    auto extrusion = candidate.extrusionParameters(feature.id);
                    if (extrusion.distanceParameter != id)
                        continue;
                    if (mode == ParameterDeletion::refuseWhenUsed)
                        throw std::invalid_argument("Parameter is still in use: " + name);
                    extrusion.distanceParameter.reset();
                    static_cast<void>(candidate.setExtrusionParameters(feature.id, extrusion));
                }
            }
            std::erase_if(table.values, [&](const auto& p) { return p.id == id; });
            return candidate.setParameterTable(table);
        },
        ErrorDisposition::reject);
}

CommandResult CadSession::bindSketchDimension(document::FeatureId sketchFeature,
                                              document::SketchConstraintId constraint,
                                              std::optional<document::ParameterId> parameter,
                                              ErrorDisposition disposition) {
    return commitParameterEdit(
        [&](document::ModelDocument& candidate) {
            auto sketch = candidate.sketchParameters(sketchFeature);
            const auto found = std::ranges::find(sketch.constraints.values, constraint,
                                                 &document::SketchConstraint::id);
            if (found == sketch.constraints.values.end())
                throw std::invalid_argument("Unknown sketch constraint");
            // Unbinding keeps the current value as a fixed one.
            found->parameter = parameter;
            const bool changed = candidate.setSketchParameters(sketchFeature, sketch);
            return (parameter && mirrorParameter(candidate, *parameter)) || changed;
        },
        disposition);
}

CommandResult CadSession::bindExtrusionDistance(document::FeatureId extrusionFeature,
                                                std::optional<document::ParameterId> parameter,
                                                ErrorDisposition disposition) {
    return commitParameterEdit(
        [&](document::ModelDocument& candidate) {
            auto extrusion = candidate.extrusionParameters(extrusionFeature);
            extrusion.distanceParameter = parameter;
            const bool changed = candidate.setExtrusionParameters(extrusionFeature, extrusion);
            return (parameter && mirrorParameter(candidate, *parameter)) || changed;
        },
        disposition);
}
} // namespace voelund::application
