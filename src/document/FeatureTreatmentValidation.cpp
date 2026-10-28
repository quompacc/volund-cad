// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/document/HoleThreads.hpp>
#include <voelund/document/ModelDocument.hpp>
#include <voelund/kernel/NumericPolicy.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <stdexcept>

// M05/M07/M08: validation of the fillet, chamfer and hole parameters.
namespace voelund::document {
namespace {
// M05/M07: the body, 1-256 distinct edges between two different faces, a model length.
void validateEdgeTreatment(BodyId body, const std::vector<FilletEdge>& edges, double value) {
    if (!kernel::modelLength(value))
        throw std::invalid_argument(kernel::modelRangeDiagnostic);
    if (body.value() == 0)
        throw std::invalid_argument("A fillet or chamfer names its body");
    if (edges.empty() || edges.size() > maximumFilletEdges)
        throw std::invalid_argument("A fillet or chamfer treats between 1 and 256 edges");
    for (std::size_t index = 0; index < edges.size(); ++index) {
        const FilletEdge& edge = edges[index];
        validateSubshapeOrigin(edge.first);
        validateSubshapeOrigin(edge.second);
        if (edge.first == edge.second)
            throw std::invalid_argument("A fillet edge lies between two different faces");
        if (edge.id == 0)
            throw std::invalid_argument("A fillet or chamfer edge has an id");
        for (std::size_t prior = 0; prior < index; ++prior) {
            const FilletEdge& other = edges[prior];
            if ((other.first == edge.first && other.second == edge.second) ||
                (other.first == edge.second && other.second == edge.first) || other.id == edge.id)
                throw std::invalid_argument("Fillet and chamfer edges must be unique");
        }
    }
}
} // namespace

void validateFilletParameters(const FilletParameters& parameters) {
    validateEdgeTreatment(parameters.body, parameters.edges, parameters.radius);
}

void validateChamferParameters(const ChamferParameters& parameters) {
    validateEdgeTreatment(parameters.body, parameters.edges, parameters.distance);
}

namespace {
// N09: the head of a counterbore or countersink, the drill point and the thread.
void validateHoleHead(const HoleParameters& parameters);
// N09c: none or two edges with distances in the model range, not with sketch points.
void validateHoleOffsets(const HoleParameters& parameters) {
    const auto& offsets = parameters.offsets;
    if (offsets.empty())
        return;
    if (offsets.size() != 2 || !parameters.points.empty() ||
        !kernel::modelCoordinate(offsets[0].distance) ||
        !kernel::modelCoordinate(offsets[1].distance))
        throw std::invalid_argument("A hole is placed by two edges and their distances");
}
// A counterbore has a depth and ends above the bottom of a blind hole.
void validateCounterbore(const HoleParameters& parameters) {
    const double depth = parameters.variant.headDepth;
    if (!kernel::modelLength(depth) || (parameters.depth && depth >= *parameters.depth))
        throw std::invalid_argument("A counterbore is shallower than its hole");
}
void validateHoleThread(const HoleParameters& parameters) {
    const auto& variant = parameters.variant;
    if (variant.drillPoint && !parameters.depth)
        throw std::invalid_argument("Only a blind hole ends in a drill point");
    if (variant.thread.empty())
        return;
    const auto thread = metricThread(variant.thread);
    if (!thread || std::abs(thread->coreDrill - parameters.diameter) > 1e-9)
        throw std::invalid_argument("A tapped hole has a known thread and its core drill");
}
void validateHoleVariant(const HoleParameters& parameters) {
    const auto kind = static_cast<int>(parameters.variant.kind);
    if (kind < 0 || kind > static_cast<int>(HoleKind::countersink))
        throw std::invalid_argument("Unknown hole kind");
    validateHoleHead(parameters);
    validateHoleThread(parameters);
}
// The head of a counterbore or countersink: wider than the hole, shallower than it.
void validateHoleHead(const HoleParameters& parameters) {
    const auto& variant = parameters.variant;
    if (variant.kind != HoleKind::simple &&
        (!kernel::modelLength(variant.headDiameter) || variant.headDiameter <= parameters.diameter))
        throw std::invalid_argument("The head of a hole is wider than the hole");
    if (variant.kind == HoleKind::counterbore)
        validateCounterbore(parameters);
    const bool angleFits = variant.countersinkAngle >= 30.0 && variant.countersinkAngle <= 150.0;
    if (variant.kind == HoleKind::countersink && !angleFits)
        throw std::invalid_argument("A countersink angle lies between 30 and 150 degrees");
}
} // namespace

void validateHoleParameters(const HoleParameters& parameters) {
    validateHoleVariant(parameters);
    // N09b: distinct point elements, in order, at most 100.
    if (parameters.points.size() > maximumHolePoints ||
        !std::ranges::is_sorted(parameters.points, {}, &SketchElementId::value) ||
        std::ranges::adjacent_find(parameters.points) != parameters.points.end())
        throw std::invalid_argument("Hole points are distinct, sorted and at most 100");
    validateHoleOffsets(parameters);
    if (!kernel::modelLength(parameters.diameter) ||
        (parameters.depth && !kernel::modelLength(*parameters.depth)) ||
        !kernel::modelCoordinate(parameters.x) || !kernel::modelCoordinate(parameters.y))
        throw std::invalid_argument(kernel::modelRangeDiagnostic);
    if (parameters.face.body.value() == 0)
        throw std::invalid_argument("A hole names the body of its face");
    validateSubshapeOrigin(parameters.face.origin);
}

} // namespace voelund::document
