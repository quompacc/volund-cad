#include <voelund/features/FeatureGraph.hpp>

#include <voelund/document/ModelDocument.hpp>
#include <voelund/features/BoxFeature.hpp>
#include <voelund/features/CylinderFeature.hpp>
#include <voelund/features/SketchEvaluation.hpp>
#include <voelund/kernel/ShapeFactory.hpp>
#include <voelund/kernel/ShapeOperations.hpp>

#include <BRepBuilderAPI_Transform.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <algorithm>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>
#include <stdexcept>
#include <utility>
#include <vector>

namespace voelund::features {
namespace {

const TopoDS_Shape& findOutput(const std::vector<EvaluatedFeature>& outputs,
                               document::FeatureId featureId, bool reference = false) {
    const auto match = std::ranges::find(outputs, featureId, &EvaluatedFeature::id);
    if (match == outputs.end()) {
        throw std::runtime_error("Feature input has not been evaluated");
    }
    return reference ? match->referenceShape : match->shape;
}

EvaluatedBody& findBody(std::vector<EvaluatedBody>& bodies, document::FeatureId featureId) {
    const auto match = std::ranges::find(bodies, featureId, &EvaluatedBody::sourceFeature);
    if (match == bodies.end()) {
        throw std::runtime_error("Join input does not produce an editable body");
    }
    return *match;
}

TopoDS_Shape createResultShape(const std::vector<EvaluatedBody>& bodies) {
    if (bodies.empty()) {
        throw std::runtime_error("Feature graph produced no bodies");
    }
    if (bodies.size() == 1) {
        return bodies.front().shape;
    }
    std::vector<TopoDS_Shape> shapes;
    shapes.reserve(bodies.size());
    for (const EvaluatedBody& body : bodies) {
        shapes.push_back(body.shape);
    }
    return kernel::ShapeOperations::compound(shapes);
}

void evaluateBox(const document::ModelDocument& document,
                 const document::FeatureDescriptor& descriptor,
                 std::vector<EvaluatedFeature>& outputs, std::vector<EvaluatedBody>& bodies,
                 std::vector<EvaluatedBody>& referenceBodies) {
    const TopoDS_Shape shape = BoxFeature(descriptor.id).evaluate(document);
    const TopoDS_Shape referenceShape =
        kernel::ShapeFactory::makeBox(document.boxDimensions(descriptor.id));
    outputs.push_back(EvaluatedFeature{descriptor.id, shape, referenceShape});
    bodies.push_back(EvaluatedBody{descriptor.id, shape});
    referenceBodies.push_back(EvaluatedBody{descriptor.id, referenceShape});
}

TopoDS_Shape combineCylinder(document::CylinderResultMode mode, const TopoDS_Shape& input,
                             const TopoDS_Shape& tool) {
    switch (mode) {
    case document::CylinderResultMode::join:
        return kernel::ShapeOperations::fuse(input, tool);
    case document::CylinderResultMode::cut:
        return kernel::ShapeOperations::cut(input, tool);
    case document::CylinderResultMode::intersect:
        return kernel::ShapeOperations::intersect(input, tool);
    case document::CylinderResultMode::separate:
        break;
    }
    throw std::logic_error("Separate cylinders are not boolean operations");
}

void evaluateCylinder(const document::ModelDocument& document,
                      const document::FeatureDescriptor& descriptor,
                      std::vector<EvaluatedFeature>& outputs, std::vector<EvaluatedBody>& bodies,
                      std::vector<EvaluatedBody>& referenceBodies) {
    if (!descriptor.inputFeature.has_value()) {
        throw std::runtime_error("Cylinder feature requires an input feature");
    }
    const document::FeatureId inputId = *descriptor.inputFeature;
    const TopoDS_Shape& inputShape = findOutput(outputs, inputId);
    const TopoDS_Shape& referenceInputShape = findOutput(outputs, inputId, true);
    const CylinderFeature cylinder(descriptor.id);
    const document::CylinderParameters& parameters = document.cylinderParameters(descriptor.id);
    if (parameters.resultMode == document::CylinderResultMode::separate) {
        const TopoDS_Shape tool = cylinder.evaluateTool(document, referenceInputShape);
        const TopoDS_Shape referenceTool =
            cylinder.evaluateTool(document, referenceInputShape, false);
        outputs.push_back(EvaluatedFeature{descriptor.id, tool, referenceTool});
        bodies.push_back(EvaluatedBody{descriptor.id, tool});
        referenceBodies.push_back(EvaluatedBody{descriptor.id, referenceTool});
        return;
    }

    const TopoDS_Shape tool = cylinder.evaluateTool(document, inputShape);
    const TopoDS_Shape referenceTool = cylinder.evaluateTool(document, referenceInputShape);
    const TopoDS_Shape result = combineCylinder(parameters.resultMode, inputShape, tool);
    const TopoDS_Shape referenceResult =
        combineCylinder(parameters.resultMode, referenceInputShape, referenceTool);
    outputs.push_back(EvaluatedFeature{descriptor.id, result, referenceResult});
    EvaluatedBody& inputBody = findBody(bodies, inputId);
    inputBody = EvaluatedBody{descriptor.id, result};
    EvaluatedBody& referenceInputBody = findBody(referenceBodies, inputId);
    referenceInputBody = EvaluatedBody{descriptor.id, referenceResult};
}

void evaluateSketch(const document::ModelDocument& document,
                    const document::FeatureDescriptor& descriptor,
                    std::vector<EvaluatedFeature>& outputs) {
    const document::SketchParameters& parameters = document.sketchParameters(descriptor.id);
    TopoDS_Compound geometry;
    BRep_Builder builder;
    builder.MakeCompound(geometry);
    for (const EvaluatedSketchElement& element : evaluateSketchElements(parameters)) {
        builder.Add(geometry, element.shape);
    }
    outputs.push_back(EvaluatedFeature{descriptor.id, geometry, geometry});
}

void evaluateExtrusion(const document::ModelDocument& document,
                       const document::FeatureDescriptor& descriptor,
                       std::vector<EvaluatedFeature>& outputs, std::vector<EvaluatedBody>& bodies,
                       std::vector<EvaluatedBody>& referenceBodies) {
    if (!descriptor.inputFeature.has_value()) {
        throw std::runtime_error("Extrusion requires a sketch input");
    }
    const document::FeatureId sketchId = *descriptor.inputFeature;
    const document::SketchParameters& sketch = document.sketchParameters(sketchId);
    const TopoDS_Shape sketchFace = evaluateSketchProfileFaces(sketch);
    const document::ExtrusionParameters& parameters = document.extrusionParameters(descriptor.id);
    // Start just inside the support body. Repeated OCCT booleans can otherwise leave
    // microscopic gaps at a face that is mathematically coincident with the sketch.
    constexpr double joinOverlap = 1.0e-4;
    const gp_Vec outward(sketch.plane.normalX, sketch.plane.normalY, sketch.plane.normalZ);
    gp_Trsf inwardTransform;
    inwardTransform.SetTranslation(outward * -joinOverlap);
    const TopoDS_Shape overlappingFace =
        BRepBuilderAPI_Transform(sketchFace, inwardTransform, true).Shape();
    const gp_Vec direction = outward * (parameters.distance + joinOverlap);
    const TopoDS_Shape tool = BRepPrimAPI_MakePrism(overlappingFace, direction).Shape();

    const auto descriptors = document.features();
    const auto sketchDescriptor =
        std::ranges::find(descriptors, sketchId, &document::FeatureDescriptor::id);
    if (sketchDescriptor == descriptors.end() || !sketchDescriptor->inputFeature.has_value()) {
        throw std::runtime_error("Sketch support feature is missing");
    }
    const document::FeatureId supportId = *sketchDescriptor->inputFeature;
    EvaluatedBody& supportBody = findBody(bodies, supportId);
    EvaluatedBody& referenceSupportBody = findBody(referenceBodies, supportId);
    const TopoDS_Shape result = kernel::ShapeOperations::fuse(supportBody.shape, tool);
    const TopoDS_Shape referenceResult =
        kernel::ShapeOperations::fuse(referenceSupportBody.shape, tool);
    outputs.push_back(EvaluatedFeature{descriptor.id, result, referenceResult});
    supportBody = EvaluatedBody{descriptor.id, result};
    referenceSupportBody = EvaluatedBody{descriptor.id, referenceResult};
}

} // namespace

FeatureEvaluationError::FeatureEvaluationError(document::FeatureId featureId, std::string message)
    : std::runtime_error(std::move(message)), featureId_(featureId) {}

document::FeatureId FeatureEvaluationError::featureId() const noexcept { return featureId_; }

FeatureGraphResult FeatureGraph::evaluate(const document::ModelDocument& document) const {
    std::vector<EvaluatedFeature> outputs;
    std::vector<EvaluatedBody> bodies;
    std::vector<EvaluatedBody> referenceBodies;
    outputs.reserve(document.features().size());
    bodies.reserve(document.features().size());
    referenceBodies.reserve(document.features().size());
    for (const document::FeatureDescriptor& descriptor : document.features()) {
        try {
            switch (descriptor.kind) {
            case document::FeatureKind::box:
                evaluateBox(document, descriptor, outputs, bodies, referenceBodies);
                break;
            case document::FeatureKind::cylinder:
                evaluateCylinder(document, descriptor, outputs, bodies, referenceBodies);
                break;
            case document::FeatureKind::sketch:
                evaluateSketch(document, descriptor, outputs);
                break;
            case document::FeatureKind::extrusion:
                evaluateExtrusion(document, descriptor, outputs, bodies, referenceBodies);
                break;
            }
        } catch (const std::exception& error) {
            throw FeatureEvaluationError(descriptor.id, error.what());
        }
    }
    return FeatureGraphResult{createResultShape(bodies), std::move(bodies), std::move(outputs)};
}

} // namespace voelund::features
