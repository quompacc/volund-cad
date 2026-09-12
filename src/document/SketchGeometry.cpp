#include <voelund/document/SketchGeometry.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace voelund::document {
namespace {

constexpr double minimumLength = 1.0e-9;

bool finite(const SketchPoint& point) { return std::isfinite(point.x) && std::isfinite(point.y); }

double squaredDistance(const SketchPoint& first, const SketchPoint& second) {
    const double x = second.x - first.x;
    const double y = second.y - first.y;
    return x * x + y * y;
}

void validateGeometry(const SketchGeometry& geometry) {
    if (const auto* line = std::get_if<SketchLine>(&geometry)) {
        if (!finite(line->start) || !finite(line->end) ||
            squaredDistance(line->start, line->end) <= minimumLength * minimumLength) {
            throw std::invalid_argument("Sketch line must have finite, distinct endpoints");
        }
        if (line->constraint == SketchLineConstraint::horizontal &&
            std::abs(line->end.y - line->start.y) > minimumLength) {
            throw std::invalid_argument("Horizontal sketch line constraint is not satisfied");
        }
        if (line->constraint == SketchLineConstraint::vertical &&
            std::abs(line->end.x - line->start.x) > minimumLength) {
            throw std::invalid_argument("Vertical sketch line constraint is not satisfied");
        }
        return;
    }
    if (const auto* rectangle = std::get_if<SketchRectangle>(&geometry)) {
        if (!finite(rectangle->first) || !finite(rectangle->second) ||
            std::abs(rectangle->second.x - rectangle->first.x) <= minimumLength ||
            std::abs(rectangle->second.y - rectangle->first.y) <= minimumLength) {
            throw std::invalid_argument("Sketch rectangle must have a finite positive area");
        }
        return;
    }
    const SketchCircle& circle = std::get<SketchCircle>(geometry);
    if (!finite(circle.center) || !std::isfinite(circle.radius) || circle.radius <= minimumLength) {
        throw std::invalid_argument("Sketch circle must have a finite positive radius");
    }
}

} // namespace

void validateSketchElements(const SketchElements& elements) {
    if (elements.values.empty()) {
        throw std::invalid_argument("Sketch must contain at least one element");
    }
    std::uint64_t maximumId = 0;
    std::vector<std::uint64_t> ids;
    ids.reserve(elements.values.size());
    for (const SketchElement& element : elements.values) {
        if (element.id.value() == 0) {
            throw std::invalid_argument("Sketch element ID must not be zero");
        }
        validateGeometry(element.geometry);
        maximumId = std::max(maximumId, element.id.value());
        ids.push_back(element.id.value());
    }
    std::ranges::sort(ids);
    if (std::ranges::adjacent_find(ids) != ids.end()) {
        throw std::invalid_argument("Sketch element IDs must be unique");
    }
    if (elements.nextId <= maximumId) {
        throw std::invalid_argument("Sketch next element ID must exceed existing IDs");
    }
}

