#include "FeatureEvaluationDetail.hpp"
#include <Standard_OutOfMemory.hxx>
#include <Standard_ProgramError.hxx>
#include <voelund/features/FeatureGraph.hpp>
#include <voelund/features/GeometryEvaluationError.hpp>
#include <voelund/kernel/OcctCompatibility.hpp>

#include <voelund/document/DocumentSnapshot.hpp>
#include <voelund/document/LegacyBodyIdentity.hpp>
#include <voelund/features/BoxFeature.hpp>
#include <voelund/features/ContourProvenance.hpp>
#include <voelund/features/CylinderFeature.hpp>
#include <voelund/features/SketchConstraintSolver.hpp>
#include <voelund/features/SketchEvaluation.hpp>
#include <voelund/features/SketchSupportBinding.hpp>
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

void evaluateBox(const document::DocumentSnapshot& document,
                 const document::FeatureDescriptor& descriptor,
                 std::vector<EvaluatedFeature>& outputs, std::vector<EvaluatedBody>& bodies,
                 std::vector<EvaluatedBody>& referenceBodies) {
    const auto evaluated = BoxFeature(descriptor.id).evaluateWithHistory(document);
    const TopoDS_Shape shape = evaluated.shape;
    const auto reference =
        kernel::ShapeFactory::makeBoxWithHistory(document.boxDimensions(descriptor.id));
    const TopoDS_Shape referenceShape = reference.shape;
    const document::BodyId bodyId{descriptor.id.value()};
    outputs.push_back(EvaluatedFeature{descriptor.id,
                                       shape,
                                       referenceShape,
                                       bodyId,
                                       primitiveSupportOrigins(descriptor.id, evaluated),
                                       {},
                                       primitiveSupportOrigins(descriptor.id, reference),
                                       primitiveEdgeOrigins(descriptor.id, evaluated),
                                       primitiveEdgeOrigins(descriptor.id, reference)});
    bodies.push_back(EvaluatedBody{bodyId, descriptor.id, shape});
    referenceBodies.push_back(EvaluatedBody{bodyId, descriptor.id, referenceShape});
}

kernel::ShapeOperationResult combineCylinder(document::CylinderResultMode mode,
                                             const TopoDS_Shape& input, const TopoDS_Shape& tool) {
    switch (mode) {
    case document::CylinderResultMode::join:
        return kernel::ShapeOperations::fuseWithHistory(input, tool);
    case document::CylinderResultMode::cut:
        return kernel::ShapeOperations::cutWithHistory(input, tool);
    case document::CylinderResultMode::intersect:
        return kernel::ShapeOperations::intersectWithHistory(input, tool);
    case document::CylinderResultMode::separate:
        break;
    }
    throw std::logic_error("Separate cylinders are not boolean operations");
}

