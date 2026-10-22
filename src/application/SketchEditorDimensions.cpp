// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>
#include <voelund/features/SketchConstraintSolver.hpp>
#include <voelund/kernel/Angle.hpp>
#include <voelund/kernel/NumericPolicy.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace voelund::application {
namespace {

using document::SketchConstraintKind;

bool isCoordinate(SketchConstraintKind kind) {
    return kind == SketchConstraintKind::coordinateX || kind == SketchConstraintKind::coordinateY ||
           kind == SketchConstraintKind::pointDistanceX ||
           kind == SketchConstraintKind::pointDistanceY;
}

[[noreturn]] void refuse(const std::string& reason) {
    throw std::invalid_argument("Maß: " + reason);
}

} // namespace

bool SketchEditorSession::isConstraintConflicting(
    document::SketchConstraintId constraintId) const noexcept {
    return std::ranges::find(lastSolveResult_.conflictingConstraints, constraintId) !=
           lastSolveResult_.conflictingConstraints.end();
}

bool SketchEditorSession::isConstraintRedundant(
    document::SketchConstraintId constraintId) const noexcept {
    return std::ranges::find(lastSolveResult_.redundantConstraints, constraintId) !=
           lastSolveResult_.redundantConstraints.end();
}

std::optional<double>
SketchEditorSession::constraintValue(document::SketchConstraintId constraintId) const noexcept {
    const auto match =
        std::ranges::find(constraints_.values, constraintId, &document::SketchConstraint::id);
    if (match == constraints_.values.end() || match->kind == SketchConstraintKind::angle ||
        features::sketchAnnotationCategory(match->kind) !=
            features::SketchAnnotationCategory::dimension)
        return std::nullopt;
    return match->value;
}

std::optional<kernel::Radians>
SketchEditorSession::constraintAngle(document::SketchConstraintId constraintId) const noexcept {
    const auto match =
        std::ranges::find(constraints_.values, constraintId, &document::SketchConstraint::id);
    if (match == constraints_.values.end() || match->kind != SketchConstraintKind::angle)
        return std::nullopt;
    return match->angleValue;
}

std::optional<std::string>
SketchEditorSession::dimensionProblem(document::SketchConstraintId constraintId) const {
    if (!active_)
        return "Maß: Der Skizzeneditor ist nicht aktiv";
    const auto match =
        std::ranges::find(constraints_.values, constraintId, &document::SketchConstraint::id);
    if (match == constraints_.values.end())
        return "Maß: Die Bedingung gibt es nicht mehr";
    if (features::sketchAnnotationCategory(match->kind) !=
        features::SketchAnnotationCategory::dimension)
        return "Maß: Eine Beziehung hat keinen Wert";
    if (match->projectedEdge.has_value() || !match->auxiliaryValues.empty())
        return "Maß: Die Koordinate folgt einer Referenzkante";
    if (match->reference)
        return "Maß: Ein Referenzmaß zeigt nur den Wert; erst auf treibend umschalten";
    return std::nullopt;
}

bool SketchEditorSession::isReferenceDimension(
    document::SketchConstraintId constraintId) const noexcept {
    const auto match =
        std::ranges::find(constraints_.values, constraintId, &document::SketchConstraint::id);
    return match != constraints_.values.end() && match->reference;
}

void SketchEditorSession::toggleReference(document::SketchConstraintId constraintId) {
    if (!active_)
        refuse("Der Skizzeneditor ist nicht aktiv");
    const auto match =
        std::ranges::find(constraints_.values, constraintId, &document::SketchConstraint::id);
    if (match == constraints_.values.end() || !features::isSketchDimension(match->kind))
        refuse("Nur eine Bemaßung kann Referenz sein");
    if (match->parameter || match->projectedEdge || !match->auxiliaryValues.empty())
        refuse("Ein gebundenes Maß bleibt treibend");
    Rollback rollback(*this);
    const bool driving = match->reference;
    const auto before = overdetermination();
    match->reference = !driving;
    // The solver may name another dimension of the redundant set, so the count decides.
    if (!solveConstraints() || (driving && overdetermination() > before))
        refuse("Treibend würde es die Skizze überbestimmen; nichts geändert");
    rollback.keep();
    commitHistory();
}

bool SketchEditorSession::editDimension(document::SketchConstraintId constraintId, double value) {
    if (const auto problem = dimensionProblem(constraintId))
        throw std::invalid_argument(*problem);
    const auto match =
        std::ranges::find(constraints_.values, constraintId, &document::SketchConstraint::id);
    const bool angle = match->kind == SketchConstraintKind::angle;
    if (!std::isfinite(value))
        refuse("Der Wert ist keine Zahl");
    if (angle) {
        if (std::abs(value) <= kernel::toRadians(kernel::Degrees(1.0e-9)).value() ||
            std::abs(value) >= std::numbers::pi)
            refuse("Ein Winkel liegt zwischen 0° und 180°, beide ausgenommen");
    } else if (isCoordinate(match->kind)) {
        if (!kernel::modelCoordinate(value))
            refuse(kernel::modelRangeDiagnostic);
    } else if (!kernel::modelLength(value)) {
        refuse(value <= 0.0 ? "Eine Länge muss größer als 0 sein"
                            : std::string(kernel::modelRangeDiagnostic));
    }
    // PAR02 (docs/parameter-table.md, rule 7): a number on a bound dimension unbinds it.
    const bool bound = match->parameter.has_value();
    if (!bound && (angle ? match->angleValue.value() : match->value) == value)
        return false;
    Rollback rollback(*this);
    match->parameter.reset();
    if (angle)
        match->angleValue = kernel::Radians(value);
    else
        match->value = value;
    if (!solveConstraints())
        refuse("Mit diesem Wert lässt sich die Skizze nicht lösen; nichts geändert");
    rollback.keep();
    return true;
}

