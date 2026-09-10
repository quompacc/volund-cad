#include <voelund/features/FeatureGraph.hpp>

#include <voelund/document/ModelDocument.hpp>
#include <voelund/features/BoxFeature.hpp>
#include <voelund/features/CylinderFeature.hpp>
#include <voelund/kernel/ShapeOperations.hpp>

#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>
#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>

namespace voelund::features {
namespace {

struct EvaluatedFeature final {
    document::FeatureId id;
    TopoDS_Shape shape;
};

const TopoDS_Shape& findOutput(
    const std::vector<EvaluatedFeature>& outputs,
    document::FeatureId featureId)
{
    const auto match = std::ranges::find(outputs, featureId, &EvaluatedFeature::id);
    if (match == outputs.end()) {
        throw std::runtime_error("Feature input has not been evaluated");
    }
    return match->shape;
}

EvaluatedBody& findBody(
    std::vector<EvaluatedBody>& bodies,
    document::FeatureId featureId)
{
    const auto match = std::ranges::find(bodies, featureId, &EvaluatedBody::sourceFeature);
    if (match == bodies.end()) {
        throw std::runtime_error("Join input does not produce an editable body");
    }
    return *match;
}

TopoDS_Shape createResultShape(const std::vector<EvaluatedBody>& bodies)
{
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

void evaluateBox(
    const document::ModelDocument& document,
    const document::FeatureDescriptor& descriptor,
    std::vector<EvaluatedFeature>& outputs,
    std::vector<EvaluatedBody>& bodies)
{
    const TopoDS_Shape shape = BoxFeature(descriptor.id).evaluate(document);
    outputs.push_back(EvaluatedFeature{descriptor.id, shape});
    bodies.push_back(EvaluatedBody{descriptor.id, shape});
}

TopoDS_Shape combineCylinder(
    document::CylinderResultMode mode,
    const TopoDS_Shape& input,
    const TopoDS_Shape& tool)
{
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

void evaluateCylinder(
    const document::ModelDocument& document,
    const document::FeatureDescriptor& descriptor,
    std::vector<EvaluatedFeature>& outputs,
    std::vector<EvaluatedBody>& bodies)
{
    if (!descriptor.inputFeature.has_value()) {
        throw std::runtime_error("Cylinder feature requires an input feature");
    }
    const document::FeatureId inputId = *descriptor.inputFeature;
    const TopoDS_Shape& inputShape = findOutput(outputs, inputId);
    const TopoDS_Shape tool = CylinderFeature(descriptor.id).evaluateTool(document, inputShape);
    const document::CylinderParameters& parameters = document.cylinderParameters(descriptor.id);
    if (parameters.resultMode == document::CylinderResultMode::separate) {
        outputs.push_back(EvaluatedFeature{descriptor.id, tool});
        bodies.push_back(EvaluatedBody{descriptor.id, tool});
        return;
    }

    const TopoDS_Shape result = combineCylinder(parameters.resultMode, inputShape, tool);
    outputs.push_back(EvaluatedFeature{descriptor.id, result});
    EvaluatedBody& inputBody = findBody(bodies, inputId);
    inputBody = EvaluatedBody{descriptor.id, result};
}

gp_Pnt sketchPoint(
    const document::SketchPlane& plane,
    double x,
    double y)
{
    const gp_Dir normal(plane.normalX, plane.normalY, plane.normalZ);
    const gp_Dir xDirection(plane.xDirectionX, plane.xDirectionY, plane.xDirectionZ);
    const gp_Dir yDirection = normal.Crossed(xDirection);
    return gp_Pnt(plane.originX, plane.originY, plane.originZ)
        .Translated(gp_Vec(xDirection) * x + gp_Vec(yDirection) * y);
}

void evaluateSketch(
    const document::ModelDocument& document,
    const document::FeatureDescriptor& descriptor,
    std::vector<EvaluatedFeature>& outputs)
{
    const document::SketchParameters& parameters =
        document.sketchParameters(descriptor.id);
    const document::SketchRectangle& rectangle = parameters.rectangle;
    BRepBuilderAPI_MakePolygon polygon;
    polygon.Add(sketchPoint(parameters.plane, rectangle.firstX, rectangle.firstY));
    polygon.Add(sketchPoint(parameters.plane, rectangle.secondX, rectangle.firstY));
    polygon.Add(sketchPoint(parameters.plane, rectangle.secondX, rectangle.secondY));
    polygon.Add(sketchPoint(parameters.plane, rectangle.firstX, rectangle.secondY));
    polygon.Close();
    const TopoDS_Shape face = BRepBuilderAPI_MakeFace(polygon.Wire()).Face();
    outputs.push_back(EvaluatedFeature{descriptor.id, face});
}

void evaluateExtrusion(
    const document::ModelDocument& document,
    const document::FeatureDescriptor& descriptor,
    std::vector<EvaluatedFeature>& outputs,
    std::vector<EvaluatedBody>& bodies)
{
    if (!descriptor.inputFeature.has_value()) {
        throw std::runtime_error("Extrusion requires a sketch input");
    }
    const document::FeatureId sketchId = *descriptor.inputFeature;
    const TopoDS_Shape& sketchFace = findOutput(outputs, sketchId);
    const document::SketchParameters& sketch = document.sketchParameters(sketchId);
    const document::ExtrusionParameters& parameters =
        document.extrusionParameters(descriptor.id);
    // Start just inside the support body. Repeated OCCT booleans can otherwise leave
    // microscopic gaps at a face that is mathematically coincident with the sketch.
    constexpr double joinOverlap = 1.0e-4;
    const gp_Vec outward(
        sketch.plane.normalX, sketch.plane.normalY, sketch.plane.normalZ);
    gp_Trsf inwardTransform;
    inwardTransform.SetTranslation(outward * -joinOverlap);
    const TopoDS_Shape overlappingFace =
        BRepBuilderAPI_Transform(sketchFace, inwardTransform, true).Shape();
    const gp_Vec direction = outward * (parameters.distance + joinOverlap);
    const TopoDS_Shape tool =
        BRepPrimAPI_MakePrism(overlappingFace, direction).Shape();

    const auto descriptors = document.features();
    const auto sketchDescriptor =
        std::ranges::find(descriptors, sketchId, &document::FeatureDescriptor::id);
    if (sketchDescriptor == descriptors.end() || !sketchDescriptor->inputFeature.has_value()) {
        throw std::runtime_error("Sketch support feature is missing");
    }
    const document::FeatureId supportId = *sketchDescriptor->inputFeature;
    EvaluatedBody& supportBody = findBody(bodies, supportId);
    const TopoDS_Shape result = kernel::ShapeOperations::fuse(supportBody.shape, tool);
    outputs.push_back(EvaluatedFeature{descriptor.id, result});
    supportBody = EvaluatedBody{descriptor.id, result};
}

} // namespace

FeatureEvaluationError::FeatureEvaluationError(
    document::FeatureId featureId,
    std::string message)
    : std::runtime_error(std::move(message))
    , featureId_(featureId)
{
}

document::FeatureId FeatureEvaluationError::featureId() const noexcept
{
    return featureId_;
}

FeatureGraphResult FeatureGraph::evaluate(const document::ModelDocument& document) const
{
    std::vector<EvaluatedFeature> outputs;
    std::vector<EvaluatedBody> bodies;
    outputs.reserve(document.features().size());
    bodies.reserve(document.features().size());
    for (const document::FeatureDescriptor& descriptor : document.features()) {
        try {
            switch (descriptor.kind) {
            case document::FeatureKind::box:
                evaluateBox(document, descriptor, outputs, bodies);
                break;
            case document::FeatureKind::cylinder:
                evaluateCylinder(document, descriptor, outputs, bodies);
                break;
            case document::FeatureKind::sketch:
                evaluateSketch(document, descriptor, outputs);
                break;
            case document::FeatureKind::extrusion:
                evaluateExtrusion(document, descriptor, outputs, bodies);
                break;
            }
        } catch (const std::exception& error) {
            throw FeatureEvaluationError(descriptor.id, error.what());
        }
    }
    return FeatureGraphResult{createResultShape(bodies), std::move(bodies)};
}

} // namespace voelund::features