void evaluateCylinder(const document::DocumentSnapshot& document,
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
        const auto evaluatedTool = cylinder.evaluateToolWithHistory(document, referenceInputShape);
        const TopoDS_Shape tool = evaluatedTool.shape;
        const auto reference =
            cylinder.evaluateToolWithHistory(document, referenceInputShape, false);
        const TopoDS_Shape referenceTool = reference.shape;
        const document::BodyId bodyId{descriptor.id.value()};
        outputs.push_back(EvaluatedFeature{descriptor.id,
                                           tool,
                                           referenceTool,
                                           bodyId,
                                           primitiveSupportOrigins(descriptor.id, evaluatedTool),
                                           {},
                                           primitiveSupportOrigins(descriptor.id, reference),
                                           primitiveEdgeOrigins(descriptor.id, evaluatedTool),
                                           primitiveEdgeOrigins(descriptor.id, reference)});
        bodies.push_back(EvaluatedBody{bodyId, descriptor.id, tool});
        referenceBodies.push_back(EvaluatedBody{bodyId, descriptor.id, referenceTool});
        return;
    }

    EvaluatedBody& inputBody = findBody(bodies, outputs, inputId);
    EvaluatedBody& referenceInputBody = findBody(referenceBodies, outputs, inputId);
    const auto evaluatedTool = cylinder.evaluateToolWithHistory(document, inputShape);
    const TopoDS_Shape tool = evaluatedTool.shape;
    const auto referenceTool = cylinder.evaluateToolWithHistory(document, referenceInputShape);
    const auto evaluated = combineCylinder(parameters.resultMode, inputShape, tool);
    const TopoDS_Shape result = evaluated.shape;
    const auto reference =
        combineCylinder(parameters.resultMode, referenceInputShape, referenceTool.shape);
    const TopoDS_Shape referenceResult = reference.shape;
    auto referenceOrigins =
        continueSupportOrigins(findFeature(outputs, inputId).referenceSupportOrigins, reference);
    auto referenceToolOrigins =
        continueSupportOrigins(primitiveSupportOrigins(descriptor.id, referenceTool), reference,
                               kernel::HistoryOperand::second);
    referenceOrigins.insert(referenceOrigins.end(), referenceToolOrigins.begin(),
                            referenceToolOrigins.end());
    auto origins = continueSupportOrigins(findFeature(outputs, inputId).supportOrigins, evaluated);
    auto toolOrigins = continueSupportOrigins(primitiveSupportOrigins(descriptor.id, evaluatedTool),
                                              evaluated, kernel::HistoryOperand::second);
    origins.insert(origins.end(), toolOrigins.begin(), toolOrigins.end());
    auto edges = continueEdgeOrigins(findFeature(outputs, inputId).edgeOrigins, evaluated);
    const auto toolEdges = continueEdgeOrigins(primitiveEdgeOrigins(descriptor.id, evaluatedTool),
                                               evaluated, kernel::HistoryOperand::second);
    edges.insert(edges.end(), toolEdges.begin(), toolEdges.end());
    auto referenceEdges =
        continueEdgeOrigins(findFeature(outputs, inputId).referenceEdgeOrigins, reference);
    const auto referenceToolEdges =
        continueEdgeOrigins(primitiveEdgeOrigins(descriptor.id, referenceTool), reference,
                            kernel::HistoryOperand::second);
    referenceEdges.insert(referenceEdges.end(), referenceToolEdges.begin(),
                          referenceToolEdges.end());
    outputs.push_back(EvaluatedFeature{descriptor.id,
                                       result,
                                       referenceResult,
                                       inputBody.bodyId,
                                       std::move(origins),
                                       {},
                                       std::move(referenceOrigins),
                                       std::move(edges),
                                       std::move(referenceEdges)});
    inputBody.sourceFeature = descriptor.id;
    inputBody.shape = result;
    referenceInputBody.sourceFeature = descriptor.id;
    referenceInputBody.shape = referenceResult;
}

void evaluateSketch(const document::DocumentSnapshot& document,
                    const document::FeatureDescriptor& descriptor,
                    std::vector<EvaluatedFeature>& outputs) {
    document::SketchParameters parameters = document.sketchParameters(descriptor.id);
    if (parameters.repairReason)
        throw GeometryEvaluationError(
            "MigrationNeedsRepair: explicit sketch reference repair required");
    if (descriptor.inputFeature.has_value()) {
        if (parameters.supportBinding)
            resolveBoundSketchSupport(parameters, findFeature(outputs, *descriptor.inputFeature));
        else if (!updateSketchSupportReferences(parameters,
                                                findOutput(outputs, *descriptor.inputFeature)))
            throw GeometryEvaluationError("Sketch support face is no longer available");
        if (solveSketchConstraints(parameters).status != SketchSolveStatus::success)
            throw GeometryEvaluationError("Sketch support references cannot be solved");
    }
    TopoDS_Compound geometry;
    BRep_Builder builder;
    builder.MakeCompound(geometry);
    const auto elements = evaluateSketchElements(parameters);
    for (const EvaluatedSketchElement& element : elements) {
        builder.Add(geometry, element.shape);
    }
    outputs.push_back(EvaluatedFeature{descriptor.id,
                                       geometry,
                                       geometry,
                                       std::nullopt,
                                       {},
                                       parameters,
                                       {},
                                       sketchEdgeOrigins(descriptor.id, elements)});
}