bool SketchEditorSession::bindDimension(document::SketchConstraintId constraintId,
                                        std::optional<document::ParameterId> parameter) {
    if (const auto problem = dimensionProblem(constraintId))
        throw std::invalid_argument(*problem);
    const auto match =
        std::ranges::find(constraints_.values, constraintId, &document::SketchConstraint::id);
    if (match->parameter == parameter)
        return false;
    Rollback rollback(*this);
    if (parameter) {
        const auto* entry = document::findParameter(session_.parameterTable(), *parameter);
        if (entry == nullptr)
            refuse("Den Parameter gibt es nicht");
        if (document::dimensionUnit(match->kind) != entry->unit)
            refuse(entry->unit == document::ParameterUnit::angle
                       ? "Ein Winkelparameter passt nicht an eine Länge"
                       : "Ein Längenparameter passt nicht an einen Winkel");
        if (match->kind == SketchConstraintKind::angle)
            match->angleValue = kernel::toRadians(kernel::Degrees(entry->value));
        else
            match->value = entry->value;
    }
    match->parameter = parameter;
    if (!solveConstraints())
        refuse("Mit dem Wert des Parameters lässt sich die Skizze nicht lösen; nichts geändert");
    rollback.keep();
    commitHistory();
    return true;
}

void SketchEditorSession::acceptParameterTableChange() noexcept {
    if (supportStamp_)
        supportStamp_ = session_.sceneStatus().document;
}

std::optional<document::ParameterId>
SketchEditorSession::dimensionParameter(document::SketchConstraintId constraintId) const noexcept {
    const auto match =
        std::ranges::find(constraints_.values, constraintId, &document::SketchConstraint::id);
    return match == constraints_.values.end() ? std::nullopt : match->parameter;
}

bool SketchEditorSession::setConstraintValue(document::SketchConstraintId constraintId,
                                             double value) {
    if (!constraintValue(constraintId).has_value())
        return false;
    try {
        return editDimension(constraintId, value);
    } catch (const std::invalid_argument&) {
        return false;
    }
}

bool SketchEditorSession::setConstraintAngle(document::SketchConstraintId constraintId,
                                             kernel::Radians angle) {
    if (!constraintAngle(constraintId).has_value())
        return false;
    try {
        return editDimension(constraintId, angle.value());
    } catch (const std::invalid_argument&) {
        return false;
    }
}

std::vector<features::SketchConstraintAnnotation>
SketchEditorSession::evaluatedConstraints() const {
    return features::evaluateSketchConstraintAnnotations(parameters(false));
}

std::optional<double> SketchEditorSession::selectedCircleRadius() const noexcept {
    if (!selectedElement_.has_value())
        return std::nullopt;
    const auto match =
        std::ranges::find(elements_.values, *selectedElement_, &document::SketchElement::id);
    if (match == elements_.values.end())
        return std::nullopt;
    if (const auto* circle = std::get_if<document::SketchCircle>(&match->geometry))
        return circle->radius;
    const auto* arc = std::get_if<document::SketchArc>(&match->geometry);
    return arc == nullptr ? std::nullopt : std::optional<double>{arc->radius};
}

std::optional<double> SketchEditorSession::selectedLineLength() const noexcept {
    if (!selectedElement_.has_value())
        return std::nullopt;
    const auto match =
        std::ranges::find(elements_.values, *selectedElement_, &document::SketchElement::id);
    if (match == elements_.values.end())
        return std::nullopt;
    const auto* line = std::get_if<document::SketchLine>(&match->geometry);
    if (line == nullptr)
        return std::nullopt;
    return std::hypot(line->end.x - line->start.x, line->end.y - line->start.y);
}

std::optional<double> SketchEditorSession::selectedRectangleWidth() const noexcept {
    if (!selectedElement_.has_value())
        return std::nullopt;
    const auto match =
        std::ranges::find(elements_.values, *selectedElement_, &document::SketchElement::id);
    if (match == elements_.values.end())
        return std::nullopt;
    const auto* rectangle = std::get_if<document::SketchRectangle>(&match->geometry);
    return rectangle == nullptr
               ? std::nullopt
               : std::optional<double>{std::abs(rectangle->second.x - rectangle->first.x)};
}

std::optional<double> SketchEditorSession::selectedRectangleHeight() const noexcept {
    if (!selectedElement_.has_value())
        return std::nullopt;
    const auto match =
        std::ranges::find(elements_.values, *selectedElement_, &document::SketchElement::id);
    if (match == elements_.values.end())
        return std::nullopt;
    const auto* rectangle = std::get_if<document::SketchRectangle>(&match->geometry);
    return rectangle == nullptr
               ? std::nullopt
               : std::optional<double>{std::abs(rectangle->second.y - rectangle->first.y)};
}

} // namespace voelund::application
