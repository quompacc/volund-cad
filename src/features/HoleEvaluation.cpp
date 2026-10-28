// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "FeatureEvaluationDetail.hpp"
#include <voelund/kernel/PlaneNormal.hpp>

#include <voelund/document/DocumentSnapshot.hpp>
#include <voelund/features/FeatureDependencies.hpp>
#include <voelund/features/GeometryEvaluationError.hpp>
#include <voelund/features/SketchSupportBinding.hpp>
#include <voelund/kernel/HoleTool.hpp>
#include <voelund/kernel/KernelError.hpp>
#include <voelund/kernel/ShapeFactory.hpp>
#include <voelund/kernel/ShapeOperations.hpp>

#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <voelund/features/FilletEdges.hpp>
#include <voelund/kernel/EdgeQueries.hpp>
#include <voelund/kernel/ShapeQueries.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

// M08 (user decision 2026-10-13): a hole into a planar body face, at (x, y) in the face's
// frame, along the inward normal. The face is named by its origin and followed through the
// body's history; a hole "through all" reaches through the body however thick it becomes.
// The hole's wall and bottom are named like a cylinder's (cylinderSide, cylinderEnd).
namespace voelund::features::detail {
namespace {

EvaluatedFeature referenceOf(const EvaluatedFeature& output) {
    return {output.id,
            output.referenceShape,
            output.referenceShape,
            output.bodyId,
            output.referenceSupportOrigins,
            {},
            {},
            output.referenceEdgeOrigins};
}

// N09: the turned profile of a counterbore, countersink or drill point (a plain hole keeps
// M08's cylinder, so its faces keep their names).
kernel::ShapeOperationResult variantTool(const document::HoleParameters& parameters,
                                         const gp_Pnt& centre, const gp_Vec& inward,
                                         double length) {
    const auto& variant = parameters.variant;
    const double degrees = std::numbers::pi / 180.0;
    kernel::HoleProfile profile{parameters.diameter / 2.0, length};
    if (variant.kind != document::HoleKind::simple)
        profile.headRadius = variant.headDiameter / 2.0;
    if (variant.kind == document::HoleKind::counterbore)
        profile.headDepth = variant.headDepth;
    profile.countersinkAngle = variant.countersinkAngle * degrees;
    if (variant.drillPoint)
        profile.tipAngle = document::drillPointAngle * degrees;
    profile.origin = {centre.X(), centre.Y(), centre.Z()};
    const gp_Dir direction(inward);
    profile.direction = {direction.X(), direction.Y(), direction.Z()};
    try {
        return kernel::makeHoleToolWithHistory(profile);
    } catch (const kernel::KernelError&) {
        throw GeometryEvaluationError("The head of the hole is deeper than the hole",
                                      "hole-head-too-deep");
    }
}

// Where the holes enter `body` through the face `owner` names, and how far they reach.
struct HoleAxis final {
    gp_Pnt origin; // of the face's frame
    gp_Vec xAxis;
    gp_Vec yAxis;
    gp_Vec normal;
    gp_Vec inward;
    double length;
    gp_Pnt faceCentre{}; // N09c: which side of an edge is the face's
};
HoleAxis holeAxis(const EvaluatedFeature& owner, const TopoDS_Shape& body,
                  const document::HoleParameters& parameters) {
    const auto frames = supportFaceFrames(owner);
    const auto frame = std::ranges::find(frames, parameters.face.origin, &SupportFaceFrame::origin);
    if (frame == frames.end())
        throw GeometryEvaluationError(
            "The face of the hole is no longer a unique planar face of its body",
            "hole-face-missing");
    const auto& plane = frame->plane;
    HoleAxis axis{gp_Pnt(plane.originX, plane.originY, plane.originZ),
                  gp_Vec(plane.xDirectionX, plane.xDirectionY, plane.xDirectionZ),
                  {},
                  gp_Vec(plane.normalX, plane.normalY, plane.normalZ),
                  {},
                  parameters.depth.value_or(0.0)};
    axis.yAxis = axis.normal.Crossed(axis.xAxis);
    // Into the body: against the outward normal of the face as it lies in the solid.
    for (TopExp_Explorer explorer(body, TopAbs_FACE); explorer.More(); explorer.Next()) {
        if (!explorer.Current().IsSame(frame->face))
            continue;
        if (const auto outward = kernel::planarFaceNormal(explorer.Current()))
            axis.inward = -gp_Vec(*outward);
        break;
    }
    if (axis.inward.Magnitude() < 0.5)
        throw GeometryEvaluationError("The face of the hole is not a face of its body",
                                      "hole-face-missing");
    axis.faceCentre = kernel::boundingBoxCentre(frame->face);
    if (!parameters.depth) {
        // Through all: longer than the body could ever be measured from this face.
        const auto box = kernel::boundingBox(body);
        axis.length = 2.0 * (box ? box->diagonal() : 0.0) + 1.0;
    }
    return axis;
}

// One edge of the face as a line in its frame: a point and the unit normal into the face.
struct EdgeLine final {
    double u, v;   // a point of the edge
    double nu, nv; // towards the face
};
EdgeLine edgeLine(const HoleAxis& axis, std::span<const SupportProvenance> origins,
                  const document::FilletEdge& named) {
    const auto shapes = filletEdgeShapes(origins, named);
    if (shapes.empty())
        throw GeometryEvaluationError("An edge the hole is placed by is gone", "hole-edge-missing");
    const auto curve = kernel::describeEdge(shapes.front());
    if (curve.kind != kernel::EdgeCurve::Kind::line)
        throw GeometryEvaluationError("A hole is placed by straight edges",
                                      "hole-edge-not-straight");
    const auto inFrame = [&](const gp_Vec& vector) {
        return std::pair{vector.Dot(axis.xAxis), vector.Dot(axis.yAxis)};
    };
    const auto [u, v] = inFrame(gp_Vec(axis.origin, curve.line.Location()));
    const auto [tu, tv] = inFrame(gp_Vec(curve.line.Direction()));
    const double length = std::hypot(tu, tv);
    if (length < 1e-9)
        throw GeometryEvaluationError("An edge the hole is placed by stands upright on its face",
                                      "hole-edge-not-straight");
    EdgeLine line{u, v, -tv / length, tu / length};
    const auto [cu, cv] = inFrame(gp_Vec(axis.origin, axis.faceCentre));
    if (line.nu * (cu - u) + line.nv * (cv - v) < 0) {
        line.nu = -line.nu;
        line.nv = -line.nv;
    }
    return line;
}

// N09c: the point at the two distances from the two edges, inside the face.
gp_Pnt offsetCentre(const HoleAxis& axis, const document::HoleParameters& parameters,
                    std::span<const SupportProvenance> origins) {
    const auto a = edgeLine(axis, origins, parameters.offsets[0].edge);
    const auto b = edgeLine(axis, origins, parameters.offsets[1].edge);
    const double det = a.nu * b.nv - a.nv * b.nu;
    if (std::abs(det) < 1e-9)
        throw GeometryEvaluationError("The edges a hole is placed by are parallel",
                                      "hole-edges-parallel");
    const double ra = parameters.offsets[0].distance + a.nu * a.u + a.nv * a.v;
    const double rb = parameters.offsets[1].distance + b.nu * b.u + b.nv * b.v;
    const double u = (ra * b.nv - rb * a.nv) / det;
    const double v = (a.nu * rb - b.nu * ra) / det;
    return axis.origin.Translated(axis.xAxis * u + axis.yAxis * v);
}

// N09b: the points of the input sketch, laid onto the face; N09c: at distances from two edges;
// otherwise (x, y) of the face.
std::vector<gp_Pnt> holeCentres(const HoleAxis& axis, const document::HoleParameters& parameters,
                                const document::SketchParameters* sketch,
                                std::span<const SupportProvenance> origins) {
    if (!parameters.offsets.empty())
        return {offsetCentre(axis, parameters, origins)};
    if (parameters.points.empty())
        return {axis.origin.Translated(axis.xAxis * parameters.x + axis.yAxis * parameters.y)};
    if (sketch == nullptr)
        throw GeometryEvaluationError("The sketch of the holes is missing", "hole-sketch-missing");
    const auto& plane = sketch->plane;
    const gp_Pnt origin(plane.originX, plane.originY, plane.originZ);
    const gp_Vec x(plane.xDirectionX, plane.xDirectionY, plane.xDirectionZ);
    const gp_Vec y = gp_Vec(plane.normalX, plane.normalY, plane.normalZ).Crossed(x);
    std::vector<gp_Pnt> centres;
    for (const auto id : parameters.points) {
        const auto element =
            std::ranges::find(sketch->elements.values, id, &document::SketchElement::id);
        const auto* point = element == sketch->elements.values.end()
                                ? nullptr
                                : std::get_if<document::SketchPointGeometry>(&element->geometry);
        if (point == nullptr)
            throw GeometryEvaluationError("A point of the holes is gone from its sketch",
                                          "hole-point-missing");
        const gp_Pnt at = origin.Translated(x * point->position.x + y * point->position.y);
        const double above = gp_Vec(axis.origin, at).Dot(axis.normal) / axis.normal.Magnitude();
        centres.push_back(at.Translated(axis.normal.Normalized() * -above));
    }
    return centres;
}

// The solid one hole removes at `centre`: M08's cylinder, or the turned profile of a variant.
kernel::ShapeOperationResult holeToolAt(const document::HoleParameters& parameters,
                                        const HoleAxis& axis, const gp_Pnt& centre) {
    if (!parameters.variant.plain())
        return variantTool(parameters, centre, axis.inward, axis.length);
    return kernel::ShapeFactory::makeCylinderWithHistory(
        {parameters.diameter / 2.0, axis.length, centre.X(), centre.Y(), centre.Z(),
         axis.inward.X(), axis.inward.Y(), axis.inward.Z()});
}

std::vector<SupportProvenance> joined(std::span<const SupportProvenance> previous,
                                      std::span<const SupportProvenance> tool,
                                      const kernel::ShapeOperationResult& result) {
    auto origins = continueSupportOrigins(previous, result, kernel::HistoryOperand::first);
    auto added = continueSupportOrigins(tool, result, kernel::HistoryOperand::second);
    origins.insert(origins.end(), added.begin(), added.end());
    return origins;
}
std::vector<EdgeProvenance> joinedEdges(std::span<const EdgeProvenance> previous,
                                        std::span<const EdgeProvenance> tool,
                                        const kernel::ShapeOperationResult& result) {
    auto edges = continueEdgeOrigins(previous, result, kernel::HistoryOperand::first);
    auto added = continueEdgeOrigins(tool, result, kernel::HistoryOperand::second);
    edges.insert(edges.end(), added.begin(), added.end());
    return edges;
}

// One body cut by every hole in turn. N09b: the faces of the second and later holes carry
// the instance {hole, i} (as pattern copies do); their edges stay unnamed.
struct Drilled final {
    TopoDS_Shape shape;
    std::vector<SupportProvenance> faces;
    std::vector<EdgeProvenance> edges;
};
Drilled drill(Drilled body, std::span<const kernel::ShapeOperationResult> tools,
              document::FeatureId hole) {
    for (std::size_t index = 0; index < tools.size(); ++index) {
        auto faces = primitiveSupportOrigins(hole, tools[index]);
        if (index > 0)
            for (auto& face : faces)
                face.origin.instances.push_back({hole, static_cast<std::uint32_t>(index)});
        const auto edges =
            index == 0 ? primitiveEdgeOrigins(hole, tools[index]) : std::vector<EdgeProvenance>{};
        const auto cut = kernel::ShapeOperations::cutWithHistory(body.shape, tools[index].shape);
        body = {cut.shape, joined(body.faces, faces, cut), joinedEdges(body.edges, edges, cut)};
    }
    return body;
}

std::vector<kernel::ShapeOperationResult> holeTools(const EvaluatedFeature& owner,
                                                    const TopoDS_Shape& body,
                                                    const document::HoleParameters& parameters,
                                                    const document::SketchParameters* sketch) {
    const auto axis = holeAxis(owner, body, parameters);
    std::vector<kernel::ShapeOperationResult> tools;
    for (const auto& centre : holeCentres(axis, parameters, sketch, owner.supportOrigins))
        tools.push_back(holeToolAt(parameters, axis, centre));
    return tools;
}

} // namespace

void evaluateHole(const document::DocumentSnapshot& document,
                  const document::FeatureDescriptor& descriptor,
                  std::vector<EvaluatedFeature>& outputs, std::vector<EvaluatedBody>& bodies,
                  std::vector<EvaluatedBody>& referenceBodies) {
    const auto& parameters = document.holeParameters(descriptor.id);
    const auto producer = bodyProducerBefore(document, descriptor.id, parameters.face.body);
    if (!producer)
        throw GeometryEvaluationError("The body of the hole has no solid before it",
                                      "body-missing");
    const document::SketchParameters* sketch = nullptr;
    if (descriptor.inputFeature)
        if (const auto& resolved = findFeature(outputs, *descriptor.inputFeature).resolvedSketch)
            sketch = &*resolved;
    const auto& owner = findFeature(outputs, *producer);
    EvaluatedBody& body = findBody(bodies, outputs, *producer);
    EvaluatedBody& referenceBody = findBody(referenceBodies, outputs, *producer);
    const auto referenceOwner = referenceOf(owner);
    const auto evaluated = drill({body.shape, owner.supportOrigins, owner.edgeOrigins},
                                 holeTools(owner, body.shape, parameters, sketch), descriptor.id);
    const auto reference =
        drill({referenceBody.shape, owner.referenceSupportOrigins, owner.referenceEdgeOrigins},
              holeTools(referenceOwner, referenceBody.shape, parameters, sketch), descriptor.id);
    outputs.push_back(EvaluatedFeature{descriptor.id,
                                       evaluated.shape,
                                       reference.shape,
                                       body.bodyId,
                                       evaluated.faces,
                                       {},
                                       reference.faces,
                                       evaluated.edges,
                                       reference.edges});
    body.sourceFeature = descriptor.id;
    body.shape = evaluated.shape;
    referenceBody.sourceFeature = descriptor.id;
    referenceBody.shape = reference.shape;
}

} // namespace voelund::features::detail