void evaluateExtrusion(const document::DocumentSnapshot& document,
                       const document::FeatureDescriptor& descriptor,
                       std::vector<EvaluatedFeature>& outputs, std::vector<EvaluatedBody>& bodies,
                       std::vector<EvaluatedBody>& referenceBodies) {
    if (!descriptor.inputFeature.has_value()) {
        throw GeometryEvaluationError("Extrusion requires a sketch input");
    }
    const document::FeatureId sketchId = *descriptor.inputFeature;
    const auto& projected = findFeature(outputs, sketchId).resolvedSketch;
    if (!projected)
        throw GeometryEvaluationError("Current sketch projection is missing");
    const document::SketchParameters& sketch = *projected;
    const document::ExtrusionParameters& parameters = document.extrusionParameters(descriptor.id);
    const auto descriptors = document.features();
    const auto sketchDescriptor =
        std::ranges::find(descriptors, sketchId, &document::FeatureDescriptor::id);
    if (sketchDescriptor == descriptors.end() || !sketchDescriptor->inputFeature.has_value())
        throw GeometryEvaluationError("Sketch support feature is missing");
    const document::FeatureId supportId = *sketchDescriptor->inputFeature;
    const auto profiles = evaluateExtrusionFaces(sketch, parameters);
    TopoDS_Compound sketchFace;
    BRep_Builder builder;
    builder.MakeCompound(sketchFace);
    for (const auto& profile : profiles)
        builder.Add(sketchFace, profile.face);
    // Exact support-plane contact: preserve both the start plane and the requested
    // end plane. OCCT's Boolean classifies the coincident faces; no length padding.
    const gp_Vec outward(sketch.plane.normalX, sketch.plane.normalY, sketch.plane.normalZ);
    BRepPrimAPI_MakePrism prism(sketchFace, outward * parameters.distance);
    const TopoDS_Shape tool = prism.Shape();
    const auto toolEdges = extrusionEdgeOrigins(descriptor.id, sketchId, profiles, prism);
    std::vector<SupportProvenance> capOrigins;
    for (const auto& profile : profiles) {
        const auto cap = prism.LastShape(profile.face);
        capOrigins.push_back({{descriptor.id, std::nullopt, profile.boundary},
                              {cap},
                              kernel::HistoryCoverage::complete,
                              cap});
    }

    EvaluatedBody& supportBody = findBody(bodies, outputs, supportId);
    EvaluatedBody& referenceSupportBody = findBody(referenceBodies, outputs, supportId);
    const auto evaluated = kernel::ShapeOperations::fuseWithHistory(supportBody.shape, tool);
    const TopoDS_Shape result = evaluated.shape;
    auto referenceSketch = sketch;
    if (sketch.supportBinding) {
        const auto& support = findFeature(outputs, supportId);
        const EvaluatedFeature referenceSupport{support.id,
                                                support.referenceShape,
                                                support.referenceShape,
                                                support.bodyId,
                                                support.referenceSupportOrigins,
                                                {},
                                                {},
                                                support.referenceEdgeOrigins};
        resolveBoundSketchSupport(referenceSketch, referenceSupport);
        if (solveSketchConstraints(referenceSketch).status != SketchSolveStatus::success)
            throw GeometryEvaluationError("Reference sketch projected edges cannot be solved");
    }
    const auto referenceProfiles = evaluateExtrusionFaces(referenceSketch, parameters);
    TopoDS_Compound referenceFaces;
    builder.MakeCompound(referenceFaces);
    for (const auto& profile : referenceProfiles)
        builder.Add(referenceFaces, profile.face);
    BRepPrimAPI_MakePrism referencePrism(referenceFaces, gp_Vec(referenceSketch.plane.normalX,
                                                                referenceSketch.plane.normalY,
                                                                referenceSketch.plane.normalZ) *
                                                             parameters.distance);
    const auto reference = kernel::ShapeOperations::fuseWithHistory(referenceSupportBody.shape,
                                                                    referencePrism.Shape());
    const auto referenceToolEdges =
        extrusionEdgeOrigins(descriptor.id, sketchId, referenceProfiles, referencePrism);
    const TopoDS_Shape referenceResult = reference.shape;
    auto referenceOrigins =
        continueSupportOrigins(findFeature(outputs, supportId).referenceSupportOrigins, reference);
    std::vector<SupportProvenance> referenceCaps;
    for (const auto& profile : referenceProfiles) {
        const auto cap = referencePrism.LastShape(profile.face);
        referenceCaps.push_back({{descriptor.id, std::nullopt, profile.boundary},
                                 {cap},
                                 kernel::HistoryCoverage::complete,
                                 cap});
    }
    const auto continuedCaps =
        continueSupportOrigins(referenceCaps, reference, kernel::HistoryOperand::second);
    referenceOrigins.insert(referenceOrigins.end(), continuedCaps.begin(), continuedCaps.end());
    auto origins =
        continueSupportOrigins(findFeature(outputs, supportId).supportOrigins, evaluated);
    auto caps = continueSupportOrigins(capOrigins, evaluated, kernel::HistoryOperand::second);
    origins.insert(origins.end(), caps.begin(), caps.end());
    auto edges = continueEdgeOrigins(findFeature(outputs, supportId).edgeOrigins, evaluated);
    const auto continuedToolEdges =
        continueEdgeOrigins(toolEdges, evaluated, kernel::HistoryOperand::second);
    edges.insert(edges.end(), continuedToolEdges.begin(), continuedToolEdges.end());
    auto referenceEdges =
        continueEdgeOrigins(findFeature(outputs, supportId).referenceEdgeOrigins, reference);
    const auto continuedReferenceEdges =
        continueEdgeOrigins(referenceToolEdges, reference, kernel::HistoryOperand::second);
    referenceEdges.insert(referenceEdges.end(), continuedReferenceEdges.begin(),
                          continuedReferenceEdges.end());
    outputs.push_back(EvaluatedFeature{descriptor.id,
                                       result,
                                       referenceResult,
                                       supportBody.bodyId,
                                       std::move(origins),
                                       {},
                                       std::move(referenceOrigins),
                                       std::move(edges),
                                       std::move(referenceEdges)});
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
    return evaluate(document::DocumentSnapshot::capture(document));
}

