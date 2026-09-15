#include <voelund/kernel/NumericPolicy.hpp>
#include <voelund/application/SketchEditorSession.hpp>

#include <algorithm>
#include <cmath>

namespace voelund::application {

bool SketchEditorSession::isConstraintConflicting(
    document::SketchConstraintId constraintId) const noexcept {
    return std::ranges::find(lastSolveResult_.conflictingConstraints, constraintId) !=
           lastSolveResult_.conflictingConstraints.end();
}

std::optional<double>
SketchEditorSession::constraintValue(document::SketchConstraintId constraintId) const noexcept {
    const auto match =
        std::ranges::find(constraints_.values, constraintId, &document::SketchConstraint::id);
    if (match == constraints_.values.end() ||
        (match->kind != document::SketchConstraintKind::distance &&
         match->kind != document::SketchConstraintKind::radius &&
         match->kind != document::SketchConstraintKind::diameter &&
         match->kind != document::SketchConstraintKind::pointDistance &&
         match->kind != document::SketchConstraintKind::pointLineDistance &&
         match->kind != document::SketchConstraintKind::pointDistanceX &&
         match->kind != document::SketchConstraintKind::pointDistanceY))
        return std::nullopt;
    return match->value;
}

bool SketchEditorSession::setConstraintValue(document::SketchConstraintId constraintId,
                                             double value) {
    return constraintValue(constraintId).has_value() &&
           setConstraintInternalValue(constraintId, value);
}

std::optional<kernel::Radians>
SketchEditorSession::constraintAngle(document::SketchConstraintId constraintId) const noexcept {
    const auto match =
        std::ranges::find(constraints_.values, constraintId, &document::SketchConstraint::id);
    if (match == constraints_.values.end() || match->kind != document::SketchConstraintKind::angle)
        return std::nullopt;
    return match->angleValue;
}

bool SketchEditorSession::setConstraintAngle(document::SketchConstraintId constraintId,
                                             kernel::Radians angle) {
    return constraintAngle(constraintId).has_value() &&
           setConstraintInternalValue(constraintId, angle.value());
}

bool SketchEditorSession::setConstraintInternalValue(document::SketchConstraintId constraintId,
                                                     double value) {
    if (!active_ || !std::isfinite(value))
        return false;
    const auto match =
        std::ranges::find(constraints_.values, constraintId, &document::SketchConstraint::id);
    if (match == constraints_.values.end())
        return false;
    const bool angle = match->kind == document::SketchConstraintKind::angle;
    const bool directional = match->kind == document::SketchConstraintKind::pointDistanceX ||
                             match->kind == document::SketchConstraintKind::pointDistanceY;
    const bool dimensional = match->kind == document::SketchConstraintKind::distance ||
                             match->kind == document::SketchConstraintKind::radius ||
                             match->kind == document::SketchConstraintKind::diameter ||
                             match->kind == document::SketchConstraintKind::pointDistance ||
                             match->kind == document::SketchConstraintKind::pointLineDistance ||
                             directional || angle;
    if (!dimensional ||
        (angle         ? std::abs(value) <= kernel::toRadians(kernel::Degrees(1.0e-9)).value() ||
                             std::abs(value) >= std::numbers::pi
         : directional ? !kernel::modelCoordinate(value)
                       : !kernel::modelLength(value)) ||
        (angle ? match->angleValue.value() : match->value) == value)
        return false;
    const document::SketchElements originalElements = elements_;
    const auto originalConstraint = *match;
    if (angle)
        match->angleValue = kernel::Radians(value);
    else
        match->value = value;
    if (solveConstraints())
        return true;
    elements_ = originalElements;
    *match = originalConstraint;
    static_cast<void>(solveConstraints());
    return false;
}

std::vector<features::SketchConstraintAnnotation>
SketchEditorSession::evaluatedConstraints() const {
    return features::evaluateSketchConstraintAnnotations(parameters(false));
}

} // namespace voelund::application
