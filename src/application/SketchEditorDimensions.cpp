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
         match->kind != document::SketchConstraintKind::pointDistance &&
         match->kind != document::SketchConstraintKind::pointDistanceX &&
         match->kind != document::SketchConstraintKind::pointDistanceY &&
         match->kind != document::SketchConstraintKind::angle))
        return std::nullopt;
    return match->value;
}

bool SketchEditorSession::setConstraintValue(document::SketchConstraintId constraintId,
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
                             match->kind == document::SketchConstraintKind::pointDistance ||
                             directional || angle;
    if (!dimensional ||
        (angle         ? std::abs(value) <= 1.0e-9 || std::abs(value) >= 180.0
         : directional ? false
                       : value <= 0.0) ||
        match->value == value)
        return false;
    const document::SketchElements originalElements = elements_;
    const double originalValue = match->value;
    match->value = value;
    if (solveConstraints())
        return true;
    elements_ = originalElements;
    match->value = originalValue;
    static_cast<void>(solveConstraints());
    return false;
}

std::vector<features::SketchConstraintAnnotation>
SketchEditorSession::evaluatedConstraints() const {
    return features::evaluateSketchConstraintAnnotations(parameters(false));
}

} // namespace voelund::application