bool FeatureGraphEvaluation::valid() const noexcept {
    return std::ranges::all_of(
        report.features, [](const auto& issue) { return issue.status == EvaluationStatus::valid; });
}

namespace {
void runFeature(const document::DocumentSnapshot& document,
                const document::FeatureDescriptor& descriptor,
                std::vector<EvaluatedFeature>& outputs, std::vector<EvaluatedBody>& bodies,
                std::vector<EvaluatedBody>& references) {
    if (detail::beforeFeatureEvaluation)
        detail::beforeFeatureEvaluation(descriptor.id);
    switch (descriptor.kind) {
    case document::FeatureKind::box:
        evaluateBox(document, descriptor, outputs, bodies, references);
        break;
    case document::FeatureKind::cylinder:
        evaluateCylinder(document, descriptor, outputs, bodies, references);
        break;
    case document::FeatureKind::sketch:
        evaluateSketch(document, descriptor, outputs);
        break;
    case document::FeatureKind::extrusion:
        evaluateExtrusion(document, descriptor, outputs, bodies, references);
        break;
    }
}
void restoreOutput(const EvaluatedFeature& output, std::vector<EvaluatedFeature>& outputs,
                   std::vector<EvaluatedBody>& bodies, std::vector<EvaluatedBody>& references) {
    outputs.push_back(output);
    if (!output.bodyId)
        return;
    const auto restore = [&](auto& list, const auto& shape) {
        const auto found = std::ranges::find(list, *output.bodyId, &EvaluatedBody::bodyId);
        if (found == list.end())
            list.push_back({*output.bodyId, output.id, shape});
        else
            *found = {*output.bodyId, output.id, shape};
    };
    restore(bodies, output.shape);
    restore(references, output.referenceShape);
}
std::vector<bool> dirtyNodes(const FeatureGraphEvaluation& next,
                             const std::shared_ptr<const FeatureGraphEvaluation>& previous) {
    const auto features = next.snapshot.features();
    if (!previous || !std::ranges::equal(next.dependencies.nodes(), previous->dependencies.nodes()))
        return std::vector<bool>(features.size(), true);
    std::vector<document::FeatureId> changed;
    for (std::size_t i = 0; i < features.size(); ++i)
        if (features[i] != previous->snapshot.features()[i] ||
            next.snapshot.parameters()[i] != previous->snapshot.parameters()[i])
            changed.push_back(features[i].id);
    return next.dependencies.affected(changed);
}
void runChecked(const document::DocumentSnapshot& document,
                const document::FeatureDescriptor& descriptor, EvaluationIssue& issue,
                std::vector<EvaluatedFeature>& outputs, std::vector<EvaluatedBody>& bodies,
                std::vector<EvaluatedBody>& references) {
    const auto failure = [&](const std::string& message) {
        issue.status = EvaluationStatus::failed;
        issue.code = "feature-evaluation-failed";
        issue.message = message;
    };
    try {
        runFeature(document, descriptor, outputs, bodies, references);
    } catch (const GeometryEvaluationError& error) {
        failure(error.what());
    } catch (const kernel::KernelError& error) {
        failure(error.what());
    } catch (const Standard_OutOfMemory&) {
        throw;
    } catch (const Standard_ProgramError&) {
        throw;
    } catch (const Standard_Failure& error) {
        failure(kernel::occtFailureDiagnostic(error));
    }
}
} // namespace

