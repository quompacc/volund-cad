#include "FeatureEvaluationDetail.hpp"
#include <Standard_OutOfMemory.hxx>
#include <Standard_ProgramError.hxx>
#include <voelund/features/FeatureGraph.hpp>
#include <voelund/features/GeometryEvaluationError.hpp>
#include <voelund/kernel/OcctCompatibility.hpp>

#include <voelund/document/ModelDocument.hpp>
#include <voelund/features/BoxFeature.hpp>
#include <voelund/features/CylinderFeature.hpp>
#include <voelund/features/SketchConstraintSolver.hpp>
#include <voelund/features/SketchEvaluation.hpp>
#include <voelund/features/SketchSupportResolver.hpp>
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

const EvaluatedFeature& findFeature(const std::vector<EvaluatedFeature>& outputs,
                                    document::FeatureId featureId) {
    const auto match = std::ranges::find(outputs, featureId, &EvaluatedFeature::id);
    if (match == outputs.end()) {
        throw GeometryEvaluationError("Feature input has not been evaluated");
    }
    return *match;
}

const TopoDS_Shape& findOutput(const std::vector<EvaluatedFeature>& outputs,
                               document::FeatureId featureId, bool reference = false) {
    const auto& output = findFeature(outputs, featureId);
    return reference ? output.referenceShape : output.shape;
}

EvaluatedBody& findBody(std::vector<EvaluatedBody>& bodies,
                        const std::vector<EvaluatedFeature>& outputs,
                        document::FeatureId featureId) {
    const auto bodyId = findFeature(outputs, featureId).bodyId;
    if (!bodyId)
        throw GeometryEvaluationError("Input feature has no solid body identity");
    const auto match = std::ranges::find(bodies, *bodyId, &EvaluatedBody::bodyId);
    if (match == bodies.end()) {
        throw GeometryEvaluationError("Join input does not produce an editable body");
    }
    // Identity does not select a historical shape version. Keep the existing
    // current-input restriction until R01 defines operations on historical results.
    if (match->sourceFeature != featureId)
        throw GeometryEvaluationError("Body input refers to an earlier feature result");
    return *match;
}

TopoDS_Shape createResultShape(const std::vector<EvaluatedBody>& bodies) {
    if (bodies.empty()) {
        throw GeometryEvaluationError("Feature graph produced no bodies");
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
    const document::BodyId bodyId{descriptor.id.value()};
    outputs.push_back(EvaluatedFeature{descriptor.id, shape, referenceShape, bodyId});
    bodies.push_back(EvaluatedBody{bodyId, descriptor.id, shape});
    referenceBodies.push_back(EvaluatedBody{bodyId, descriptor.id, referenceShape});
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
        throw GeometryEvaluationError("Cylinder feature requires an input feature");
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
        const document::BodyId bodyId{descriptor.id.value()};
        outputs.push_back(EvaluatedFeature{descriptor.id, tool, referenceTool, bodyId});
        bodies.push_back(EvaluatedBody{bodyId, descriptor.id, tool});
        referenceBodies.push_back(EvaluatedBody{bodyId, descriptor.id, referenceTool});
        return;
    }

    EvaluatedBody& inputBody = findBody(bodies, outputs, inputId);
    EvaluatedBody& referenceInputBody = findBody(referenceBodies, outputs, inputId);
    const TopoDS_Shape tool = cylinder.evaluateTool(document, inputShape);
    const TopoDS_Shape referenceTool = cylinder.evaluateTool(document, referenceInputShape);
    const TopoDS_Shape result = combineCylinder(parameters.resultMode, inputShape, tool);
    const TopoDS_Shape referenceResult =
        combineCylinder(parameters.resultMode, referenceInputShape, referenceTool);
    outputs.push_back(EvaluatedFeature{descriptor.id, result, referenceResult, inputBody.bodyId});
    inputBody.sourceFeature = descriptor.id;
    inputBody.shape = result;
    referenceInputBody.sourceFeature = descriptor.id;
    referenceInputBody.shape = referenceResult;
}

