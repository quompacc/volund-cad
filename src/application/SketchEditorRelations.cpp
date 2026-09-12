#include <voelund/application/SketchEditorSession.hpp>

#include <algorithm>
#include <utility>

namespace voelund::application {
namespace {

const document::SketchElement* findElement(const document::SketchElements& elements,
                                           document::SketchElementId id) {
    const auto match = std::ranges::find(elements.values, id, &document::SketchElement::id);
    return match == elements.values.end() ? nullptr : &*match;
}

bool line(const document::SketchElement* element) {
    return element && std::holds_alternative<document::SketchLine>(element->geometry);
}

bool circle(const document::SketchElement* element) {
    return element && std::holds_alternative<document::SketchCircle>(element->geometry);
}

} // namespace

bool SketchEditorSession::canAddSelectedRelation(
    document::SketchConstraintKind kind) const noexcept {
    if (!active_ || selectedReferences_.size() != 2)
        return false;
    const auto& first = selectedReferences_[0];
    const auto& second = selectedReferences_[1];
    if (first == second)
        return false;
    if (kind == document::SketchConstraintKind::coincident)
        return true;
    if (first.element == second.element)
        return false;
    const document::SketchElement* firstElement = findElement(elements_, first.element);
    const document::SketchElement* secondElement = findElement(elements_, second.element);
    if (kind == document::SketchConstraintKind::parallel ||
        kind == document::SketchConstraintKind::perpendicular)
        return line(firstElement) && line(secondElement);
    if (kind == document::SketchConstraintKind::pointOnObject)
        return (line(firstElement) || circle(firstElement)) &&
               (line(secondElement) || circle(secondElement));
    if (kind == document::SketchConstraintKind::equal)
        return (line(firstElement) && line(secondElement)) ||
               (circle(firstElement) && circle(secondElement));
    if (kind == document::SketchConstraintKind::tangent)
        return (line(firstElement) && circle(secondElement)) ||
               (circle(firstElement) && line(secondElement)) ||
               (circle(firstElement) && circle(secondElement));
    return false;
}

bool SketchEditorSession::addSelectedRelation(document::SketchConstraintKind kind) {
    if (!canAddSelectedRelation(kind))
        return false;
    constraints_.values.push_back({document::SketchConstraintId{constraints_.nextId++}, kind,
                                   selectedReferences_[0], selectedReferences_[1], 0.0});
    document::SketchParameters sketch{plane_, elements_, constraints_};
    lastSolveResult_ = features::solveSketchConstraints(sketch);
    if (lastSolveResult_.status == features::SketchSolveStatus::success)
        elements_ = std::move(sketch.elements);
    return true;
}

} // namespace voelund::application
