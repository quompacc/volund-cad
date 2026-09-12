#include <voelund/features/SketchConstraintSolver.hpp>

#include <GCS.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace voelund::features {
namespace {

struct LineState final {
    document::SketchElement* element;
    double firstX;
    double firstY;
    double secondX;
    double secondY;
    GCS::Point first;
    GCS::Point second;
    GCS::Line line;

    explicit LineState(document::SketchElement& source)
        : element(&source), firstX(std::get<document::SketchLine>(source.geometry).start.x),
          firstY(std::get<document::SketchLine>(source.geometry).start.y),
          secondX(std::get<document::SketchLine>(source.geometry).end.x),
          secondY(std::get<document::SketchLine>(source.geometry).end.y), first{&firstX, &firstY},
          second{&secondX, &secondY} {
        line.p1 = first;
        line.p2 = second;
    }

    void appendUnknowns(GCS::VEC_pD& unknowns) {
        unknowns.insert(unknowns.end(), {&firstX, &firstY, &secondX, &secondY});
    }

    void writeBack() const {
        auto& target = std::get<document::SketchLine>(element->geometry);
        target.start = {firstX, firstY};
        target.end = {secondX, secondY};
    }
};

struct CircleState final {
    document::SketchElement* element;
    double centerX;
    double centerY;
    double radius;
    GCS::Circle circle;

    explicit CircleState(document::SketchElement& source)
        : element(&source), centerX(std::get<document::SketchCircle>(source.geometry).center.x),
          centerY(std::get<document::SketchCircle>(source.geometry).center.y),
          radius(std::get<document::SketchCircle>(source.geometry).radius) {
        circle.center = {&centerX, &centerY};
        circle.rad = &radius;
    }

    void appendUnknowns(GCS::VEC_pD& unknowns) {
        unknowns.insert(unknowns.end(), {&centerX, &centerY, &radius});
    }

    void writeBack() const {
        auto& target = std::get<document::SketchCircle>(element->geometry);
        target.center = {centerX, centerY};
        target.radius = radius;
    }
};

struct ArcState final {
    document::SketchElement* element;
    double centerX;
    double centerY;
    double radius;
    double startX;
    double startY;
    double endX;
    double endY;
    double startAngle;
    double endAngle;
    GCS::Arc arc;

    explicit ArcState(document::SketchElement& source)
        : element(&source), centerX(std::get<document::SketchArc>(source.geometry).center.x),
          centerY(std::get<document::SketchArc>(source.geometry).center.y),
          radius(std::get<document::SketchArc>(source.geometry).radius),
          startAngle(std::get<document::SketchArc>(source.geometry).startAngle),
          endAngle(std::get<document::SketchArc>(source.geometry).endAngle) {
        startX = centerX + radius * std::cos(startAngle);
        startY = centerY + radius * std::sin(startAngle);
        endX = centerX + radius * std::cos(endAngle);
        endY = centerY + radius * std::sin(endAngle);
        arc.center = {&centerX, &centerY};
        arc.rad = &radius;
        arc.start = {&startX, &startY};
        arc.end = {&endX, &endY};
        arc.startAngle = &startAngle;
        arc.endAngle = &endAngle;
    }

    void appendUnknowns(GCS::VEC_pD& unknowns) {
        unknowns.insert(unknowns.end(), {&centerX, &centerY, &radius, &startX, &startY, &endX,
                                         &endY, &startAngle, &endAngle});
    }

    void writeBack() const {
        auto& target = std::get<document::SketchArc>(element->geometry);
        target.center = {centerX, centerY};
        target.radius = radius;
        target.startAngle = std::remainder(startAngle, 2.0 * std::numbers::pi);
        double sweep = std::fmod(endAngle - startAngle, 2.0 * std::numbers::pi);
        if (sweep <= 1.0e-9)
            sweep += 2.0 * std::numbers::pi;
        target.endAngle = target.startAngle + sweep;
    }
};

struct EllipseState final {
    document::SketchElement* element;
    double centerX;
    double centerY;
    double focusX;
    double focusY;
    double minorRadius;
    GCS::Ellipse ellipse;

