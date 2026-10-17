#include "FeatureEvaluationDetail.hpp"

#include <voelund/document/DocumentSnapshot.hpp>
#include <voelund/features/FeatureDependencies.hpp>
#include <voelund/features/GeometryEvaluationError.hpp>
#include <voelund/features/SketchSupportBinding.hpp>
#include <voelund/kernel/ShapeFactory.hpp>
#include <voelund/kernel/ShapeOperations.hpp>

#include <BRepBuilderAPI_Transform.hxx>
#include <TopoDS.hxx>
#include <gp_Ax2.hxx>
#include <gp_Trsf.hxx>

#include <algorithm>
#include <array>

// M09 (user decision 2026-10-15): the mirror image of a body about a base plane, a
// construction plane or a planar face, as it was before the mirror. A new body keeps the
// original; join fuses the image into it. Mirrored faces keep the names of their originals plus
// the copy step (SubshapeOrigin::instances), so both halves stay apart. Edges of the image
// are named only on a body of its own; in a join the original's edges keep their names.
namespace voelund::features::detail {
namespace {

struct Plane final {
    std::array<double, 3> origin;
    std::array<double, 3> normal;
};

Plane mirrorPlane(const document::DocumentSnapshot& document, document::FeatureId mirror,
                  const document::MirrorPlane& plane,
                  const std::vector<EvaluatedFeature>& outputs) {
    const auto of = [](const document::SketchPlane& frame) {
        return Plane{{frame.originX, frame.originY, frame.originZ},
                     {frame.normalX, frame.normalY, frame.normalZ}};
    };
    if (plane.basePlane)
        return of(document::datumSketchPlane(*plane.basePlane));
    if (plane.constructionPlane) {
        const auto& frame = findFeature(outputs, *plane.constructionPlane).datumFrame;
        if (!frame)
            throw GeometryEvaluationError("The mirror plane is missing", "mirror-plane-missing");
        return of(*frame);
    }
    const auto producer = bodyProducerBefore(document, mirror, plane.face->body);
    if (!producer)
        throw GeometryEvaluationError("The mirror face has no body", "mirror-plane-missing");
    const auto frames = supportFaceFrames(findFeature(outputs, *producer));
    const auto found = std::ranges::find(frames, plane.face->origin, &SupportFaceFrame::origin);
    if (found == frames.end())
        throw GeometryEvaluationError("The mirror face is no longer a unique planar face",
                                      "mirror-plane-missing");
    return of(found->plane);
}

// The image of a body's names: the same faces mirrored, each one copy further.
struct Image final {
    kernel::ShapeOperationResult shape;
    std::vector<SupportProvenance> faces;
    std::vector<EdgeProvenance> edges;
};

Image mirrorImage(const TopoDS_Shape& body, std::span<const SupportProvenance> faces,
                  std::span<const EdgeProvenance> edges, const Plane& plane,
                  document::FeatureId mirror) {
    Image image{kernel::ShapeFactory::mirrorWithHistory(body, plane.origin, plane.normal), {}, {}};
    gp_Trsf reflection;
    reflection.SetMirror(gp_Ax2(gp_Pnt(plane.origin[0], plane.origin[1], plane.origin[2]),
                                gp_Dir(plane.normal[0], plane.normal[1], plane.normal[2])));
    const auto reflect = [&](const auto& shape) {
        return BRepBuilderAPI_Transform(shape, reflection, true).Shape();
    };
    image.faces = continueSupportOrigins(faces, image.shape, kernel::HistoryOperand::first);
    for (auto& face : image.faces) {
        face.origin.instances.push_back({mirror, 1});
        if (!face.frameFace.IsNull())
            face.frameFace = reflect(face.frameFace);
    }
    image.edges = continueEdgeOrigins(edges, image.shape, kernel::HistoryOperand::first);
    for (auto& edge : image.edges)
        edge.source = TopoDS::Edge(reflect(edge.source));
    return image;
}

} // namespace

void evaluateMirror(const document::DocumentSnapshot& document,
                    const document::FeatureDescriptor& descriptor,
                    std::vector<EvaluatedFeature>& outputs, std::vector<EvaluatedBody>& bodies,
                    std::vector<EvaluatedBody>& referenceBodies) {
    const auto& parameters = document.mirrorParameters(descriptor.id);
    const auto producer = bodyProducerBefore(document, descriptor.id, parameters.body);
    if (!producer)
        throw GeometryEvaluationError("The body of the mirror has no solid before it",
                                      "mirror-body-missing");
    const auto plane = mirrorPlane(document, descriptor.id, parameters.plane, outputs);
    const auto& owner = findFeature(outputs, *producer);
    EvaluatedBody& body = findBody(bodies, outputs, *producer);
    EvaluatedBody& referenceBody = findBody(referenceBodies, outputs, *producer);
    auto image =
        mirrorImage(body.shape, owner.supportOrigins, owner.edgeOrigins, plane, descriptor.id);
    if (parameters.mode == document::MirrorMode::newBody) {
        // A body of its own, placed where it is: its reference is the image itself.
        const document::BodyId bodyId{descriptor.id.value()};
        EvaluatedFeature output{descriptor.id, image.shape.shape, image.shape.shape,
                                bodyId,        image.faces,       {},
                                image.faces,   image.edges,       image.edges};
        bodies.push_back(EvaluatedBody{bodyId, descriptor.id, output.shape});
        referenceBodies.push_back(EvaluatedBody{bodyId, descriptor.id, output.referenceShape});
        outputs.push_back(std::move(output));
        return;
    }
    const auto fused = kernel::ShapeOperations::fuseWithHistory(body.shape, image.shape.shape);
    auto faces = continueSupportOrigins(owner.supportOrigins, fused, kernel::HistoryOperand::first);
    const auto mirrored =
        continueSupportOrigins(image.faces, fused, kernel::HistoryOperand::second);
    faces.insert(faces.end(), mirrored.begin(), mirrored.end());
    EvaluatedFeature output{
        descriptor.id,
        fused.shape,
        referenceBody.shape,
        body.bodyId,
        std::move(faces),
        {},
        owner.referenceSupportOrigins,
        continueEdgeOrigins(owner.edgeOrigins, fused, kernel::HistoryOperand::first),
        owner.referenceEdgeOrigins};
    // The unplaced reference is joined with its own image; where it cannot be, it stays.
    try {
        auto referenceImage = mirrorImage(referenceBody.shape, owner.referenceSupportOrigins,
                                          owner.referenceEdgeOrigins, plane, descriptor.id);
        const auto reference = kernel::ShapeOperations::fuseWithHistory(referenceBody.shape,
                                                                        referenceImage.shape.shape);
        auto referenceFaces = continueSupportOrigins(owner.referenceSupportOrigins, reference,
                                                     kernel::HistoryOperand::first);
        const auto referenceMirrored =
            continueSupportOrigins(referenceImage.faces, reference, kernel::HistoryOperand::second);
        referenceFaces.insert(referenceFaces.end(), referenceMirrored.begin(),
                              referenceMirrored.end());
        output.referenceShape = reference.shape;
        output.referenceSupportOrigins = std::move(referenceFaces);
        output.referenceEdgeOrigins = continueEdgeOrigins(owner.referenceEdgeOrigins, reference,
                                                          kernel::HistoryOperand::first);
    } catch (const kernel::KernelError&) {
    }
    body.sourceFeature = descriptor.id;
    body.shape = output.shape;
    referenceBody.sourceFeature = descriptor.id;
    referenceBody.shape = output.referenceShape;
    outputs.push_back(std::move(output));
}

} // namespace voelund::features::detail
