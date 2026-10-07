#include "FeatureEvaluationDetail.hpp"

#include <voelund/document/DocumentSnapshot.hpp>
#include <voelund/features/FeatureDependencies.hpp>
#include <voelund/features/GeometryEvaluationError.hpp>
#include <voelund/features/SketchSupportBinding.hpp>
#include <voelund/kernel/NumericPolicy.hpp>
#include <voelund/kernel/ShapeOperations.hpp>

#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <algorithm>
#include <cmath>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

// T04: push/pull. The profile of the extrusion is a planar face of a body as it was before
// this feature, named by its semantic origin, so the prism follows the face when the body
// changes. The prism joins or cuts that body; its end cap has its own stable origin and can
// be pushed or pulled again.
namespace voelund::features::detail {
namespace {

struct FacePrism final {
    TopoDS_Shape shape;
    std::vector<SupportProvenance> caps;
};

const SupportFaceFrame& profileFace(const std::vector<SupportFaceFrame>& faces,
                                    const document::SubshapeOrigin& origin) {
    const auto face = std::ranges::find(faces, origin, &SupportFaceFrame::origin);
    if (face == faces.end())
        throw GeometryEvaluationError("Profile face is no longer a unique planar face of its body");
    return *face;
}

// The outward normal of the face as it lies in the body: provenance keeps the face, not the
// orientation of its occurrence in the solid. Into the body when reversed.
gp_Vec sweep(const TopoDS_Shape& body, const SupportFaceFrame& face,
             const document::ExtrusionParameters& parameters) {
    for (TopExp_Explorer explorer(body, TopAbs_FACE); explorer.More(); explorer.Next()) {
        if (!explorer.Current().IsSame(face.face))
            continue;
        const BRepAdaptor_Surface surface(TopoDS::Face(explorer.Current()));
        gp_Vec normal(surface.Plane().Axis().Direction());
        if (explorer.Current().Orientation() == TopAbs_REVERSED)
            normal.Reverse();
        return parameters.reversed ? -normal : normal;
    }
    throw GeometryEvaluationError("Profile face is not a face of its body");
}

// T03 semantics: the length from the (offset) start plane to a parallel end face ahead.
double lengthToFace(const gp_Pnt& start, const gp_Vec& along, const SupportFaceFrame& end) {
    const gp_Vec normal(end.plane.normalX, end.plane.normalY, end.plane.normalZ);
    if (along.CrossMagnitude(normal) > std::sin(kernel::angularTolerance.radians))
        throw GeometryEvaluationError("End face must be parallel to the profile face");
    const double length =
        gp_Vec(start, gp_Pnt(end.plane.originX, end.plane.originY, end.plane.originZ)).Dot(along);
    if (length <= 1e-6)
        throw GeometryEvaluationError("End face does not lie ahead in the extrusion direction");
    return length;
}

FacePrism makePrism(document::FeatureId extrusion, const TopoDS_Shape& body,
                    const SupportFaceFrame& face, const document::ExtrusionParameters& parameters,
                    const SupportFaceFrame* endFace) {
    const gp_Vec along = sweep(body, face, parameters);
    TopoDS_Shape start = face.face;
    gp_Pnt origin(face.plane.originX, face.plane.originY, face.plane.originZ);
    if (parameters.offset != 0.0) {
        gp_Trsf shift;
        shift.SetTranslation(along * parameters.offset);
        start = BRepBuilderAPI_Transform(face.face, shift, true).Shape();
        origin.Translate(along * parameters.offset);
    }
    const double length = endFace ? lengthToFace(origin, along, *endFace) : parameters.distance;
    BRepPrimAPI_MakePrism prism(start, along * length);
    const auto cap = prism.LastShape(start);
    return {prism.Shape(),
            {{{extrusion, std::nullopt, {}, true}, {cap}, kernel::HistoryCoverage::complete, cap}}};
}

// The reference evaluation of an output, as its own evaluated feature.
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

} // namespace

void evaluateFaceExtrusion(const document::DocumentSnapshot& document,
                           const document::FeatureDescriptor& descriptor,
                           const document::ExtrusionParameters& parameters,
                           std::vector<EvaluatedFeature>& outputs,
                           std::vector<EvaluatedBody>& bodies,
                           std::vector<EvaluatedBody>& referenceBodies) {
    const auto& profile = *parameters.faceProfile;
    const auto producer = bodyProducerBefore(document, descriptor.id, profile.body);
    if (!producer)
        throw GeometryEvaluationError("Profile face body has no solid before this extrusion");
    std::optional<document::FeatureId> endOwner;
    if (parameters.upToFace) {
        endOwner = bodyProducerBefore(document, descriptor.id, parameters.upToFace->body);
        if (!endOwner)
            throw GeometryEvaluationError("End face body has no solid before this extrusion");
    }
    const auto& owner = findFeature(outputs, *producer);
    const auto faces = supportFaceFrames(owner);
    const auto& face = profileFace(faces, profile.origin);
    std::optional<SupportFaceFrame> end;
    if (endOwner)
        end = profileFace(supportFaceFrames(findFeature(outputs, *endOwner)),
                          parameters.upToFace->origin);
    const auto prism =
        makePrism(descriptor.id, owner.shape, face, parameters, end ? &*end : nullptr);

    // The reference prism sweeps the reference face by the same length.
    const auto referenceOwner = referenceOf(owner);
    const auto referenceFaces = supportFaceFrames(referenceOwner);
    auto referenceParameters = parameters;
    if (end) {
        const gp_Vec along = sweep(owner.shape, face, parameters);
        gp_Pnt start(face.plane.originX, face.plane.originY, face.plane.originZ);
        start.Translate(along * parameters.offset);
        referenceParameters.distance = lengthToFace(start, along, *end);
    }
    const auto referencePrism =
        makePrism(descriptor.id, owner.referenceShape, profileFace(referenceFaces, profile.origin),
                  referenceParameters, nullptr);

    const auto operation = document::effectiveOperation(parameters, false);
    EvaluatedBody& body = findBody(bodies, outputs, *producer);
    EvaluatedBody& referenceBody = findBody(referenceBodies, outputs, *producer);
    const auto combine = [operation](const TopoDS_Shape& target, const TopoDS_Shape& tool) {
        return operation == document::ExtrusionOperation::cut
                   ? kernel::ShapeOperations::cutWithHistory(target, tool)
                   : kernel::ShapeOperations::fuseWithHistory(target, tool);
    };
    const auto evaluated = combine(body.shape, prism.shape);
    const auto reference = combine(referenceBody.shape, referencePrism.shape);
    const auto joined = [](const auto& previous, const auto& tool, const auto& result) {
        auto origins = continueSupportOrigins(previous, result, kernel::HistoryOperand::first);
        auto added = continueSupportOrigins(tool, result, kernel::HistoryOperand::second);
        origins.insert(origins.end(), added.begin(), added.end());
        return origins;
    };
    outputs.push_back(EvaluatedFeature{
        descriptor.id, evaluated.shape, reference.shape, body.bodyId,
        joined(owner.supportOrigins, prism.caps, evaluated), {},
        joined(owner.referenceSupportOrigins, referencePrism.caps, reference),
        continueEdgeOrigins(owner.edgeOrigins, evaluated, kernel::HistoryOperand::first),
        continueEdgeOrigins(owner.referenceEdgeOrigins, reference, kernel::HistoryOperand::first)});
    body.sourceFeature = descriptor.id;
    body.shape = evaluated.shape;
    referenceBody.sourceFeature = descriptor.id;
    referenceBody.shape = reference.shape;
}

} // namespace voelund::features::detail
