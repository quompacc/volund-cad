// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/features/SketchConstraintSolver.hpp>
#include <voelund/kernel/Angle.hpp>

#include "SketchSolverStates.hpp"

#include <GCS.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <numbers>
#include <optional>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace voelund::features {
namespace {
using namespace detail;

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
    // S21: the ends of a B-spline.
    if (const auto end = centers.find(
            bsplineEndKey(id, reference.point == document::SketchPointPosition::start));
        end != centers.end())
        return *end->second;
    LineState& line = *lines.at(id);
    return reference.point == document::SketchPointPosition::start ? line.first : line.second;
}

// S06: the end point a coincident relation shares between two lines or arcs, as a reference
// to `second`. Tangency there is a tangent joint: as a tangent to the full circle on top of the
// coincidence it is degenerate at the joint, and the solver counts it as redundant.
std::optional<document::SketchGeometryReference>
sharedEnd(const document::SketchConstraints& constraints, std::uint64_t first,
          std::uint64_t second) {
    const auto isEnd = [](document::SketchPointPosition position) {
        return position != document::SketchPointPosition::center;
    };
    for (const auto& constraint : constraints.values) {
        if (constraint.kind != document::SketchConstraintKind::coincident ||
            !isEnd(constraint.first.point) || !isEnd(constraint.second->point))
            continue;
        const auto a = constraint.first.element.value();
        const auto b = constraint.second->element.value();
        if (a == first && b == second)
            return *constraint.second;
        if (a == second && b == first)
            return constraint.first;
    }
    return std::nullopt;
}

} // namespace

