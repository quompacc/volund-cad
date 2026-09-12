#include <voelund/features/SketchConstraintSolver.hpp>

#include <GCS.h>

#include <cstdint>
#include <memory>
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

using LineMap = std::unordered_map<std::uint64_t, LineState*>;
using CircleMap = std::unordered_map<std::uint64_t, CircleState*>;

GCS::Point& point(const document::SketchGeometryReference& reference, const LineMap& lines,
                  const CircleMap& circles) {
    if (reference.point == document::SketchPointPosition::center)
        return circles.at(reference.element.value())->circle.center;
    LineState& line = *lines.at(reference.element.value());
    return reference.point == document::SketchPointPosition::start ? line.first : line.second;
}

} // namespace

SketchSolveResult solveSketchConstraints(document::SketchParameters& sketch) {
    document::validateSketchParameters(sketch);
    std::vector<std::unique_ptr<LineState>> lineStates;
    std::vector<std::unique_ptr<CircleState>> circleStates;
    LineMap lines;
    CircleMap circles;
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
            circleStates.push_back(std::move(state));
        }
    }

    GCS::System system;
    std::vector<double> values;
    values.reserve(sketch.constraints.values.size());
    std::vector<document::SketchConstraintId> tags;
    tags.reserve(sketch.constraints.values.size());
    for (const document::SketchConstraint& constraint : sketch.constraints.values) {
        values.push_back(constraint.value);
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
        else if (constraint.kind == document::SketchConstraintKind::radius)
            static_cast<void>(system.addConstraintCircleRadius(
                circles.at(constraint.first.element.value())->circle, &values.back(), tag));
        else if (constraint.kind == document::SketchConstraintKind::coincident)
            static_cast<void>(
                system.addConstraintP2PCoincident(point(constraint.first, lines, circles),
                                                  point(*constraint.second, lines, circles), tag));
        else {
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
    result.status = SketchSolveStatus::success;
    return result;
}

} // namespace voelund::features
