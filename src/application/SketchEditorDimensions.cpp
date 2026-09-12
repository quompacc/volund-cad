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
         match->kind != document::SketchConstraintKind::radius))
        return std::nullopt;
    return match->value;
}

bool SketchEditorSession::setConstraintValue(document::SketchConstraintId constraintId,
                                             double value) {
    if (!active_ || !std::isfinite(value) || value <= 0.0)
        return false;
    const auto match =
        std::ranges::find(constraints_.values, constraintId, &document::SketchConstraint::id);
    if (match == constraints_.values.end() ||
        (match->kind != document::SketchConstraintKind::distance &&
         match->kind != document::SketchConstraintKind::radius) ||
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
