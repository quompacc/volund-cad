// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "FeatureEvaluationDetail.hpp"

#include <voelund/document/DocumentSnapshot.hpp>
#include <voelund/features/FeatureDependencies.hpp>
#include <voelund/features/GeometryEvaluationError.hpp>
#include <voelund/kernel/ShapeFactory.hpp>
#include <voelund/kernel/ShapeOperations.hpp>

#include <TopoDS.hxx>
#include <gp_Ax1.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>
#include <voelund/kernel/ShapeQueries.hpp>

#include <array>
#include <cmath>
#include <numbers>

// M10 (user decision 2026-10-15): copies of a body as it was before the pattern, along a
// direction or about an axis. Copy k stands k steps from the original: `spacing` apart, or
// turned by a share of `angle` (a full turn evenly without a doubled end, a part with both ends
// included). Copied faces keep their originals' names plus {pattern, k}. As new bodies the
// pattern passes its body on unchanged and every copy is a body of its own; joined, the copies
// fuse into the body one after the other.
namespace voelund::features::detail {
namespace {
constexpr std::uint32_t maximumColumns = document::maximumPatternCount + 1;

struct Step final {
    std::array<double, 3> origin;
    std::array<double, 3> direction;
    double angle;
    std::array<double, 3> translation;

    [[nodiscard]] gp_Trsf transform() const {
        gp_Trsf turn;
        if (angle != 0.0)
            turn.SetRotation(gp_Ax1(gp_Pnt(origin[0], origin[1], origin[2]),
                                    gp_Dir(direction[0], direction[1], direction[2])),
                             angle);
        gp_Trsf shift;
        shift.SetTranslation(gp_Vec(translation[0], translation[1], translation[2]));
        return shift * turn;
    }
};

// The pattern's direction or axis: an earlier construction axis or one as it describes.
struct AxisChoice final {
    const document::DatumAxisParameters& axis;
    const std::optional<document::FeatureId>& datumAxis;
    bool reversed;
};

AxisLine lineOf(const document::DocumentSnapshot& document, document::FeatureId pattern,
                const AxisChoice& choice, const std::vector<EvaluatedFeature>& outputs) {
    AxisLine line{gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)};
    if (choice.datumAxis) {
        const auto& frame = findFeature(outputs, *choice.datumAxis).datumFrame;
        if (!frame)
            throw GeometryEvaluationError("The axis of the pattern is missing",
                                          "pattern-axis-missing");
        line = {gp_Pnt(frame->originX, frame->originY, frame->originZ),
                gp_Dir(frame->normalX, frame->normalY, frame->normalZ)};
    } else {
        try {
            line = datumAxisLine(document, pattern, choice.axis, outputs);
        } catch (const GeometryEvaluationError& error) {
            throw GeometryEvaluationError(error.what(), "pattern-axis-missing");
        }
    }
    if (choice.reversed)
        line.direction.Reverse();
    return line;
}

// N11a: the first direction, and a grid's second one.
struct Directions final {
    AxisLine first;
    std::optional<AxisLine> second;
};

Directions axisOf(const document::DocumentSnapshot& document, document::FeatureId pattern,
                  const document::PatternParameters& parameters,
                  const std::vector<EvaluatedFeature>& outputs) {
    Directions result{lineOf(document, pattern,
                             {parameters.axis, parameters.datumAxis, parameters.reversed}, outputs),
                      std::nullopt};
    if (const auto& second = parameters.second)
        result.second =
            lineOf(document, pattern, {second->axis, second->datumAxis, second->reversed}, outputs);
    return result;
}

// Instance i + count * j of a grid stands i spacings along the first direction and j along the
// second.
std::array<double, 3> shiftOf(const Directions& axes, const document::PatternParameters& parameters,
                              std::uint32_t index) {
    const auto columns = parameters.second ? parameters.count : maximumColumns;
    const gp_Vec along = gp_Vec(axes.first.direction) * (parameters.spacing * (index % columns));
    gp_Vec shift = along;
    if (axes.second)
        shift += gp_Vec(axes.second->direction) * (parameters.second->spacing * (index / columns));
    return {shift.X(), shift.Y(), shift.Z()};
}

Step stepOf(const Directions& axes, const document::PatternParameters& parameters,
            std::uint32_t index) {
    const AxisLine& axis = axes.first;
    const std::array<double, 3> origin{axis.origin.X(), axis.origin.Y(), axis.origin.Z()};
    const std::array<double, 3> direction{axis.direction.X(), axis.direction.Y(),
                                          axis.direction.Z()};
    if (parameters.kind == document::PatternKind::linear)
        return {origin, direction, 0.0, shiftOf(axes, parameters, index)};
    const double total = parameters.angle.value();
    const bool fullTurn = std::abs(total - 2 * std::numbers::pi) < 1e-9;
    const double share = total / (fullTurn ? parameters.count : parameters.count - 1);
    return {origin, direction, share * index, {0.0, 0.0, 0.0}};
}

struct Copy final {
    kernel::ShapeOperationResult shape;
    std::vector<SupportProvenance> faces;
    std::vector<EdgeProvenance> edges;
};

Copy copyOf(const TopoDS_Shape& body, std::span<const SupportProvenance> faces,
            std::span<const EdgeProvenance> edges, const Step& step, document::FeatureId pattern,
            std::uint32_t index) {
    Copy copy{kernel::ShapeFactory::copyWithHistory(body, step.origin, step.direction, step.angle,
                                                    step.translation),
              {},
              {}};
    const auto transform = step.transform();
    const auto moved = [&](const auto& shape) { return kernel::transformedCopy(shape, transform); };
    copy.faces = continueSupportOrigins(faces, copy.shape, kernel::HistoryOperand::first);
    for (auto& face : copy.faces) {
        face.origin.instances.push_back({pattern, index});
        if (!face.frameFace.IsNull())
            face.frameFace = moved(face.frameFace);
    }
    copy.edges = continueEdgeOrigins(edges, copy.shape, kernel::HistoryOperand::first);
    for (auto& edge : copy.edges)
        edge.source = TopoDS::Edge(moved(edge.source));
    return copy;
}

// The body joined with its copies 1..count-1 in turn; edges of the copies stay unnamed.
struct Joined final {
    TopoDS_Shape shape;
    std::vector<SupportProvenance> faces;
    std::vector<EdgeProvenance> edges;
};

Joined join(const TopoDS_Shape& body, std::span<const SupportProvenance> faces,
            std::span<const EdgeProvenance> edges, const Directions& axis,
            const document::PatternParameters& parameters, document::FeatureId pattern) {
    Joined result{body, {faces.begin(), faces.end()}, {edges.begin(), edges.end()}};
    for (std::uint32_t index = 1; index < document::patternInstances(parameters); ++index) {
        if (document::patternSkips(parameters, index))
            continue; // N11a
        const auto copy =
            copyOf(body, faces, edges, stepOf(axis, parameters, index), pattern, index);
        const auto fused = kernel::ShapeOperations::fuseWithHistory(result.shape, copy.shape.shape);
        auto joinedFaces =
            continueSupportOrigins(result.faces, fused, kernel::HistoryOperand::first);
        const auto copied =
            continueSupportOrigins(copy.faces, fused, kernel::HistoryOperand::second);
        joinedFaces.insert(joinedFaces.end(), copied.begin(), copied.end());
        result = {fused.shape, std::move(joinedFaces),
                  continueEdgeOrigins(result.edges, fused, kernel::HistoryOperand::first)};
    }
    return result;
}

} // namespace