    explicit EllipseState(document::SketchElement& source)
        : element(&source), centerX(std::get<document::SketchEllipse>(source.geometry).center.x),
          centerY(std::get<document::SketchEllipse>(source.geometry).center.y),
          minorRadius(std::get<document::SketchEllipse>(source.geometry).minorRadius) {
        const auto& value = std::get<document::SketchEllipse>(source.geometry);
        const double focalDistance =
            std::sqrt(value.majorRadius * value.majorRadius - value.minorRadius * value.minorRadius);
        focusX = centerX + focalDistance * std::cos(value.rotation);
        focusY = centerY + focalDistance * std::sin(value.rotation);
        ellipse.center = {&centerX, &centerY};
        ellipse.focus1 = {&focusX, &focusY};
        ellipse.radmin = &minorRadius;
    }

    void appendUnknowns(GCS::VEC_pD& unknowns) {
        unknowns.insert(unknowns.end(), {&centerX, &centerY, &focusX, &focusY, &minorRadius});
    }

    void writeBack() const {
        auto& target = std::get<document::SketchEllipse>(element->geometry);
        target.center = {centerX, centerY};
        target.minorRadius = std::abs(minorRadius);
        target.majorRadius =
            std::hypot(std::hypot(focusX - centerX, focusY - centerY), target.minorRadius);
        target.rotation = std::atan2(focusY - centerY, focusX - centerX);
    }
};

using LineMap = std::unordered_map<std::uint64_t, LineState*>;
using CircleMap = std::unordered_map<std::uint64_t, CircleState*>;
using ArcMap = std::unordered_map<std::uint64_t, ArcState*>;
using PointMap = std::unordered_map<std::uint64_t, GCS::Point*>;

GCS::Point& point(const document::SketchGeometryReference& reference, const LineMap& lines,
                  const CircleMap& circles, const ArcMap& arcs, const PointMap& centers) {
    static_cast<void>(circles);
    const auto id = reference.element.value();
    if (arcs.contains(id)) {
        ArcState& arc = *arcs.at(id);
        if (reference.point == document::SketchPointPosition::center)
            return arc.arc.center;
        return reference.point == document::SketchPointPosition::start ? arc.arc.start
                                                                       : arc.arc.end;
    }
    if (reference.point == document::SketchPointPosition::center)
        return *centers.at(id);
    LineState& line = *lines.at(id);
    return reference.point == document::SketchPointPosition::start ? line.first : line.second;
}

} // namespace