SketchSolveResult solveSketchConstraints(document::SketchParameters& sketch) {
    document::validateSketchParameters(sketch);
    const document::SketchElements originalElements = sketch.elements;
    // Reserved once: states point into themselves and must never be relocated.
    std::vector<ElementState> states;
    states.reserve(sketch.elements.values.size());
    std::vector<ArcState*> arcStates;
    LineMap lines;
    CircleMap circles;
    ArcMap arcs;
    PointMap centers;
    GCS::VEC_pD unknowns;
    // Unknowns of each element, to tell free from fully determined elements.
    std::vector<std::pair<document::SketchElementId, std::size_t>> unknownStarts;
    for (document::SketchElement& element : sketch.elements.values) {
        // S12: pattern instances are derived after the solve, never unknowns.
        if (document::isSketchPatternInstance(sketch.patterns, element.id))
            continue;
        unknownStarts.emplace_back(element.id, unknowns.size());
        // S17: a projected edge is fixed; its parameters are constants for the solver.
        GCS::VEC_pD ignored;
        auto& into =
            std::ranges::find(sketch.projections, element.id,
                              &document::SketchProjection::element) != sketch.projections.end()
                ? ignored
                : unknowns;
        if (std::holds_alternative<document::SketchLine>(element.geometry)) {
            auto& state = states.emplace_back(std::in_place_type<LineState>, element);
            auto& line = std::get<LineState>(state);
            line.appendUnknowns(into);
            lines.emplace(element.id.value(), &line);
        } else if (std::holds_alternative<document::SketchCircle>(element.geometry)) {
            auto& state = states.emplace_back(std::in_place_type<CircleState>, element);
            auto& circle = std::get<CircleState>(state);
            circle.appendUnknowns(into);
            circles.emplace(element.id.value(), &circle);
            centers.emplace(element.id.value(), &circle.circle.center);
        } else if (std::holds_alternative<document::SketchArc>(element.geometry)) {
            auto& state = states.emplace_back(std::in_place_type<ArcState>, element);
            auto& arc = std::get<ArcState>(state);
            arc.appendUnknowns(into);
            arcs.emplace(element.id.value(), &arc);
            centers.emplace(element.id.value(), &arc.arc.center);
            if (&into == &unknowns)
                arcStates.push_back(&arc); // a fixed arc already keeps its rules
        } else if (std::holds_alternative<document::SketchEllipse>(element.geometry)) {
            auto& state = states.emplace_back(std::in_place_type<EllipseState>, element);
            auto& ellipse = std::get<EllipseState>(state);
            ellipse.appendUnknowns(into);
            centers.emplace(element.id.value(), &ellipse.ellipse.center);
        } else if (std::holds_alternative<document::SketchBSpline>(element.geometry)) {
            auto& state = states.emplace_back(std::in_place_type<BSplineState>, element);
            auto& spline = std::get<BSplineState>(state);
            spline.appendUnknowns(into);
            centers.emplace(bsplineEndKey(element.id.value(), true), &spline.poles.front());
            centers.emplace(bsplineEndKey(element.id.value(), false), &spline.poles.back());
        } else if (std::holds_alternative<document::SketchPointGeometry>(element.geometry)) {
            auto& state = states.emplace_back(std::in_place_type<PointState>, element);
            auto& point = std::get<PointState>(state);
            point.appendUnknowns(into);
            centers.emplace(element.id.value(), &point.point);
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
    // Reserved once: the solver keeps pointers to the angles of tangent joints.
    std::vector<double> jointAngles;
    jointAngles.reserve(sketch.constraints.values.size());
    for (const document::SketchConstraint& constraint : sketch.constraints.values) {
        values.push_back(constraint.kind == document::SketchConstraintKind::angle
                             ? constraint.angleValue.value()
                             : constraint.value);
        tags.push_back(constraint.id);
        const int tag = static_cast<int>(tags.size());
        if (constraint.reference) // S18: measured after the solve, never an equation
            continue;
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
            static_cast<void>(system.addConstraintVertical(
                point(constraint.first, lines, circles, arcs, centers),
                point(*constraint.second, lines, circles, arcs, centers), tag));
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
            const auto curve = [&](std::uint64_t id) -> GCS::Curve& {
                if (lines.contains(id))
                    return lines.at(id)->line;
                return arcs.at(id)->arc;
            };
            if (const auto joint = sharedEnd(sketch.constraints, firstId, secondId)) {
                // Same or opposite direction at the joint, whichever the geometry has now.
                GCS::Point& at = point(*joint, lines, circles, arcs, centers);
                const double current =
                    system.calculateAngleViaPoint(curve(firstId), curve(secondId), at);
                jointAngles.push_back(std::cos(current) >= 0.0 ? 0.0 : std::numbers::pi);
                static_cast<void>(system.addConstraintAngleViaPoint(curve(firstId), curve(secondId),
                                                                    at, &jointAngles.back(), tag));
            } else if (lines.contains(firstId))
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
    GCS::VEC_pD dependent;
    system.getDependentParams(dependent);
    const std::set<double*> freeParameters(dependent.begin(), dependent.end());
    for (std::size_t index = 0; index < unknownStarts.size(); ++index) {
        const auto end =
            index + 1 < unknownStarts.size() ? unknownStarts[index + 1].second : unknowns.size();
        for (auto parameter = unknownStarts[index].second; parameter < end; ++parameter)
            if (freeParameters.contains(unknowns[parameter])) {
                result.freeElements.push_back(unknownStarts[index].first);
                break;
            }
    }
    for (int tag : conflictingTags) {
        if (tag > 0 && static_cast<std::size_t>(tag) <= tags.size())
            result.conflictingConstraints.push_back(tags[static_cast<std::size_t>(tag - 1)]);
    }
    // An instance is free while its source or the mirror axis is.
    for (const auto& pattern : sketch.patterns.values) {
        const auto free = [&](document::SketchElementId id) {
            return std::ranges::find(result.freeElements, id) != result.freeElements.end();
        };
        const bool axisFree = pattern.axis && free(*pattern.axis);
        for (std::size_t index = 0; index < pattern.instances.size(); ++index)
            if (axisFree || free(pattern.sources[index % pattern.sources.size()]))
                result.freeElements.push_back(pattern.instances[index]);
    }
    GCS::VEC_I redundantTags;
    system.getRedundant(redundantTags);
    for (int tag : redundantTags) {
        if (tag > 0 && static_cast<std::size_t>(tag) <= tags.size() &&
            std::ranges::find(result.redundantConstraints,
                              tags[static_cast<std::size_t>(tag - 1)]) ==
                result.redundantConstraints.end())
            result.redundantConstraints.push_back(tags[static_cast<std::size_t>(tag - 1)]);
    }
    if (!result.conflictingConstraints.empty()) {
        result.status = SketchSolveStatus::conflicting;
        return result;
    }

    const int status = system.solve(unknowns);
    if (status != GCS::Success && status != GCS::Converged)
        return result;
    system.applySolution();
    for (const auto& state : states)
        std::visit([](const auto& element) { element.writeBack(); }, state);
    document::regenerateSketchPatterns(sketch.elements, sketch.patterns);
    const auto originalConstraints = sketch.constraints;
    measureReferenceDimensions(sketch);
    try {
        document::validateSketchParameters(sketch);
    } catch (const std::invalid_argument&) {
        sketch.elements = originalElements;
        sketch.constraints = originalConstraints;
        return result;
    }
    result.status = SketchSolveStatus::success;
    return result;
}

} // namespace voelund::features
