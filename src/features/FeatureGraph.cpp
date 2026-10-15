#include "FeatureEvaluationDetail.hpp"
#include "PrimitivePlacement.hpp"
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
#include <voelund/kernel/SolidPrimitives.hpp>

#include <BRepBuilderAPI_Transform.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <algorithm>
#include <functional>
#include <gp_Trsf.hxx>
#include <stdexcept>
#include <utility>
#include <vector>

namespace voelund::features {
namespace detail {

const EvaluatedFeature& findFeature(const std::vector<EvaluatedFeature>& outputs,
                                    document::FeatureId featureId) {
    const auto match = std::ranges::find(outputs, featureId, &EvaluatedFeature::id);
    if (match == outputs.end()) {
        throw GeometryEvaluationError("Feature input has not been evaluated");
    }
    return *match;
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

} // namespace detail

namespace {
using detail::findBody;
using detail::findFeature;

const TopoDS_Shape& findOutput(const std::vector<EvaluatedFeature>& outputs,
                               document::FeatureId featureId, bool reference = false) {
    const auto& output = findFeature(outputs, featureId);
    return reference ? output.referenceShape : output.shape;
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

// A cylinder or M27 primitive: a tool built on its input (or at the origin), placed, and
// either a body of its own or joined to, cut from or intersected with the input body.
using PrimitiveTool =
    std::function<kernel::ShapeOperationResult(const TopoDS_Shape& input, bool applyPlacement)>;
void evaluatePrimitiveTool(document::CylinderResultMode mode, const PrimitiveTool& build,
                           const document::FeatureDescriptor& descriptor,
                           std::vector<EvaluatedFeature>& outputs,
                           std::vector<EvaluatedBody>& bodies,
                           std::vector<EvaluatedBody>& referenceBodies) {
    // M01: a new-body cylinder without input stands at the origin.
    static const TopoDS_Shape none;
    if (!descriptor.inputFeature.has_value() && mode != document::CylinderResultMode::separate)
        throw GeometryEvaluationError(descriptor.kind == document::FeatureKind::cylinder
                                          ? "Cylinder feature requires an input feature"
                                          : "Primitive feature requires an input feature");
    const auto input = descriptor.inputFeature;
    const TopoDS_Shape& inputShape = input ? findOutput(outputs, *input) : none;
    const TopoDS_Shape& referenceInputShape = input ? findOutput(outputs, *input, true) : none;
    if (mode == document::CylinderResultMode::separate) {
        const auto evaluatedTool = build(referenceInputShape, true);
        const TopoDS_Shape tool = evaluatedTool.shape;
        const auto reference = build(referenceInputShape, false);
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
    const document::FeatureId inputId = *input; // checked above for the boolean modes

    EvaluatedBody& inputBody = findBody(bodies, outputs, inputId);
    EvaluatedBody& referenceInputBody = findBody(referenceBodies, outputs, inputId);
    const auto evaluatedTool = build(inputShape, true);
    const TopoDS_Shape tool = evaluatedTool.shape;
    const auto referenceTool = build(referenceInputShape, true);
    const auto evaluated = combineCylinder(mode, inputShape, tool);
    const TopoDS_Shape result = evaluated.shape;
    const auto reference =
        combineCylinder(mode, referenceInputShape, referenceTool.shape);
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

void evaluateCylinder(const document::DocumentSnapshot& document,
                      const document::FeatureDescriptor& descriptor,
                      std::vector<EvaluatedFeature>& outputs, std::vector<EvaluatedBody>& bodies,
                      std::vector<EvaluatedBody>& referenceBodies) {
    const CylinderFeature cylinder(descriptor.id);
    evaluatePrimitiveTool(
        document.cylinderParameters(descriptor.id).resultMode,
        [&](const TopoDS_Shape& input, bool place) {
            return cylinder.evaluateToolWithHistory(document, input, place);
        },
        descriptor, outputs, bodies, referenceBodies);
}

// M27: sphere, cone, torus and wedge stand where a cylinder would.
void evaluatePrimitive(const document::DocumentSnapshot& document,
                       const document::FeatureDescriptor& descriptor,
                       std::vector<EvaluatedFeature>& outputs, std::vector<EvaluatedBody>& bodies,
                       std::vector<EvaluatedBody>& referenceBodies) {
    const auto& parameters = document.primitiveParameters(descriptor.id);
    evaluatePrimitiveTool(
        parameters.resultMode,
        [&](const TopoDS_Shape& input, bool place) {
            auto definition = parameters.definition();
            const double height = kernel::primitiveHeight(definition);
            const gp_Pnt origin = CylinderFeature::attachmentOrigin(input, height);
            definition.originX = origin.X();
            definition.originY = origin.Y();
            definition.originZ = origin.Z();
            auto shape = kernel::makeSolidPrimitiveWithHistory(definition);
            if (!place)
                return shape;
            return detail::placePrimitive(shape, origin.X(), origin.Y(),
                                          origin.Z() + height * 0.5, parameters.placement);
        },
        descriptor, outputs, bodies, referenceBodies);
}

// M19: a sketch on a construction plane lies where the plane is now.
void setConstructionPlane(document::SketchParameters& parameters,
                          const std::vector<EvaluatedFeature>& outputs) {
    if (!parameters.constructionPlane)
        return;
    const auto& frame = findFeature(outputs, *parameters.constructionPlane).datumFrame;
    if (!frame)
        throw GeometryEvaluationError("The construction plane of the sketch is missing");
    parameters.plane = *frame;
}

// A sketch without support: its construction plane and projected edges are brought up to
// date; true when the sketch then needs a solve (projections or bound parameters).
bool prepareStandaloneSketch(document::SketchParameters& parameters,
                             const document::DocumentSnapshot& document,
                             document::FeatureId sketch,
                             const std::vector<EvaluatedFeature>& outputs) {
    setConstructionPlane(parameters, outputs);
    detail::refreshSketchProjections(parameters, document, sketch, outputs);
    return !parameters.projections.empty() ||
           std::ranges::any_of(parameters.constraints.values,
                               [](const auto& constraint) { return constraint.parameter.has_value(); });
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
        detail::refreshSketchProjections(parameters, document, descriptor.id, outputs);
        if (solveSketchConstraints(parameters).status != SketchSolveStatus::success)
            throw GeometryEvaluationError("Sketch support references cannot be solved");
    } else if (prepareStandaloneSketch(parameters, document, descriptor.id, outputs)) {
        // PAR01: a table change mirrors values without an editor solve; an unsolvable
        // result stays visible as an error instead of stale geometry. S17: projected edges
        // move the elements bound to them.
        if (solveSketchConstraints(parameters).status != SketchSolveStatus::success)
            throw GeometryEvaluationError("Sketch parameters cannot be solved");
    } else {
        // S12: stored geometry unsolved; instances still follow their sources.
        document::regenerateSketchPatterns(parameters.elements, parameters.patterns);
        measureReferenceDimensions(parameters); // S18
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
        detail::evaluateExtrusion(document, descriptor, outputs, bodies, references);
        break;
    case document::FeatureKind::fillet:
        detail::evaluateFillet(document, descriptor, outputs, bodies, references);
        break;
    case document::FeatureKind::chamfer:
        detail::evaluateChamfer(document, descriptor, outputs, bodies, references);
        break;
    case document::FeatureKind::hole:
        detail::evaluateHole(document, descriptor, outputs, bodies, references);
        break;
    case document::FeatureKind::combine:
        detail::evaluateCombine(document, descriptor, outputs, bodies, references);
        break;
    case document::FeatureKind::datumPlane:
        detail::evaluateDatumPlane(document, descriptor, outputs);
        break;
    case document::FeatureKind::datumAxis:
        detail::evaluateDatumAxis(document, descriptor, outputs);
        break;
    case document::FeatureKind::shell:
        detail::evaluateShell(document, descriptor, outputs, bodies, references);
        break;
    case document::FeatureKind::primitive:
        evaluatePrimitive(document, descriptor, outputs, bodies, references);
        break;
    case document::FeatureKind::draft:
        detail::evaluateDraft(document, descriptor, outputs, bodies, references);
        break;
    case document::FeatureKind::mirror:
        detail::evaluateMirror(document, descriptor, outputs, bodies, references);
        break;
    case document::FeatureKind::pattern:
        detail::evaluatePattern(document, descriptor, outputs, bodies, references);
        break;
    case document::FeatureKind::patternCopy:
        detail::evaluatePatternCopy(document, descriptor, outputs, bodies, references);
        break;
    }
}
void restoreOutput(const EvaluatedFeature& output, std::vector<EvaluatedFeature>& outputs,
                   std::vector<EvaluatedBody>& bodies, std::vector<EvaluatedBody>& references) {
    outputs.push_back(output);
    for (const auto consumed : output.consumedBodies) {
        std::erase_if(bodies, [&](const auto& body) { return body.bodyId == consumed; });
        std::erase_if(references, [&](const auto& body) { return body.bodyId == consumed; });
    }
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
    const auto failure = [&](const std::string& message, const std::string& code = {}) {
        issue.status = EvaluationStatus::failed;
        issue.code = code.empty() ? "feature-evaluation-failed" : code;
        issue.message = message;
    };
    try {
        runFeature(document, descriptor, outputs, bodies, references);
    } catch (const GeometryEvaluationError& error) {
        failure(error.what(), error.code());
        issue.elements = error.elements();
    } catch (const kernel::KernelError& error) {
        failure(error.what(), kernel::booleanFailureCode(error.failure()));
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