std::shared_ptr<const FeatureGraphEvaluation> FeatureGraph::evaluateDetailed(
    const document::DocumentSnapshot& document,
    const std::shared_ptr<const FeatureGraphEvaluation>& previous) const {
    auto next = std::make_shared<FeatureGraphEvaluation>(
        FeatureGraphEvaluation{document, FeatureDependencies::from(document), {}, {}});
    const auto dirty = dirtyNodes(*next, previous);
    auto& result = next->result;
    std::vector<EvaluatedBody> references;
    for (const auto& feature : document.features())
        next->report.features.push_back({feature.id, EvaluationStatus::valid, {}, {}, {}});
    for (const auto index : next->dependencies.order()) {
        const auto& descriptor = document.features()[index];
        auto& issue = next->report.features[index];
        for (const auto input : next->dependencies.nodes()[index].inputs) {
            const auto found =
                std::ranges::find(next->report.features, input, &EvaluationIssue::feature);
            if (found->status != EvaluationStatus::valid) {
                issue = {descriptor.id,
                         EvaluationStatus::blockedByDependency,
                         "blocked-by-dependency",
                         "Blocked by failed input " + std::to_string(input.value()),
                         {}};
                break;
            }
        }
        if (issue.status != EvaluationStatus::valid)
            continue;
        if (!dirty[index]) {
            issue = previous->report.features[index];
            if (issue.status == EvaluationStatus::valid)
                restoreOutput(findFeature(previous->result.features, descriptor.id),
                              result.features, result.bodies, references);
        } else
            runChecked(document, descriptor, issue, result.features, result.bodies, references);
    }
    // A failed body modifier must not expose an older producer as a current final body.
    const auto identities =
        document::legacyBodyIdentities(document.features(), document.parameters());
    for (std::size_t i = 0; i < identities.size(); ++i)
        if (identities[i].body && next->report.features[i].status != EvaluationStatus::valid)
            std::erase_if(result.bodies,
                          [&](const auto& body) { return body.bodyId == identities[i].body; });
    if (!result.bodies.empty())
        result.resultShape = createResultShape(result.bodies);
    return next;
}

FeatureGraphResult FeatureGraph::evaluate(const document::DocumentSnapshot& document) const {
    const auto evaluated = evaluateDetailed(document);
    for (const auto& issue : evaluated->report.features)
        if (issue.status == EvaluationStatus::failed)
            throw FeatureEvaluationError(issue.feature, issue.message);
    return evaluated->result;
}
} // namespace voelund::features