void evaluateSketch(const document::ModelDocument& document,
                    const document::FeatureDescriptor& descriptor,
                    std::vector<EvaluatedFeature>& outputs) {
    document::SketchParameters parameters = document.sketchParameters(descriptor.id);
    if (descriptor.inputFeature.has_value()) {
        if (!updateSketchSupportReferences(parameters,
                                           findOutput(outputs, *descriptor.inputFeature)))
            throw GeometryEvaluationError("Sketch support face is no longer available");
        if (solveSketchConstraints(parameters).status != SketchSolveStatus::success)
            throw GeometryEvaluationError("Sketch support references cannot be solved");
    }
    TopoDS_Compound geometry;
    BRep_Builder builder;
    builder.MakeCompound(geometry);
    for (const EvaluatedSketchElement& element : evaluateSketchElements(parameters)) {
        builder.Add(geometry, element.shape);
    }
    outputs.push_back(EvaluatedFeature{descriptor.id, geometry, geometry, std::nullopt});
}

void evaluateExtrusion(const document::ModelDocument& document,
                       const document::FeatureDescriptor& descriptor,
                       std::vector<EvaluatedFeature>& outputs, std::vector<EvaluatedBody>& bodies,
                       std::vector<EvaluatedBody>& referenceBodies) {
    if (!descriptor.inputFeature.has_value()) {
        throw GeometryEvaluationError("Extrusion requires a sketch input");
    }
    const document::FeatureId sketchId = *descriptor.inputFeature;
    document::SketchParameters sketch = document.sketchParameters(sketchId);
    const document::ExtrusionParameters& parameters = document.extrusionParameters(descriptor.id);
    const auto descriptors = document.features();
    const auto sketchDescriptor =
        std::ranges::find(descriptors, sketchId, &document::FeatureDescriptor::id);
    if (sketchDescriptor == descriptors.end() || !sketchDescriptor->inputFeature.has_value())
        throw GeometryEvaluationError("Sketch support feature is missing");
    const document::FeatureId supportId = *sketchDescriptor->inputFeature;
    if (!updateSketchSupportReferences(sketch, findOutput(outputs, supportId)))
        throw GeometryEvaluationError("Sketch support face is no longer available");
    if (solveSketchConstraints(sketch).status != SketchSolveStatus::success)
        throw GeometryEvaluationError("Sketch support references cannot be solved");
    const TopoDS_Shape sketchFace = evaluateSketchProfileFaces(sketch, parameters.profiles);
    // Exact support-plane contact: preserve both the start plane and the requested
    // end plane. OCCT's Boolean classifies the coincident faces; no length padding.
    const gp_Vec outward(sketch.plane.normalX, sketch.plane.normalY, sketch.plane.normalZ);
    const TopoDS_Shape tool = BRepPrimAPI_MakePrism(sketchFace, outward * parameters.distance).Shape();

    EvaluatedBody& supportBody = findBody(bodies, outputs, supportId);
    EvaluatedBody& referenceSupportBody = findBody(referenceBodies, outputs, supportId);
    const TopoDS_Shape result = kernel::ShapeOperations::fuse(supportBody.shape, tool);
    const TopoDS_Shape referenceResult =
        kernel::ShapeOperations::fuse(referenceSupportBody.shape, tool);
    outputs.push_back(EvaluatedFeature{descriptor.id, result, referenceResult, supportBody.bodyId});
    supportBody.sourceFeature = descriptor.id;
    supportBody.shape = result;
    referenceSupportBody.sourceFeature = descriptor.id;
    referenceSupportBody.shape = referenceResult;
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
            if (detail::beforeFeatureEvaluation)
                detail::beforeFeatureEvaluation(descriptor.id);
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
        } catch (const GeometryEvaluationError& error) {
            throw FeatureEvaluationError(descriptor.id, error.what());
        } catch (const kernel::KernelError& error) {
            throw FeatureEvaluationError(descriptor.id, error.what());
        } catch (const Standard_OutOfMemory&) {
            throw;
        } catch (const Standard_ProgramError&) {
            throw;
        } catch (const Standard_Failure& failure) {
            throw FeatureEvaluationError(descriptor.id, kernel::occtFailureDiagnostic(failure));
        }
    }
    return FeatureGraphResult{createResultShape(bodies), std::move(bodies), std::move(outputs)};
}

} // namespace voelund::features
