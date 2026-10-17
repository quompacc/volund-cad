#include "FeatureEvaluationDetail.hpp"
#include <voelund/kernel/PlaneNormal.hpp>

#include <voelund/document/DocumentSnapshot.hpp>
#include <voelund/features/FeatureDependencies.hpp>
#include <voelund/features/GeometryEvaluationError.hpp>
#include <voelund/features/SketchSupportBinding.hpp>
#include <voelund/kernel/ShapeFactory.hpp>
#include <voelund/kernel/ShapeOperations.hpp>

#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>

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

// The cylinder the hole removes from `body`, whose current faces `owner` names.
kernel::ShapeOperationResult holeTool(const EvaluatedFeature& owner, const TopoDS_Shape& body,
                                      const document::HoleParameters& parameters) {
    const auto frames = supportFaceFrames(owner);
    const auto frame = std::ranges::find(frames, parameters.face.origin, &SupportFaceFrame::origin);
    if (frame == frames.end())
        throw GeometryEvaluationError(
            "The face of the hole is no longer a unique planar face of its body",
            "hole-face-missing");
    const auto& plane = frame->plane;
    const gp_Vec normal(plane.normalX, plane.normalY, plane.normalZ);
    const gp_Vec xAxis(plane.xDirectionX, plane.xDirectionY, plane.xDirectionZ);
    const gp_Vec yAxis = normal.Crossed(xAxis);
    gp_Pnt centre(plane.originX, plane.originY, plane.originZ);
    centre.Translate(xAxis * parameters.x + yAxis * parameters.y);
    // Into the body: against the outward normal of the face as it lies in the solid.
    gp_Vec inward;
    for (TopExp_Explorer explorer(body, TopAbs_FACE); explorer.More(); explorer.Next()) {
        if (!explorer.Current().IsSame(frame->face))
            continue;
        const BRepAdaptor_Surface surface(TopoDS::Face(explorer.Current()));
        inward = -gp_Vec(kernel::planeNormal(surface.Plane(), explorer.Current().Orientation()));
        break;
    }
    if (inward.Magnitude() < 0.5)
        throw GeometryEvaluationError("The face of the hole is not a face of its body",
                                      "hole-face-missing");
    double length = parameters.depth.value_or(0.0);
    if (!parameters.depth) {
        // Through all: longer than the body could ever be measured from this face.
        Bnd_Box box;
        BRepBndLib::Add(body, box);
        double x0 = 0, y0 = 0, z0 = 0, x1 = 0, y1 = 0, z1 = 0;
        box.Get(x0, y0, z0, x1, y1, z1);
        length = 2.0 * std::hypot(x1 - x0, y1 - y0, z1 - z0) + 1.0;
    }
    return kernel::ShapeFactory::makeCylinderWithHistory({parameters.diameter / 2.0, length,
                                                          centre.X(), centre.Y(), centre.Z(),
                                                          inward.X(), inward.Y(), inward.Z()});
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
    const auto& owner = findFeature(outputs, *producer);
    EvaluatedBody& body = findBody(bodies, outputs, *producer);
    EvaluatedBody& referenceBody = findBody(referenceBodies, outputs, *producer);
    const auto tool = holeTool(owner, body.shape, parameters);
    const auto referenceOwner = referenceOf(owner);
    const auto referenceTool = holeTool(referenceOwner, referenceBody.shape, parameters);
    const auto evaluated = kernel::ShapeOperations::cutWithHistory(body.shape, tool.shape);
    const auto reference =
        kernel::ShapeOperations::cutWithHistory(referenceBody.shape, referenceTool.shape);
    outputs.push_back(EvaluatedFeature{
        descriptor.id,
        evaluated.shape,
        reference.shape,
        body.bodyId,
        joined(owner.supportOrigins, primitiveSupportOrigins(descriptor.id, tool), evaluated),
        {},
        joined(owner.referenceSupportOrigins, primitiveSupportOrigins(descriptor.id, referenceTool),
               reference),
        joinedEdges(owner.edgeOrigins, primitiveEdgeOrigins(descriptor.id, tool), evaluated),
        joinedEdges(owner.referenceEdgeOrigins, primitiveEdgeOrigins(descriptor.id, referenceTool),
                    reference)});
    body.sourceFeature = descriptor.id;
    body.shape = evaluated.shape;
    referenceBody.sourceFeature = descriptor.id;
    referenceBody.shape = reference.shape;
}

} // namespace voelund::features::detail
