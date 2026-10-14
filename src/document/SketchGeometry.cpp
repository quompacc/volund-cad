#include "SketchGeometryRange.hpp"
#include <voelund/document/IdCapacity.hpp>
#include <voelund/document/SketchGeometry.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <numbers>
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
    detail::validateGeometryRange(geometry);
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
    if (const auto* circle = std::get_if<SketchCircle>(&geometry)) {
        if (!finite(circle->center) || !std::isfinite(circle->radius) ||
            circle->radius <= minimumLength)
            throw std::invalid_argument("Sketch circle must have a finite positive radius");
        return;
    }
    if (const auto* ellipse = std::get_if<SketchEllipse>(&geometry)) {
        if (!finite(ellipse->center) || !std::isfinite(ellipse->majorRadius) ||
            !std::isfinite(ellipse->minorRadius) || !std::isfinite(ellipse->rotation) ||
            ellipse->majorRadius <= minimumLength || ellipse->minorRadius <= minimumLength ||
            ellipse->minorRadius > ellipse->majorRadius)
            throw std::invalid_argument("Sketch ellipse must have finite positive radii");
        return;
    }
    if (const auto* point = std::get_if<SketchPointGeometry>(&geometry)) {
        if (!finite(point->position))
            throw std::invalid_argument("Sketch point must be finite");
        return;
    }
    if (const auto* spline = std::get_if<SketchBSpline>(&geometry)) { // S21
        const auto& poles = spline->poles;
        if (poles.size() < minimumBSplinePoles || poles.size() > maximumBSplinePoles ||
            !std::ranges::all_of(poles, [](const SketchPoint& pole) { return finite(pole); }) ||
            squaredDistance(poles.front(), poles.back()) <= minimumLength * minimumLength)
            throw std::invalid_argument(
                "Sketch B-spline needs 4 to 64 finite control points and two distinct ends");
        return;
    }
    const SketchArc& arc = std::get<SketchArc>(geometry);
    const double sweep = arc.endAngle - arc.startAngle;
    if (!finite(arc.center) || !std::isfinite(arc.radius) || arc.radius <= minimumLength ||
        !std::isfinite(arc.startAngle) || !std::isfinite(arc.endAngle) ||
        sweep <= kernel::angularTolerance.radians || sweep >= 2.0 * std::numbers::pi)
        throw std::invalid_argument("Sketch arc must have a finite center, radius and sweep");
}

} // namespace

