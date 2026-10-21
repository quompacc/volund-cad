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
    return extrusion && (extrusion->distanceParameter == id || extrusion->offsetParameter == id);
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
    // N01b: bound fields of other features.
    return uses + static_cast<std::size_t>(std::ranges::count(parameterTable().bindings, id,
                                                              &document::FieldBinding::parameter));
}

namespace {
// N01: a parameter other expressions use cannot go.
void refuseWhenInExpressions(const document::ParameterTable& table, const std::string& name) {
    const auto dependents = document::parameterDependents(table, name);
    if (dependents.empty())
        return;
    std::string names;
    for (const auto id : dependents)
        names += (names.empty() ? "" : ", ") + document::findParameter(table, id)->name;
    throw std::invalid_argument("Parameter is used in the expressions of: " + names);
}

// The table without the parameter; its field bindings (N01b) go too, the fields keep their values.
document::ParameterTable withoutParameter(document::ParameterTable table, document::ParameterId id,
                                          ParameterDeletion mode, const std::string& name) {
    if (mode == ParameterDeletion::refuseWhenUsed &&
        std::ranges::count(table.bindings, id, &document::FieldBinding::parameter) > 0)
        throw std::invalid_argument("Parameter is still in use: " + name);
    std::erase_if(table.bindings, [&](const auto& b) { return b.parameter == id; });
    std::erase_if(table.values, [&](const auto& p) { return p.id == id; });
    return table;
}
} // namespace

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
            if (extrusion.distanceParameter == id)
                extrusion.distance = value.value;
            if (extrusion.offsetParameter == id)
                extrusion.offset = value.value; // T03
            changed = candidate.setExtrusionParameters(feature.id, extrusion) || changed;
        }
    }
    return changed;
}

CommandResult
CadSession::commitParameterEdit(const std::function<bool(document::ModelDocument&)>& edit,
                                ErrorDisposition disposition) {
    document::DocumentTransaction transaction(document_);
    const auto input = detail::prepareInput([&] { return edit(transaction.edit()); });
    if (!input.value)
        return CommandResult::invalidInput(input.diagnostic);
    if (!*input.value)
        return {CommandStatus::rejected, RejectionReason::noChange};
    return finishCandidate(transaction, selectedFeature_, CommandKind::editFeature, disposition,
                           false);
}

// N01: the table with every expression worked out; each parameter whose value changed is
// mirrored into what is bound to it.
bool CadSession::applyParameterTable(document::ModelDocument& candidate,
                                     document::ParameterTable table) const {
    document::resolveParameterTable(table);
    const auto before = candidate.parameterTable();
    bool changed = candidate.setParameterTable(table);
    for (const auto& parameter : table.values) {
        const auto* old = document::findParameter(before, parameter.id);
        if (old == nullptr || old->value != parameter.value)
            changed = mirrorParameter(candidate, parameter.id) || changed;
    }
    return changed;
}

CommandResult CadSession::createParameter(std::string name, document::ParameterUnit unit,
                                          double value, std::string comment,
                                          std::string expression) {
    return commitParameterEdit(
        [&](document::ModelDocument& candidate) {
            auto table = candidate.parameterTable();
            if (!document::hasIdCapacity(table.nextId, 1))
                throw std::overflow_error("Parameter IDs exhausted");
            table.values.push_back({ParameterId{table.nextId++}, std::move(name), unit, value,
                                    std::move(comment), std::move(expression)});
            return applyParameterTable(candidate, std::move(table));
        },
        ErrorDisposition::reject);
}

CommandResult CadSession::setParameterValue(document::ParameterId id, double value,
                                            ErrorDisposition disposition) {
    return commitParameterEdit(
        [&](document::ModelDocument& candidate) {
            auto table = candidate.parameterTable();
            auto& parameter = parameterIn(table, id);
            parameter.value = value;
            parameter.expression.clear(); // N01: a typed value replaces an expression
            return applyParameterTable(candidate, std::move(table));
        },
        disposition);
}

CommandResult CadSession::setParameterExpression(document::ParameterId id, std::string expression,
                                                 ErrorDisposition disposition) {
    return commitParameterEdit(
        [&](document::ModelDocument& candidate) {
            auto table = candidate.parameterTable();
            parameterIn(table, id).expression = std::move(expression);
            return applyParameterTable(candidate, std::move(table));
        },
        disposition);
}

CommandResult CadSession::renameBody(document::BodyId body, std::string name) {
    return commitParameterEdit(
        [&](document::ModelDocument& candidate) {
            return candidate.renameBody(body, std::move(name));
        },
        ErrorDisposition::reject);
}

bool CadSession::setBodyHidden(document::BodyId body, bool hidden) {
    if (!document_.setBodyHidden(body, hidden))
        return false;
    // The revision moved without a geometric change; the evaluation reuses every result.
    static_cast<void>(recompute());
    return true;
}

// M14: the marker moves without an undo step; the model is evaluated up to it.
bool CadSession::setRollback(std::optional<document::FeatureId> featureId) {
    if (!document_.setRollback(featureId))
        return false;
    normalizeSelection();
    static_cast<void>(recompute());
    return true;
}

std::optional<document::FeatureId> CadSession::rollbackAfter() const noexcept {
    return document_.rollbackAfter();
}

CommandResult CadSession::renameParameter(document::ParameterId id, std::string name) {
    return commitParameterEdit(
        [&](document::ModelDocument& candidate) {
            auto table = candidate.parameterTable();
            auto& parameter = parameterIn(table, id);
            // N01: expressions follow the new name.
            for (auto& other : table.values)
                if (!other.expression.empty())
                    other.expression =
                        document::renameInExpression(other.expression, parameter.name, name);
            parameter.name = std::move(name);
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
            refuseWhenInExpressions(table, name); // N01
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
                    if (extrusion.distanceParameter != id && extrusion.offsetParameter != id)
                        continue;
                    if (mode == ParameterDeletion::refuseWhenUsed)
                        throw std::invalid_argument("Parameter is still in use: " + name);
                    if (extrusion.distanceParameter == id)
                        extrusion.distanceParameter.reset();
                    if (extrusion.offsetParameter == id)
                        extrusion.offsetParameter.reset();
                    static_cast<void>(candidate.setExtrusionParameters(feature.id, extrusion));
                }
            }
            table = withoutParameter(candidate.parameterTable(), id, mode, name);
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
                                                ErrorDisposition disposition, bool offset) {
    return commitParameterEdit(
        [&](document::ModelDocument& candidate) {
            auto extrusion = candidate.extrusionParameters(extrusionFeature);
            (offset ? extrusion.offsetParameter : extrusion.distanceParameter) = parameter;
            const bool changed = candidate.setExtrusionParameters(extrusionFeature, extrusion);
            return (parameter && mirrorParameter(candidate, *parameter)) || changed;
        },
        disposition);
}
} // namespace voelund::application