SketchSolveResult solveSketchConstraints(document::SketchParameters& sketch) {
    document::validateSketchParameters(sketch);
    const document::SketchElements originalElements = sketch.elements;
    std::vector<std::unique_ptr<LineState>> lineStates;
    std::vector<std::unique_ptr<CircleState>> circleStates;
    std::vector<std::unique_ptr<ArcState>> arcStates;
    std::vector<std::unique_ptr<EllipseState>> ellipseStates;
    LineMap lines;
    CircleMap circles;
    ArcMap arcs;
    PointMap centers;
    GCS::VEC_pD unknowns;
    for (document::SketchElement& element : sketch.elements.values) {
        if (std::holds_alternative<document::SketchLine>(element.geometry)) {
            auto state = std::make_unique<LineState>(element);
            state->appendUnknowns(unknowns);
            lines.emplace(element.id.value(), state.get());
            lineStates.push_back(std::move(state));
        } else if (std::holds_alternative<document::SketchCircle>(element.geometry)) {
            auto state = std::make_unique<CircleState>(element);
            state->appendUnknowns(unknowns);
            circles.emplace(element.id.value(), state.get());
            centers.emplace(element.id.value(), &state->circle.center);
            circleStates.push_back(std::move(state));
        } else if (std::holds_alternative<document::SketchArc>(element.geometry)) {
            auto state = std::make_unique<ArcState>(element);
            state->appendUnknowns(unknowns);
            arcs.emplace(element.id.value(), state.get());
            centers.emplace(element.id.value(), &state->arc.center);
            arcStates.push_back(std::move(state));
        } else if (std::holds_alternative<document::SketchEllipse>(element.geometry)) {
            auto state = std::make_unique<EllipseState>(element);
            state->appendUnknowns(unknowns);
            centers.emplace(element.id.value(), &state->ellipse.center);
            ellipseStates.push_back(std::move(state));
        }
    }

    GCS::System system;
    for (const auto& arc : arcStates)
        static_cast<void>(system.addConstraintArcRules(arc->arc));
    std::vector<double> values;
    values.reserve(sketch.constraints.values.size());
    std::vector<std::array<double, 4>> fixedValues;
    fixedValues.reserve(sketch.constraints.values.size());
    std::vector<document::SketchConstraintId> tags;
    tags.reserve(sketch.constraints.values.size());
    for (const document::SketchConstraint& constraint : sketch.constraints.values) {
        values.push_back(constraint.kind == document::SketchConstraintKind::angle
                             ? constraint.value * std::numbers::pi / 180.0
                             : constraint.value);
        tags.push_back(constraint.id);
        const int tag = static_cast<int>(tags.size());
        LineState* firstLine = lines.contains(constraint.first.element.value())
                                   ? lines.at(constraint.first.element.value())
                                   : nullptr;
        if (constraint.kind == document::SketchConstraintKind::horizontal)
            static_cast<void>(system.addConstraintHorizontal(firstLine->line, tag));
        else if (constraint.kind == document::SketchConstraintKind::vertical)
            static_cast<void>(system.addConstraintVertical(firstLine->line, tag));
        else if (constraint.kind == document::SketchConstraintKind::distance)
            static_cast<void>(system.addConstraintP2PDistance(firstLine->first, firstLine->second,
                                                              &values.back(), tag));
        else if (constraint.kind == document::SketchConstraintKind::radius) {
            const auto id = constraint.first.element.value();
            if (arcs.contains(id))
                static_cast<void>(
                    system.addConstraintArcRadius(arcs.at(id)->arc, &values.back(), tag));
            else
                static_cast<void>(
                    system.addConstraintCircleRadius(circles.at(id)->circle, &values.back(), tag));
        } else if (constraint.kind == document::SketchConstraintKind::diameter) {
            const auto id = constraint.first.element.value();
            if (arcs.contains(id))
                static_cast<void>(
                    system.addConstraintArcDiameter(arcs.at(id)->arc, &values.back(), tag));
            else
                static_cast<void>(system.addConstraintCircleDiameter(circles.at(id)->circle,
                                                                     &values.back(), tag));
        } else if (constraint.kind == document::SketchConstraintKind::coincident)
            static_cast<void>(system.addConstraintP2PCoincident(
                point(constraint.first, lines, circles, arcs, centers),
                point(*constraint.second, lines, circles, arcs, centers), tag));
        else if (constraint.kind == document::SketchConstraintKind::horizontalPoints)
            static_cast<void>(system.addConstraintHorizontal(
                point(constraint.first, lines, circles, arcs, centers),
                point(*constraint.second, lines, circles, arcs, centers), tag));
        else if (constraint.kind == document::SketchConstraintKind::verticalPoints)
            static_cast<void>(
                system.addConstraintVertical(point(constraint.first, lines, circles, arcs, centers),
                                             point(*constraint.second, lines, circles, arcs, centers),
                                             tag));
        else if (constraint.kind == document::SketchConstraintKind::concentric)
            static_cast<void>(system.addConstraintP2PCoincident(
                *centers.at(constraint.first.element.value()),
                *centers.at(constraint.second->element.value()), tag));
        else if (constraint.kind == document::SketchConstraintKind::pointDistance)
            static_cast<void>(system.addConstraintP2PDistance(
                point(constraint.first, lines, circles, arcs, centers),
                point(*constraint.second, lines, circles, arcs, centers), &values.back(), tag));
        else if (constraint.kind == document::SketchConstraintKind::pointLineDistance)
            static_cast<void>(system.addConstraintP2LDistance(
                point(constraint.first, lines, circles, arcs, centers),
                lines.at(constraint.second->element.value())->line, &values.back(), tag));
        else if (constraint.kind == document::SketchConstraintKind::pointDistanceX ||
                 constraint.kind == document::SketchConstraintKind::pointDistanceY) {
            GCS::Point& firstPoint = point(constraint.first, lines, circles, arcs, centers);
            GCS::Point& secondPoint = point(*constraint.second, lines, circles, arcs, centers);
            double* firstCoordinate =
                constraint.kind == document::SketchConstraintKind::pointDistanceX ? firstPoint.x
                                                                                  : firstPoint.y;
            double* secondCoordinate =
                constraint.kind == document::SketchConstraintKind::pointDistanceX ? secondPoint.x
                                                                                  : secondPoint.y;
            static_cast<void>(system.addConstraintDifference(firstCoordinate, secondCoordinate,
                                                             &values.back(), tag));
        } else if (constraint.kind == document::SketchConstraintKind::angle)
            static_cast<void>(system.addConstraintL2LAngle(
                firstLine->line, lines.at(constraint.second->element.value())->line, &values.back(),
                tag));
        else if (constraint.kind == document::SketchConstraintKind::symmetric)
            static_cast<void>(system.addConstraintP2PSymmetric(
                point(constraint.first, lines, circles, arcs, centers),
                point(*constraint.second, lines, circles, arcs, centers),
                lines.at(constraint.third->element.value())->line, tag));
        else if (constraint.kind == document::SketchConstraintKind::fixed) {
            fixedValues.push_back(
                {constraint.value, constraint.auxiliaryValues[0], constraint.auxiliaryValues[1],
                 constraint.auxiliaryValues.size() == 3 ? constraint.auxiliaryValues[2] : 0.0});
            auto& fixed = fixedValues.back();
            if (firstLine) {
                static_cast<void>(
                    system.addConstraintCoordinateX(firstLine->first, &fixed[0], tag));
                static_cast<void>(
                    system.addConstraintCoordinateY(firstLine->first, &fixed[1], tag));
                static_cast<void>(
                    system.addConstraintCoordinateX(firstLine->second, &fixed[2], tag));
                static_cast<void>(
                    system.addConstraintCoordinateY(firstLine->second, &fixed[3], tag));
            } else {
                auto& circle = circles.at(constraint.first.element.value())->circle;
                static_cast<void>(system.addConstraintCoordinateX(circle.center, &fixed[0], tag));
                static_cast<void>(system.addConstraintCoordinateY(circle.center, &fixed[1], tag));
                static_cast<void>(system.addConstraintCircleRadius(circle, &fixed[2], tag));
            }
        } else if (constraint.kind == document::SketchConstraintKind::coordinateX)
            static_cast<void>(system.addConstraintCoordinateX(
                point(constraint.first, lines, circles, arcs, centers), &values.back(), tag));
        else if (constraint.kind == document::SketchConstraintKind::coordinateY)
            static_cast<void>(system.addConstraintCoordinateY(
                point(constraint.first, lines, circles, arcs, centers), &values.back(), tag));
        else if (constraint.kind == document::SketchConstraintKind::pointOnObject) {
            GCS::Point& source = point(constraint.first, lines, circles, arcs, centers);
            const auto targetId = constraint.second->element.value();
            if (lines.contains(targetId))
                static_cast<void>(
                    system.addConstraintPointOnLine(source, lines.at(targetId)->line, tag));
            else
                static_cast<void>(
                    system.addConstraintPointOnCircle(source, circles.at(targetId)->circle, tag));
        } else if (constraint.kind == document::SketchConstraintKind::midpoint) {
            GCS::Point& source = point(constraint.first, lines, circles, arcs, centers);
            GCS::Line& target = lines.at(constraint.second->element.value())->line;
            static_cast<void>(system.addConstraintPointOnLine(source, target, tag));
            static_cast<void>(system.addConstraintPointOnPerpBisector(source, target, tag));
        } else if (constraint.kind == document::SketchConstraintKind::perpendicularBisector) {
            GCS::Line& target = lines.at(constraint.second->element.value())->line;
            static_cast<void>(system.addConstraintPerpendicular(firstLine->line, target, tag));
            static_cast<void>(
                system.addConstraintPointOnPerpBisector(firstLine->first, target, tag));
        } else if (constraint.kind == document::SketchConstraintKind::collinear) {
            GCS::Line& target = lines.at(constraint.second->element.value())->line;
            static_cast<void>(system.addConstraintParallel(firstLine->line, target, tag));
            static_cast<void>(system.addConstraintPointOnLine(firstLine->first, target, tag));
        } else if (constraint.kind == document::SketchConstraintKind::equal) {
            const auto secondId = constraint.second->element.value();
            if (firstLine)
                static_cast<void>(system.addConstraintEqualLength(firstLine->line,
                                                                  lines.at(secondId)->line, tag));
            else if (arcs.contains(constraint.first.element.value()) && arcs.contains(secondId))
                static_cast<void>(system.addConstraintEqualRadius(
                    arcs.at(constraint.first.element.value())->arc, arcs.at(secondId)->arc, tag));
            else if (arcs.contains(constraint.first.element.value()))
                static_cast<void>(system.addConstraintEqualRadius(
                    circles.at(secondId)->circle, arcs.at(constraint.first.element.value())->arc,
                    tag));
            else if (arcs.contains(secondId))
                static_cast<void>(system.addConstraintEqualRadius(
                    circles.at(constraint.first.element.value())->circle, arcs.at(secondId)->arc,
                    tag));
            else
                static_cast<void>(system.addConstraintEqualRadius(
                    circles.at(constraint.first.element.value())->circle,
                    circles.at(secondId)->circle, tag));
        } else if (constraint.kind == document::SketchConstraintKind::tangent) {
            const auto firstId = constraint.first.element.value();
            const auto secondId = constraint.second->element.value();
            if (lines.contains(firstId))
                static_cast<void>(arcs.contains(secondId)
                                      ? system.addConstraintTangent(lines.at(firstId)->line,
                                                                    arcs.at(secondId)->arc, tag)
                                      : system.addConstraintTangent(lines.at(firstId)->line,
                                                                    circles.at(secondId)->circle,
                                                                    tag));
            else if (lines.contains(secondId))
                static_cast<void>(arcs.contains(firstId)
                                      ? system.addConstraintTangent(lines.at(secondId)->line,
                                                                    arcs.at(firstId)->arc, tag)
                                      : system.addConstraintTangent(lines.at(secondId)->line,
                                                                    circles.at(firstId)->circle,
                                                                    tag));
            else if (arcs.contains(firstId) && arcs.contains(secondId))
                static_cast<void>(system.addConstraintTangent(arcs.at(firstId)->arc,
                                                              arcs.at(secondId)->arc, tag));
            else if (arcs.contains(firstId))
                static_cast<void>(system.addConstraintTangent(circles.at(secondId)->circle,
                                                              arcs.at(firstId)->arc, tag));
            else if (arcs.contains(secondId))
                static_cast<void>(system.addConstraintTangent(circles.at(firstId)->circle,
                                                              arcs.at(secondId)->arc, tag));
            else
                static_cast<void>(system.addConstraintTangent(circles.at(firstId)->circle,
                                                              circles.at(secondId)->circle, tag));
        } else {
            LineState& secondLine = *lines.at(constraint.second->element.value());
            if (constraint.kind == document::SketchConstraintKind::parallel)
                static_cast<void>(
                    system.addConstraintParallel(firstLine->line, secondLine.line, tag));
            else
                static_cast<void>(
                    system.addConstraintPerpendicular(firstLine->line, secondLine.line, tag));
        }
    }

    system.declareUnknowns(unknowns);
    static_cast<void>(system.diagnose());
    GCS::VEC_I conflictingTags;
    system.getConflicting(conflictingTags);
    SketchSolveResult result;
    result.degreesOfFreedom = system.dofsNumber();
    for (int tag : conflictingTags) {
        if (tag > 0 && static_cast<std::size_t>(tag) <= tags.size())
            result.conflictingConstraints.push_back(tags[static_cast<std::size_t>(tag - 1)]);
    }
    if (!result.conflictingConstraints.empty()) {
        result.status = SketchSolveStatus::conflicting;
        return result;
    }

    const int status = system.solve(unknowns);
    if (status != GCS::Success && status != GCS::Converged)
        return result;
    system.applySolution();
    for (const auto& line : lineStates)
        line->writeBack();
    for (const auto& circle : circleStates)
        circle->writeBack();
    for (const auto& arc : arcStates)
        arc->writeBack();
    for (const auto& ellipse : ellipseStates)
        ellipse->writeBack();
    try {
        document::validateSketchParameters(sketch);
    } catch (const std::invalid_argument&) {
        sketch.elements = originalElements;
        return result;
    }
    result.status = SketchSolveStatus::success;
    return result;
}

} // namespace voelund::features
