#include <voelund/application/SketchEditorSession.hpp>

#include <algorithm>
#include <optional>

namespace voelund::application {
namespace {

std::optional<document::SketchPoint>
referencePoint(std::span<const document::SketchElement> elements,
               const document::SketchGeometryReference& reference) {
    const auto match = std::ranges::find(elements, reference.element, &document::SketchElement::id);
    if (match == elements.end())
        return std::nullopt;
    if (const auto* line = std::get_if<document::SketchLine>(&match->geometry)) {
        if (reference.point == document::SketchPointPosition::center)
            return std::nullopt;
        return reference.point == document::SketchPointPosition::end ? line->end : line->start;
    }
    if (const auto* circle = std::get_if<document::SketchCircle>(&match->geometry)) {
        if (reference.point == document::SketchPointPosition::center)
            return circle->center;
    }
    return std::nullopt;
}

bool hasCoordinate(std::span<const document::SketchConstraint> constraints,
                   const document::SketchGeometryReference& reference,
                   document::SketchConstraintKind kind) {
    return std::ranges::any_of(constraints, [reference, kind](const auto& constraint) {
        return constraint.kind == kind && constraint.first == reference &&
               !constraint.second.has_value();
    });
}

} // namespace

bool SketchEditorSession::canLockSelectedReference() const noexcept {
    return active_ && selectedReferences_.size() == 1 &&
           referencePoint(elements_.values, selectedReferences_.front()).has_value() &&
           !isSelectedReferenceLocked();
}

bool SketchEditorSession::isSelectedReferenceLocked() const noexcept {
    if (!active_ || selectedReferences_.size() != 1)
        return false;
    const auto& reference = selectedReferences_.front();
    return hasCoordinate(constraints_.values, reference,
                         document::SketchConstraintKind::coordinateX) &&
           hasCoordinate(constraints_.values, reference,
                         document::SketchConstraintKind::coordinateY);
}

bool SketchEditorSession::lockSelectedReference() {
    if (!canLockSelectedReference())
        return false;
    const auto reference = selectedReferences_.front();
    const document::SketchPoint position = *referencePoint(elements_.values, reference);
    const auto addCoordinate = [this, reference](document::SketchConstraintKind kind,
                                                 double value) {
        if (hasCoordinate(constraints_.values, reference, kind))
            return;
        constraints_.values.push_back({document::SketchConstraintId{constraints_.nextId++}, kind,
                                       reference, std::nullopt, value});
    };
    addCoordinate(document::SketchConstraintKind::coordinateX, position.x);
    addCoordinate(document::SketchConstraintKind::coordinateY, position.y);
    static_cast<void>(solveConstraints());
    return true;
}

bool SketchEditorSession::unlockSelectedReference() {
    if (!isSelectedReferenceLocked())
        return false;
    const auto reference = selectedReferences_.front();
    std::erase_if(constraints_.values, [reference](const auto& constraint) {
        return constraint.first == reference && !constraint.second.has_value() &&
               (constraint.kind == document::SketchConstraintKind::coordinateX ||
                constraint.kind == document::SketchConstraintKind::coordinateY);
    });
    static_cast<void>(solveConstraints());
    return true;
}

} // namespace voelund::application