void validateSketchElements(const SketchElements& elements) {
    if (!hasIdCapacity(elements.nextId, 0))
        throw std::invalid_argument("Sketch next element ID is exhausted or zero");
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
        if (std::holds_alternative<SketchPointGeometry>(element.geometry) && !element.construction)
            throw std::invalid_argument("A sketch point is construction geometry");
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
    if (!hasIdCapacity(constraints.nextId, 0))
        throw std::invalid_argument("Sketch next constraint ID is exhausted or zero");
    std::uint64_t maximumId = 0;
    std::vector<std::uint64_t> ids;
    ids.reserve(constraints.values.size());
    std::map<std::uint64_t, const SketchElement*> byId;
    for (const auto& element : elements.values)
        byId.emplace(element.id.value(), &element);
    const auto findElement = [&byId](SketchElementId id) -> const SketchElement& {
        const auto match = byId.find(id.value());
        if (match == byId.end())
            throw std::invalid_argument("Sketch constraint references an unknown element");
        return *match->second;
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
        if ((std::holds_alternative<SketchEllipse>(geometry) ||
             std::holds_alternative<SketchPointGeometry>(geometry)) &&
            reference.point == SketchPointPosition::center)
            return;
        if (std::holds_alternative<SketchArc>(geometry))
            return;
        if (std::holds_alternative<SketchBSpline>(geometry) && // S21: its two ends
            reference.point != SketchPointPosition::center)
            return;
        throw std::invalid_argument("Sketch constraint reference is not a supported point");
    };

    for (const SketchConstraint& constraint : constraints.values) {
        if (constraint.id.value() == 0)
            throw std::invalid_argument("Sketch constraint ID must not be zero");
        maximumId = std::max(maximumId, constraint.id.value());
        ids.push_back(constraint.id.value());
        const auto kind = constraint.kind;
        if (constraint.projectedEdge) {
            validateProjectedEdgeReference(*constraint.projectedEdge);
            if ((kind != SketchConstraintKind::coordinateX &&
                 kind != SketchConstraintKind::coordinateY) ||
                constraint.second || constraint.third || !constraint.auxiliaryValues.empty())
                throw std::invalid_argument("Projected edges require a unary coordinate constraint "
                                            "without legacy attachment");
        }
        if (constraint.reference &&
            ((kind != SketchConstraintKind::distance && kind != SketchConstraintKind::radius &&
              kind != SketchConstraintKind::diameter &&
              kind != SketchConstraintKind::pointDistance &&
              kind != SketchConstraintKind::pointLineDistance &&
              kind != SketchConstraintKind::pointDistanceX &&
              kind != SketchConstraintKind::pointDistanceY &&
              kind != SketchConstraintKind::angle) ||
             constraint.parameter || constraint.projectedEdge))
            throw std::invalid_argument("Only an unbound dimension can be a reference");
        if ((kind == SketchConstraintKind::coordinateX ||
             kind == SketchConstraintKind::coordinateY ||
             kind == SketchConstraintKind::pointDistanceX ||
             kind == SketchConstraintKind::pointDistanceY) &&
            !kernel::modelCoordinate(constraint.value))
            throw std::invalid_argument(kernel::modelRangeDiagnostic);
        if ((kind == SketchConstraintKind::distance || kind == SketchConstraintKind::radius ||
             kind == SketchConstraintKind::diameter ||
             kind == SketchConstraintKind::pointDistance ||
             kind == SketchConstraintKind::pointLineDistance) &&
            !constraint.reference && !kernel::modelLength(constraint.value))
            throw std::invalid_argument(kernel::modelRangeDiagnostic);
        if (!std::isfinite(constraint.value))
            throw std::invalid_argument("Sketch constraint value must be finite");
        if ((constraint.kind == SketchConstraintKind::angle && constraint.value != 0.0) ||
            (constraint.kind != SketchConstraintKind::angle &&
             constraint.angleValue.value() != 0.0))
            throw std::invalid_argument("Sketch constraint value uses the wrong unit field");
        if (std::ranges::any_of(constraint.auxiliaryValues,
                                [](double value) { return !std::isfinite(value); }))
            throw std::invalid_argument("Sketch constraint auxiliary values must be finite");
        const bool supportCoordinate = (constraint.kind == SketchConstraintKind::coordinateX ||
                                        constraint.kind == SketchConstraintKind::coordinateY) &&
                                       constraint.auxiliaryValues.size() == 2;
        if (constraint.kind != SketchConstraintKind::fixed && !supportCoordinate &&
            !constraint.auxiliaryValues.empty())
            throw std::invalid_argument(
                "Only fixed and support-coordinate constraints accept auxiliary values");
        if (constraint.kind != SketchConstraintKind::symmetric && constraint.third.has_value())
            throw std::invalid_argument("Only symmetric constraints accept a third reference");

        // S18: a reference shows what the geometry measures, also zero.
        const bool positive = constraint.reference || constraint.value > minimumLength;
        if (constraint.kind == SketchConstraintKind::fixed) {
            const SketchGeometry& geometry = findElement(constraint.first.element).geometry;
            const bool line = std::holds_alternative<SketchLine>(geometry);
            const bool circle = std::holds_alternative<SketchCircle>(geometry);
            const std::size_t expectedValues = line ? 3 : 2;
            if ((!line && !circle) || constraint.second.has_value() ||
                constraint.auxiliaryValues.size() != expectedValues)
                throw std::invalid_argument(
                    "Fixed constraint needs one line or circle and its complete coordinates");
            if (line)
                validateGeometry(
                    SketchLine{{constraint.value, constraint.auxiliaryValues[0]},
                               {constraint.auxiliaryValues[1], constraint.auxiliaryValues[2]}});
            else
                validateGeometry(SketchCircle{{constraint.value, constraint.auxiliaryValues[0]},
                                              constraint.auxiliaryValues[1]});
        } else if (constraint.kind == SketchConstraintKind::coordinateX ||
                   constraint.kind == SketchConstraintKind::coordinateY) {
            requirePoint(constraint.first);
            if (constraint.second.has_value())
                throw std::invalid_argument("Coordinate constraint must be unary");
            if (supportCoordinate &&
                (constraint.auxiliaryValues[0] < 1.0 ||
                 std::floor(constraint.auxiliaryValues[0]) != constraint.auxiliaryValues[0] ||
                 constraint.auxiliaryValues[1] < 0.0 || constraint.auxiliaryValues[1] > 1.0))
                throw std::invalid_argument("Support-coordinate attachment is invalid");
        } else if (constraint.kind == SketchConstraintKind::horizontal ||
                   constraint.kind == SketchConstraintKind::vertical ||
                   constraint.kind == SketchConstraintKind::distance) {
            requireLine(constraint.first);
            if (constraint.second.has_value())
                throw std::invalid_argument("Unary sketch constraint has a second reference");
            if (constraint.kind == SketchConstraintKind::distance &&
                !positive)
                throw std::invalid_argument("Sketch distance constraint must be positive");
        } else if (constraint.kind == SketchConstraintKind::radius ||
                   constraint.kind == SketchConstraintKind::diameter) {
            const SketchGeometry& geometry = findElement(constraint.first.element).geometry;
            if (!std::holds_alternative<SketchCircle>(geometry) &&
                !std::holds_alternative<SketchArc>(geometry))
                throw std::invalid_argument("Sketch radial constraint requires a circle or arc");
            if (constraint.second.has_value() || !positive)
                throw std::invalid_argument("Sketch radial constraint must be unary and positive");
        } else {
            if (!constraint.second.has_value())
                throw std::invalid_argument("Binary sketch constraint needs a second reference");
            if (constraint.kind == SketchConstraintKind::coincident) {
                requirePoint(constraint.first);
                requirePoint(*constraint.second);
            } else if (constraint.kind == SketchConstraintKind::pointDistance) {
                requirePoint(constraint.first);
                requirePoint(*constraint.second);
                if (constraint.first == *constraint.second || !positive)
                    throw std::invalid_argument(
                        "Point distance constraint needs two points and a positive value");
            } else if (constraint.kind == SketchConstraintKind::pointLineDistance) {
                requirePoint(constraint.first);
                requireLine(*constraint.second);
                if (constraint.first.element == constraint.second->element ||
                    !positive)
                    throw std::invalid_argument(
                        "Point-line distance constraint needs a point, a different line and a "
                        "positive value");
            } else if (constraint.kind == SketchConstraintKind::pointDistanceX ||
                       constraint.kind == SketchConstraintKind::pointDistanceY) {
                requirePoint(constraint.first);
                requirePoint(*constraint.second);
                if (constraint.first == *constraint.second)
                    throw std::invalid_argument(
                        "Directional point distance constraint needs two points");
            } else if (constraint.kind == SketchConstraintKind::horizontalPoints ||
                       constraint.kind == SketchConstraintKind::verticalPoints) {
                requirePoint(constraint.first);
                requirePoint(*constraint.second);
                if (constraint.first == *constraint.second)
                    throw std::invalid_argument(
                        "Point alignment constraint needs two different points");
            } else if (constraint.kind == SketchConstraintKind::angle) {
                requireLine(constraint.first);
                requireLine(*constraint.second);
                if (constraint.first.element == constraint.second->element ||
                    constraint.value != 0.0 ||
                    (!constraint.reference && // S18: a reference shows also 0 or 180
                     (std::abs(constraint.angleValue.value()) <=
                          kernel::toRadians(kernel::Degrees(minimumLength)).value() ||
                      std::abs(constraint.angleValue.value()) >= std::numbers::pi)))
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
            } else if (constraint.kind == SketchConstraintKind::collinear) {
                requireLine(constraint.first);
                requireLine(*constraint.second);
                if (constraint.first.element == constraint.second->element)
                    throw std::invalid_argument("Collinear constraint needs two different lines");
            } else if (constraint.kind == SketchConstraintKind::concentric) {
                const SketchGeometry& first = findElement(constraint.first.element).geometry;
                const SketchGeometry& second = findElement(constraint.second->element).geometry;
                if (constraint.first.element == constraint.second->element ||
                    (!std::holds_alternative<SketchCircle>(first) &&
                     !std::holds_alternative<SketchArc>(first)) ||
                    (!std::holds_alternative<SketchCircle>(second) &&
                     !std::holds_alternative<SketchArc>(second)))
                    throw std::invalid_argument(
                        "Concentric constraint needs two different circles");
            } else if (constraint.kind == SketchConstraintKind::equal) {
                const SketchGeometry& first = findElement(constraint.first.element).geometry;
                const SketchGeometry& second = findElement(constraint.second->element).geometry;
                if (constraint.first.element == constraint.second->element ||
                    !((std::holds_alternative<SketchLine>(first) &&
                       std::holds_alternative<SketchLine>(second)) ||
                      ((std::holds_alternative<SketchCircle>(first) ||
                        std::holds_alternative<SketchArc>(first)) &&
                       (std::holds_alternative<SketchCircle>(second) ||
                        std::holds_alternative<SketchArc>(second)))))
                    throw std::invalid_argument(
                        "Equal constraint needs two different lines or two different circles");
            } else if (constraint.kind == SketchConstraintKind::tangent) {
                const SketchGeometry& first = findElement(constraint.first.element).geometry;
                const SketchGeometry& second = findElement(constraint.second->element).geometry;
                const bool firstLine = std::holds_alternative<SketchLine>(first);
                const bool secondLine = std::holds_alternative<SketchLine>(second);
                const bool firstCircle = std::holds_alternative<SketchCircle>(first);
                const bool secondCircle = std::holds_alternative<SketchCircle>(second);
                const bool firstArc = std::holds_alternative<SketchArc>(first);
                const bool secondArc = std::holds_alternative<SketchArc>(second);
                if (constraint.first.element == constraint.second->element ||
                    !((firstLine && (secondCircle || secondArc)) ||
                      (secondLine && (firstCircle || firstArc)) ||
                      ((firstCircle || firstArc) && (secondCircle || secondArc))))
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
