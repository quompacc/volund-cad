#include "FeatureEvaluationDetail.hpp"

#include <voelund/document/DocumentSnapshot.hpp>
#include <voelund/features/FeatureDependencies.hpp>
#include <voelund/features/GeometryEvaluationError.hpp>
#include <voelund/kernel/ShapeFactory.hpp>

#include <TopoDS.hxx>

// M11 (user decision 2026-10-15): moves and turns a body as it is before the move, about the
// stored pivot like a primitive's placement. Faces and edges keep their names; the frames of
// faces move along, so sketches bound to them follow. The unplaced reference stays unmoved.
namespace voelund::features::detail {

void evaluateMove(const document::DocumentSnapshot& document,
                  const document::FeatureDescriptor& descriptor,
                  std::vector<EvaluatedFeature>& outputs, std::vector<EvaluatedBody>& bodies,
                  std::vector<EvaluatedBody>& referenceBodies) {
    const auto& parameters = document.moveParameters(descriptor.id);
    const auto producer = bodyProducerBefore(document, descriptor.id, parameters.body);
    if (!producer)
        throw GeometryEvaluationError("The body of the move has no solid before it",
                                      "move-body-missing");
    const auto& owner = findFeature(outputs, *producer);
    EvaluatedBody& body = findBody(bodies, outputs, *producer);
    EvaluatedBody& referenceBody = findBody(referenceBodies, outputs, *producer);
    const auto& [x, y, z] = parameters.pivot;
    const auto moved = [&](const TopoDS_Shape& shape) {
        return kernel::ShapeFactory::applyTransform(shape, x, y, z, parameters.transform)
            .Oriented(shape.Orientation());
    };
    const auto evaluated =
        kernel::ShapeFactory::applyTransformWithHistory(body.shape, x, y, z, parameters.transform);
    auto faces = continueSupportOrigins(owner.supportOrigins, evaluated);
    for (auto& face : faces)
        if (!face.frameFace.IsNull())
            face.frameFace = moved(face.frameFace);
    auto edges = continueEdgeOrigins(owner.edgeOrigins, evaluated);
    for (auto& edge : edges)
        edge.source = TopoDS::Edge(moved(edge.source));
    EvaluatedFeature output{descriptor.id,
                            evaluated.shape,
                            referenceBody.shape,
                            body.bodyId,
                            std::move(faces),
                            {},
                            owner.referenceSupportOrigins,
                            std::move(edges),
                            owner.referenceEdgeOrigins};
    body.sourceFeature = descriptor.id;
    body.shape = output.shape;
    referenceBody.sourceFeature = descriptor.id;
    outputs.push_back(std::move(output));
}

} // namespace voelund::features::detail