void evaluatePattern(const document::DocumentSnapshot& document,
                     const document::FeatureDescriptor& descriptor,
                     std::vector<EvaluatedFeature>& outputs, std::vector<EvaluatedBody>& bodies,
                     std::vector<EvaluatedBody>& referenceBodies) {
    const auto& parameters = document.patternParameters(descriptor.id);
    const auto producer = bodyProducerBefore(document, descriptor.id, parameters.body);
    if (!producer)
        throw GeometryEvaluationError("The body of the pattern has no solid before it",
                                      "pattern-body-missing");
    const auto axis = axisOf(document, descriptor.id, parameters, outputs);
    const auto& owner = findFeature(outputs, *producer);
    EvaluatedBody& body = findBody(bodies, outputs, *producer);
    EvaluatedBody& referenceBody = findBody(referenceBodies, outputs, *producer);
    EvaluatedFeature output{descriptor.id,
                            body.shape,
                            referenceBody.shape,
                            body.bodyId,
                            owner.supportOrigins,
                            {},
                            owner.referenceSupportOrigins,
                            owner.edgeOrigins,
                            owner.referenceEdgeOrigins};
    if (parameters.mode == document::PatternMode::join) {
        auto joined = join(body.shape, owner.supportOrigins, owner.edgeOrigins, axis, parameters,
                           descriptor.id);
        output.shape = joined.shape;
        output.supportOrigins = std::move(joined.faces);
        output.edgeOrigins = std::move(joined.edges);
        // The unplaced reference joins its own copies; where it cannot, it stays.
        try {
            auto reference = join(referenceBody.shape, owner.referenceSupportOrigins,
                                  owner.referenceEdgeOrigins, axis, parameters, descriptor.id);
            output.referenceShape = reference.shape;
            output.referenceSupportOrigins = std::move(reference.faces);
            output.referenceEdgeOrigins = std::move(reference.edges);
        } catch (const kernel::KernelError&) {
        }
    }
    body.sourceFeature = descriptor.id;
    body.shape = output.shape;
    referenceBody.sourceFeature = descriptor.id;
    referenceBody.shape = output.referenceShape;
    outputs.push_back(std::move(output));
}

void evaluatePatternCopy(const document::DocumentSnapshot& document,
                         const document::FeatureDescriptor& descriptor,
                         std::vector<EvaluatedFeature>& outputs, std::vector<EvaluatedBody>& bodies,
                         std::vector<EvaluatedBody>& referenceBodies) {
    const auto& values = document.patternCopyParameters(descriptor.id);
    const auto& parameters = document.patternParameters(values.pattern);
    // A new-body pattern passes its body on unchanged: its result is the original.
    const auto& original = findFeature(outputs, values.pattern);
    const auto axis = axisOf(document, values.pattern, parameters, outputs);
    auto copy = copyOf(original.shape, original.supportOrigins, original.edgeOrigins,
                       stepOf(axis, parameters, values.index), values.pattern, values.index);
    const document::BodyId bodyId{descriptor.id.value()};
    EvaluatedFeature output{descriptor.id, copy.shape.shape, copy.shape.shape,
                            bodyId,        copy.faces,       {},
                            copy.faces,    copy.edges,       copy.edges};
    bodies.push_back(EvaluatedBody{bodyId, descriptor.id, output.shape});
    referenceBodies.push_back(EvaluatedBody{bodyId, descriptor.id, output.referenceShape});
    outputs.push_back(std::move(output));
}

} // namespace voelund::features::detail