void validateSketchConstraints(const SketchElements& elements,
                               const SketchConstraints& constraints) {
    std::uint64_t maximumId = 0;
    std::vector<std::uint64_t> ids;
    ids.reserve(constraints.values.size());
    const auto findElement = [&elements](SketchElementId id) -> const SketchElement& {
        const auto match = std::ranges::find(elements.values, id, &SketchElement::id);
        if (match == elements.values.end())
            throw std::invalid_argument("Sketch constraint references an unknown element");
        return *match;
    };
    const auto requireLine = [&findElement](const SketchGeometryReference& reference) {
        if (!std::holds_alternative<SketchLine>(findElement(reference.element).geometry))
            throw std::invalid_argument("Sketch constraint requires a line");
    };
    const auto requirePoint = [&findElement](const SketchGeometryReference& reference) {
        const SketchGeometry& geometry = findElement(reference.element).geometry;
        if (std::holds_alternative<SketchLine>(geometry)) {
            if (reference.point == SketchPointPosition::center)
                throw std::invalid_argument("Sketch line has no center point");
            return;
        }
        if (std::holds_alternative<SketchCircle>(geometry) &&
            reference.point == SketchPointPosition::center)
            return;
        throw std::invalid_argument("Sketch constraint reference is not a supported point");
    };

    for (const SketchConstraint& constraint : constraints.values) {
        if (constraint.id.value() == 0)
            throw std::invalid_argument("Sketch constraint ID must not be zero");
        maximumId = std::max(maximumId, constraint.id.value());
        ids.push_back(constraint.id.value());
        if (!std::isfinite(constraint.value))
            throw std::invalid_argument("Sketch constraint value must be finite");
        if (constraint.kind != SketchConstraintKind::symmetric && constraint.third.has_value())
            throw std::invalid_argument("Only symmetric constraints accept a third reference");

        if (constraint.kind == SketchConstraintKind::coordinateX ||
            constraint.kind == SketchConstraintKind::coordinateY) {
            requirePoint(constraint.first);
            if (constraint.second.has_value())
                throw std::invalid_argument("Coordinate constraint must be unary");
        } else if (constraint.kind == SketchConstraintKind::horizontal ||
                   constraint.kind == SketchConstraintKind::vertical ||
                   constraint.kind == SketchConstraintKind::distance) {
            requireLine(constraint.first);
            if (constraint.second.has_value())
                throw std::invalid_argument("Unary sketch constraint has a second reference");
            if (constraint.kind == SketchConstraintKind::distance &&
                constraint.value <= minimumLength)
                throw std::invalid_argument("Sketch distance constraint must be positive");
        } else if (constraint.kind == SketchConstraintKind::radius) {
            if (!std::holds_alternative<SketchCircle>(
                    findElement(constraint.first.element).geometry))
                throw std::invalid_argument("Sketch radius constraint requires a circle");
            if (constraint.second.has_value() || constraint.value <= minimumLength)
                throw std::invalid_argument("Sketch radius constraint must be unary and positive");
        } else {
            if (!constraint.second.has_value())
                throw std::invalid_argument("Binary sketch constraint needs a second reference");
            if (constraint.kind == SketchConstraintKind::coincident) {
                requirePoint(constraint.first);
                requirePoint(*constraint.second);
            } else if (constraint.kind == SketchConstraintKind::pointDistance) {
                requirePoint(constraint.first);
                requirePoint(*constraint.second);
                if (constraint.first == *constraint.second || constraint.value <= minimumLength)
                    throw std::invalid_argument(
                        "Point distance constraint needs two points and a positive value");
            } else if (constraint.kind == SketchConstraintKind::pointDistanceX ||
                       constraint.kind == SketchConstraintKind::pointDistanceY) {
                requirePoint(constraint.first);
                requirePoint(*constraint.second);
                if (constraint.first == *constraint.second)
                    throw std::invalid_argument(
                        "Directional point distance constraint needs two points");
            } else if (constraint.kind == SketchConstraintKind::angle) {
                requireLine(constraint.first);
                requireLine(*constraint.second);
                if (constraint.first.element == constraint.second->element ||
                    std::abs(constraint.value) <= minimumLength ||
                    std::abs(constraint.value) >= 180.0)
                    throw std::invalid_argument("Angle constraint needs two different lines and an "
                                                "angle below 180 degrees");
            } else if (constraint.kind == SketchConstraintKind::symmetric) {
                if (!constraint.third.has_value())
                    throw std::invalid_argument("Symmetric constraint needs an axis reference");
                requirePoint(constraint.first);
                requirePoint(*constraint.second);
                requireLine(*constraint.third);
                if (constraint.first == *constraint.second ||
                    constraint.third->element == constraint.first.element ||
                    constraint.third->element == constraint.second->element)
                    throw std::invalid_argument(
                        "Symmetric constraint needs two points and a different axis line");
            } else if (constraint.kind == SketchConstraintKind::pointOnObject) {
                requirePoint(constraint.first);
                const SketchGeometry& target = findElement(constraint.second->element).geometry;
                if (constraint.first.element == constraint.second->element ||
                    (!std::holds_alternative<SketchLine>(target) &&
                     !std::holds_alternative<SketchCircle>(target)))
                    throw std::invalid_argument(
                        "Point-on-object constraint needs a different line or circle");
            } else if (constraint.kind == SketchConstraintKind::midpoint) {
                requirePoint(constraint.first);
                requireLine(*constraint.second);
                if (constraint.first.element == constraint.second->element)
                    throw std::invalid_argument(
                        "Midpoint constraint needs a point and a different line");
            } else if (constraint.kind == SketchConstraintKind::perpendicularBisector) {
                requireLine(constraint.first);
                requireLine(*constraint.second);
                if (constraint.first.element == constraint.second->element)
                    throw std::invalid_argument(
                        "Perpendicular bisector constraint needs two different lines");
            } else if (constraint.kind == SketchConstraintKind::equal) {
                const SketchGeometry& first = findElement(constraint.first.element).geometry;
                const SketchGeometry& second = findElement(constraint.second->element).geometry;
                if (constraint.first.element == constraint.second->element ||
                    first.index() != second.index() ||
                    (!std::holds_alternative<SketchLine>(first) &&
                     !std::holds_alternative<SketchCircle>(first)))
                    throw std::invalid_argument(
                        "Equal constraint needs two different lines or two different circles");
            } else if (constraint.kind == SketchConstraintKind::tangent) {
                const SketchGeometry& first = findElement(constraint.first.element).geometry;
                const SketchGeometry& second = findElement(constraint.second->element).geometry;
                const bool firstLine = std::holds_alternative<SketchLine>(first);
                const bool secondLine = std::holds_alternative<SketchLine>(second);
                const bool firstCircle = std::holds_alternative<SketchCircle>(first);
                const bool secondCircle = std::holds_alternative<SketchCircle>(second);
                if (constraint.first.element == constraint.second->element ||
                    !((firstLine && secondCircle) || (firstCircle && secondLine) ||
                      (firstCircle && secondCircle)))
                    throw std::invalid_argument(
                        "Tangent constraint needs a line and circle or two circles");
            } else {
                requireLine(constraint.first);
                requireLine(*constraint.second);
            }
        }
    }
    std::ranges::sort(ids);
    if (std::ranges::adjacent_find(ids) != ids.end())
        throw std::invalid_argument("Sketch constraint IDs must be unique");
    if (constraints.nextId <= maximumId)
        throw std::invalid_argument("Sketch next constraint ID must exceed existing IDs");
}

} // namespace voelund::document
