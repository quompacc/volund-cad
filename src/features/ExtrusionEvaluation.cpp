// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "FeatureEvaluationDetail.hpp"

#include <voelund/document/DocumentSnapshot.hpp>
#include <voelund/features/ContourProvenance.hpp>
#include <voelund/features/FeatureDependencies.hpp>
#include <voelund/features/GeometryEvaluationError.hpp>
#include <voelund/features/SketchConstraintSolver.hpp>
#include <voelund/features/SketchEvaluation.hpp>
#include <voelund/features/SketchSupportBinding.hpp>
#include <voelund/kernel/NumericPolicy.hpp>
#include <voelund/kernel/ShapeOperations.hpp>

#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <algorithm>
#include <cmath>
#include <gp_Ax1.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <utility>
#include <vector>
#include <voelund/kernel/Helix.hpp>
#include <voelund/kernel/Loft.hpp>
#include <voelund/kernel/Prism.hpp>
#include <voelund/kernel/Revolution.hpp>
#include <voelund/kernel/Sweep.hpp>

namespace voelund::features::detail {
namespace {

// U15: a coded evaluation failure in one line.
[[noreturn]] void fail(const char* message, const char* code) {
    throw GeometryEvaluationError(message, code);
}
struct Prism final {
    TopoDS_Shape shape;
    std::vector<SupportProvenance> caps;
    std::vector<EdgeProvenance> edges;
};
// The sweep direction: the sketch normal, reversed on request.
gp_Vec direction(const document::SketchParameters& sketch,
                 const document::ExtrusionParameters& parameters) {
    const gp_Vec normal(sketch.plane.normalX, sketch.plane.normalY, sketch.plane.normalZ);
    return parameters.reversed ? -normal : normal;
}
// T03: the sketch moved by the start offset along the sweep direction. M02: a symmetric
// prism starts half its total length behind the sketch plane.
document::SketchParameters startSketch(document::SketchParameters sketch,
                                       const document::ExtrusionParameters& parameters) {
    const double start = parameters.symmetric ? -0.5 * parameters.distance : parameters.offset;
    const auto shift = direction(sketch, parameters) * start;
    sketch.plane.originX += shift.X();
    sketch.plane.originY += shift.Y();
    sketch.plane.originZ += shift.Z();
    return sketch;
}
// T03: the length from the start plane to a parallel end face ahead of it.
double lengthToFace(const document::SketchParameters& start,
                    const document::ExtrusionParameters& parameters,
                    const EvaluatedFeature& faceOwner, const document::SubshapeOrigin& origin) {
    const auto faces = supportFaceFrames(faceOwner);
    const auto face = std::ranges::find(faces, origin, &SupportFaceFrame::origin);
    if (face == faces.end())
        throw GeometryEvaluationError("End face is no longer a unique planar face of its body",
                                      "end-face-missing");
    const auto along = direction(start, parameters);
    const gp_Vec normal(face->plane.normalX, face->plane.normalY, face->plane.normalZ);
    if (along.CrossMagnitude(normal) > std::sin(kernel::angularTolerance.radians))
        throw GeometryEvaluationError("End face must be parallel to the sketch",
                                      "end-face-not-parallel");
    const gp_Vec offset(gp_Pnt(start.plane.originX, start.plane.originY, start.plane.originZ),
                        gp_Pnt(face->plane.originX, face->plane.originY, face->plane.originZ));
    const double length = offset.Dot(along);
    if (length <= 1e-6)
        throw GeometryEvaluationError("End face does not lie ahead in the extrusion direction",
                                      "end-face-behind");
    return length;
}
// The selected regions swept along the sketch normal, against it when reversed.
Prism makePrism(document::FeatureId extrusion, document::FeatureId sketchId,
                const document::SketchParameters& sketch,
                const document::ExtrusionParameters& parameters, double length) {
    const auto profiles = evaluateExtrusionFaces(sketch, parameters);
    TopoDS_Compound faces;
    BRep_Builder builder;
    builder.MakeCompound(faces);
    for (const auto& profile : profiles)
        builder.Add(faces, profile.face);
    // Exact support-plane contact: preserve both the start plane and the requested
    // end plane. OCCT's Boolean classifies the coincident faces; no length padding.
    // M02: the kernel's prism checks the sweep and its solid before anything uses it.
    const gp_Vec along = direction(sketch, parameters);
    const kernel::Prism prism(faces, along.X(), along.Y(), along.Z(), length);
    Prism result{prism.shape(), {}, extrusionEdgeOrigins(extrusion, sketchId, profiles, prism)};
    for (const auto& profile : profiles) {
        const auto cap = prism.endOf(profile.face);
        result.caps.push_back({{extrusion, std::nullopt, profile.boundary},
                               {cap},
                               kernel::HistoryCoverage::complete,
                               cap});
        // T04b: the start cap, and each side face named by the contour edge it was swept
        // from (ExtrusionCapKey / ExtrusionSideKey).
        document::SubshapeOrigin start{extrusion, std::nullopt, profile.boundary};
        start.extrusionFace = document::ExtrusionFace::start;
        addSupportFace(result.caps, start, prism.startOf(profile.face));
        for (const auto& edge : profile.edges) {
            document::SubshapeOrigin side{extrusion, std::nullopt, profile.boundary};
            side.extrusionFace = document::ExtrusionFace::side;
            side.sweptEdge = document::ContourEdgeOrigin{sketchId, edge.element, edge.role};
            for (const auto& face : prism.generated(edge.edge))
                addSupportFace(result.caps, side, face);
        }
    }
    return result;
}

// M20: the revolve axis in model coordinates: a line of the sketch, a base axis or the frame
// of a construction axis.
gp_Ax1 revolveAxis(const document::SketchParameters& sketch, const document::RevolveAxis& axis,
                   const std::vector<EvaluatedFeature>& outputs) {
    if (axis.baseAxis)
        return gp_Ax1(gp_Pnt(0, 0, 0), *axis.baseAxis == document::BaseAxis::x   ? gp_Dir(1, 0, 0)
                                       : *axis.baseAxis == document::BaseAxis::y ? gp_Dir(0, 1, 0)
                                                                                 : gp_Dir(0, 0, 1));
    if (axis.datumAxis) {
        const auto& frame = findFeature(outputs, *axis.datumAxis).datumFrame;
        if (!frame)
            throw GeometryEvaluationError("The revolve axis is no construction axis",
                                          "revolve-axis-missing");
        return gp_Ax1(gp_Pnt(frame->originX, frame->originY, frame->originZ),
                      gp_Dir(frame->normalX, frame->normalY, frame->normalZ));
    }
    const auto element =
        std::ranges::find(sketch.elements.values, *axis.sketchLine, &document::SketchElement::id);
    const auto* line = element == sketch.elements.values.end()
                           ? nullptr
                           : std::get_if<document::SketchLine>(&element->geometry);
    if (line == nullptr)
        throw GeometryEvaluationError("The revolve axis is no line of its sketch",
                                      "revolve-axis-missing");
    const auto& plane = sketch.plane;
    const gp_Vec x(plane.xDirectionX, plane.xDirectionY, plane.xDirectionZ);
    const gp_Vec y = gp_Vec(plane.normalX, plane.normalY, plane.normalZ).Crossed(x);
    const gp_Pnt origin(plane.originX, plane.originY, plane.originZ);
    const gp_Pnt start = origin.Translated(x * line->start.x + y * line->start.y);
    const gp_Pnt end = origin.Translated(x * line->end.x + y * line->end.y);
    if (start.Distance(end) < 1e-9)
        throw GeometryEvaluationError("The revolve axis line has no length",
                                      "revolve-axis-missing");
    return gp_Ax1(start, gp_Dir(gp_Vec(start, end)));
}
// M26: the one selected region swept along a helix about the revolve axis; reversed is
// left-handed. Faces: start, end and the sides each contour edge sweeps.
Prism makeHelix(document::FeatureId extrusion, document::FeatureId sketchId,
                const document::SketchParameters& sketch,
                const document::ExtrusionParameters& parameters,
                const std::vector<EvaluatedFeature>& outputs) {
    const gp_Ax1 axis = revolveAxis(sketch, parameters.revolve->axis, outputs);
    const auto profiles = evaluateExtrusionFaces(sketch, parameters);
    if (profiles.size() != 1)
        throw GeometryEvaluationError("A helix sweeps exactly one region", "helix-one-region");
    const auto& location = axis.Location();
    const auto& direction = axis.Direction();
    const kernel::Helix helix(profiles.front().face, {location.X(), location.Y(), location.Z()},
                              {direction.X(), direction.Y(), direction.Z()},
                              *parameters.revolve->pitch, parameters.revolve->angle.value(),
                              parameters.reversed);
    Prism result{helix.shape(), {}, {}};
    const auto& profile = profiles.front();
    if (const auto cap = helix.endFace(); !cap.IsNull())
        result.caps.push_back({{extrusion, std::nullopt, profile.boundary},
                               {cap},
                               kernel::HistoryCoverage::complete,
                               cap});
    document::SubshapeOrigin start{extrusion, std::nullopt, profile.boundary};
    start.extrusionFace = document::ExtrusionFace::start;
    addSupportFace(result.caps, start, helix.startFace());
    for (const auto& edge : profile.edges) {
        document::SubshapeOrigin side{extrusion, std::nullopt, profile.boundary};
        side.extrusionFace = document::ExtrusionFace::side;
        side.sweptEdge = document::ContourEdgeOrigin{sketchId, edge.element, edge.role};
        for (const auto& face : helix.generated(edge.edge))
            addSupportFace(result.caps, side, face);
    }
    return result;
}
// M20: the selected regions turned about the axis; reversed turns the other way, symmetric
// starts half the angle behind the sketch plane. Faces are named as for a prism: start, end
// (none for a full turn) and the side each contour edge turns into.
Prism makeRevolution(document::FeatureId extrusion, document::FeatureId sketchId,
                     document::SketchParameters sketch,
                     const document::ExtrusionParameters& parameters,
                     const std::vector<EvaluatedFeature>& outputs) {
    if (parameters.revolve->pitch) // M26
        return makeHelix(extrusion, sketchId, sketch, parameters, outputs);
    gp_Ax1 axis = revolveAxis(sketch, parameters.revolve->axis, outputs);
    if (parameters.reversed)
        axis.Reverse();
    const double angle = parameters.revolve->angle.value();
    if (parameters.symmetric) {
        auto& plane = sketch.plane;
        gp_Pnt origin(plane.originX, plane.originY, plane.originZ);
        gp_Dir normal(plane.normalX, plane.normalY, plane.normalZ);
        gp_Dir x(plane.xDirectionX, plane.xDirectionY, plane.xDirectionZ);
        origin.Rotate(axis, -angle / 2);
        normal.Rotate(axis, -angle / 2);
        x.Rotate(axis, -angle / 2);
        plane = {origin.X(), origin.Y(), origin.Z(), normal.X(), normal.Y(),
                 normal.Z(), x.X(),      x.Y(),      x.Z()};
    }
    const auto profiles = evaluateExtrusionFaces(sketch, parameters);
    TopoDS_Compound faces;
    BRep_Builder builder;
    builder.MakeCompound(faces);
    for (const auto& profile : profiles)
        builder.Add(faces, profile.face);
    const auto& location = axis.Location();
    const auto& direction = axis.Direction();
    const kernel::Revolution revolution(faces, {location.X(), location.Y(), location.Z()},
                                        {direction.X(), direction.Y(), direction.Z()}, angle);
    Prism result{
        revolution.shape(), {}, extrusionEdgeOrigins(extrusion, sketchId, profiles, revolution)};
    for (const auto& profile : profiles) {
        if (!revolution.full()) {
            const auto cap = revolution.endOf(profile.face);
            result.caps.push_back({{extrusion, std::nullopt, profile.boundary},
                                   {cap},
                                   kernel::HistoryCoverage::complete,
                                   cap});
            document::SubshapeOrigin start{extrusion, std::nullopt, profile.boundary};
            start.extrusionFace = document::ExtrusionFace::start;
            addSupportFace(result.caps, start, revolution.startOf(profile.face));
        }
        for (const auto& edge : profile.edges) {
            document::SubshapeOrigin side{extrusion, std::nullopt, profile.boundary};
            side.extrusionFace = document::ExtrusionFace::side;
            side.sweptEdge = document::ContourEdgeOrigin{sketchId, edge.element, edge.role};
            for (const auto& face : revolution.generated(edge.edge))
                addSupportFace(result.caps, side, face);
        }
    }
    return result;
}

// M23: the selected regions swept along the path of an earlier sketch. Faces are named as for
// a prism: start, end (none on a closed path) and the sides each contour edge sweeps; the
// path may bend, so its contour edges are not named.
Prism makeSweep(document::FeatureId extrusion, const document::SketchParameters& sketch,
                const document::ExtrusionParameters& parameters,
                const std::vector<EvaluatedFeature>& outputs, document::FeatureId sketchId) {
    const auto& path = *parameters.sweep;
    const auto& pathOutput = findFeature(outputs, path.sketch);
    if (!pathOutput.resolvedSketch)
        throw GeometryEvaluationError("The sweep path sketch has no geometry");
    const auto elements = evaluateSketchElements(*pathOutput.resolvedSketch);
    std::vector<TopoDS_Edge> edges;
    for (const auto id : path.elements) {
        const auto element = std::ranges::find(elements, id, &EvaluatedSketchElement::id);
        if (element == elements.end())
            throw GeometryEvaluationError("A sweep path element is missing", "sweep-path-missing",
                                          {id.value()});
        for (TopExp_Explorer it(element->shape, TopAbs_EDGE); it.More(); it.Next())
            edges.push_back(TopoDS::Edge(it.Current()));
    }
    // The chain in any order: each pass adds the edges that touch the wire so far.
    BRepBuilderAPI_MakeWire wire;
    std::vector<bool> used(edges.size(), false);
    for (bool added = true; added;) {
        added = false;
        for (std::size_t index = 0; index < edges.size(); ++index) {
            if (used[index])
                continue;
            wire.Add(edges[index]);
            if (wire.Error() == BRepBuilderAPI_WireDone) {
                used[index] = true;
                added = true;
            }
        }
    }
    if (std::ranges::find(used, false) != used.end() || !wire.IsDone())
        throw GeometryEvaluationError("The sweep path is not one connected chain",
                                      "sweep-path-disconnected");
    const auto profiles = evaluateExtrusionFaces(sketch, parameters);
    TopoDS_Compound faces;
    BRep_Builder builder;
    builder.MakeCompound(faces);
    for (const auto& profile : profiles)
        builder.Add(faces, profile.face);
    const kernel::Sweep swept(faces, wire.Wire());
    Prism result{swept.shape(), {}, {}};
    for (const auto& profile : profiles) {
        if (const auto cap = swept.endOf(profile.face); !cap.IsNull()) {
            result.caps.push_back({{extrusion, std::nullopt, profile.boundary},
                                   {cap},
                                   kernel::HistoryCoverage::complete,
                                   cap});
            document::SubshapeOrigin start{extrusion, std::nullopt, profile.boundary};
            start.extrusionFace = document::ExtrusionFace::start;
            addSupportFace(result.caps, start, swept.startOf(profile.face));
        }
        for (const auto& edge : profile.edges) {
            document::SubshapeOrigin side{extrusion, std::nullopt, profile.boundary};
            side.extrusionFace = document::ExtrusionFace::side;
            side.sweptEdge = document::ContourEdgeOrigin{sketchId, edge.element, edge.role};
            for (const auto& face : swept.generated(edge.edge))
                addSupportFace(result.caps, side, face);
        }
    }
    return result;
}

// M24: the one selected region lofted through one region of each further sketch, in order.
// Faces: start on the first section, end on the last, the sides by the first section's edges.
Prism makeLoft(document::FeatureId extrusion, const document::SketchParameters& sketch,
               const document::ExtrusionParameters& parameters,
               const std::vector<EvaluatedFeature>& outputs, document::FeatureId sketchId) {
    const auto profiles = evaluateExtrusionFaces(sketch, parameters);
    if (profiles.size() != 1)
        throw GeometryEvaluationError("A loft starts from exactly one region");
    const auto outer = [](const TopoDS_Shape& shape) {
        const TopoDS_Face face = TopoDS::Face(shape);
        int wires = 0;
        for (TopExp_Explorer it(face, TopAbs_WIRE); it.More(); it.Next())
            ++wires;
        if (wires != 1)
            throw GeometryEvaluationError("A loft section has no holes", "loft-section-hole");
        return TopoDS_Shape(BRepTools::OuterWire(face));
    };
    const auto inOnePlane = [](const document::SketchPlane& a, const document::SketchPlane& b) {
        const gp_Vec normal(a.normalX, a.normalY, a.normalZ);
        const gp_Vec offset(gp_Pnt(a.originX, a.originY, a.originZ),
                            gp_Pnt(b.originX, b.originY, b.originZ));
        return normal.CrossMagnitude(gp_Vec(b.normalX, b.normalY, b.normalZ)) < 1e-9 &&
               std::abs(offset.Dot(normal)) < 1e-6;
    };
    std::vector<TopoDS_Shape> wires{outer(profiles.front().face)};
    document::SketchPlane previous = sketch.plane;
    for (const auto& section : parameters.loft->sections) {
        const auto& output = findFeature(outputs, section.sketch);
        if (!output.resolvedSketch)
            throw GeometryEvaluationError("A loft section sketch has no geometry");
        document::ExtrusionParameters one{0.0, {}, {section.region}};
        std::vector<EvaluatedProfileFace> faces;
        try {
            faces = evaluateExtrusionFaces(*output.resolvedSketch, one);
        } catch (const GeometryEvaluationError&) {
            throw GeometryEvaluationError("A loft section region is missing",
                                          "loft-section-missing");
        }
        if (faces.size() != 1)
            throw GeometryEvaluationError("A loft section region is missing",
                                          "loft-section-missing");
        if (inOnePlane(previous, output.resolvedSketch->plane))
            throw GeometryEvaluationError("Loft sections lie in one plane", "loft-coplanar");
        previous = output.resolvedSketch->plane;
        wires.push_back(outer(faces.front().face));
    }
    const kernel::Loft loft(wires, parameters.loft->ruled);
    Prism result{loft.shape(), {}, {}};
    const auto& profile = profiles.front();
    if (const auto cap = loft.endFace(); !cap.IsNull())
        result.caps.push_back({{extrusion, std::nullopt, profile.boundary},
                               {cap},
                               kernel::HistoryCoverage::complete,
                               cap});
    document::SubshapeOrigin start{extrusion, std::nullopt, profile.boundary};
    start.extrusionFace = document::ExtrusionFace::start;
    addSupportFace(result.caps, start, loft.startFace());
    for (const auto& edge : profile.edges) {
        document::SubshapeOrigin side{extrusion, std::nullopt, profile.boundary};
        side.extrusionFace = document::ExtrusionFace::side;
        side.sweptEdge = document::ContourEdgeOrigin{sketchId, edge.element, edge.role};
        if (const auto face = loft.generated(edge.edge); !face.IsNull())
            addSupportFace(result.caps, side, face);
    }
    return result;
}

kernel::ShapeOperationResult combine(document::ExtrusionOperation operation,
                                     const TopoDS_Shape& body, const TopoDS_Shape& tool) {
    switch (operation) {
    case document::ExtrusionOperation::cut:
        return kernel::ShapeOperations::cutWithHistory(body, tool);
    case document::ExtrusionOperation::intersect:
        return kernel::ShapeOperations::intersectWithHistory(body, tool);
    default:
        return kernel::ShapeOperations::fuseWithHistory(body, tool);
    }
}

template <class Origins>
Origins continued(const Origins& input, const Origins& tool,
                  const kernel::ShapeOperationResult& result, auto continueOrigins) {
    auto origins = continueOrigins(input, result, kernel::HistoryOperand::first);
    auto added = continueOrigins(tool, result, kernel::HistoryOperand::second);
    origins.insert(origins.end(), added.begin(), added.end());
    return origins;
}
} // namespace

void evaluateExtrusion(const document::DocumentSnapshot& document,
                       const document::FeatureDescriptor& descriptor,
                       std::vector<EvaluatedFeature>& outputs, std::vector<EvaluatedBody>& bodies,
                       std::vector<EvaluatedBody>& referenceBodies) {
    if (const auto& own = document.extrusionParameters(descriptor.id); own.faceProfile) {
        evaluateFaceExtrusion(document, descriptor, own, outputs, bodies, referenceBodies);
        return;
    }
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
    if (sketchDescriptor == descriptors.end())
        throw GeometryEvaluationError("Extrusion sketch is missing");
    const auto support = effectiveInput(document, sketchId); // M14
    const auto operation = document::effectiveOperation(parameters, !support);
    // T03: the end face's body as it was before this extrusion, current and reference.
    std::optional<document::FeatureId> faceOwner;
    if (parameters.upToFace) {
        faceOwner = bodyProducerBefore(document, descriptor.id, parameters.upToFace->body);
        if (!faceOwner)
            fail("End face body has no solid before this extrusion", "body-missing");
    }
    // M20: a revolve turns the sketch itself; start offsets belong to prisms.
    const auto start = parameters.revolve || parameters.sweep || parameters.loft
                           ? sketch
                           : startSketch(sketch, parameters);
    // The reference prism keeps this length: reference results are unplaced primitives, so
    // the end face's reference position says nothing about the current extent.
    const double length = faceOwner
                              ? lengthToFace(start, parameters, findFeature(outputs, *faceOwner),
                                             parameters.upToFace->origin)
                              : parameters.distance;
    const auto sweep = [&](const document::SketchParameters& at) {
        return parameters.revolve ? makeRevolution(descriptor.id, sketchId, at, parameters, outputs)
               : parameters.sweep ? makeSweep(descriptor.id, at, parameters, outputs, sketchId)
               : parameters.loft  ? makeLoft(descriptor.id, at, parameters, outputs, sketchId)
                                  : makePrism(descriptor.id, sketchId, at, parameters, length);
    };
    const auto prism = sweep(start);

    // The reference evaluation resolves a bound face sketch on the support's reference
    // result; a base-plane sketch is its own reference.
    auto referenceSketch = sketch;
    if (support && sketch.supportBinding) {
        const auto& supportOutput = findFeature(outputs, *support);
        const EvaluatedFeature referenceSupport{supportOutput.id,
                                                supportOutput.referenceShape,
                                                supportOutput.referenceShape,
                                                supportOutput.bodyId,
                                                supportOutput.referenceSupportOrigins,
                                                {},
                                                {},
                                                supportOutput.referenceEdgeOrigins};
        resolveBoundSketchSupport(referenceSketch, referenceSupport);
        if (solveSketchConstraints(referenceSketch).status != SketchSolveStatus::success)
            fail("Reference sketch projected edges cannot be solved", "sketch-unsolved");
    }
    const auto referenceStart = parameters.revolve || parameters.sweep || parameters.loft
                                    ? referenceSketch
                                    : startSketch(referenceSketch, parameters);
    // M20: a base or construction axis lies in model coordinates, which the unplaced
    // reference of a face sketch need not share; the reference then keeps the current turn.
    const auto referencePrism = [&] {
        // M23: a sweep path, too, lies in model coordinates.
        if ((!parameters.revolve && !parameters.sweep && !parameters.loft) ||
            (parameters.revolve && parameters.revolve->axis.sketchLine))
            return sweep(referenceStart);
        try {
            return sweep(referenceStart);
        } catch (const kernel::KernelError&) {
            return prism;
        }
    }();

    if (operation == document::ExtrusionOperation::newBody) {
        // V01: a base-plane sketch opened its own body (BodyId = sketch FeatureId); T02: a
        // new body on a face is the extrusion's own. Nothing is fused and nothing is guessed.
        const document::BodyId bodyId{support ? descriptor.id.value() : sketchId.value()};
        if (std::ranges::find(bodies, bodyId, &EvaluatedBody::bodyId) != bodies.end())
            fail("Base-plane body already has a solid", "base-body-exists");
        outputs.push_back(EvaluatedFeature{descriptor.id,
                                           prism.shape,
                                           referencePrism.shape,
                                           bodyId,
                                           prism.caps,
                                           {},
                                           referencePrism.caps,
                                           prism.edges,
                                           referencePrism.edges});
        bodies.push_back({bodyId, descriptor.id, prism.shape});
        referenceBodies.push_back({bodyId, descriptor.id, referencePrism.shape});
        return;
    }

    // Join or cut works on the support's body, or on the body a base-plane sketch names.
    std::optional<document::FeatureId> input = support;
    if (!input) {
        if (!parameters.target)
            fail("Join or cut of a base-plane sketch needs a target body", "needs-target");
        input = targetProducer(document, descriptor.id);
        if (!input)
            fail("Target body has no solid before this extrusion", "body-missing");
    }
    EvaluatedBody& body = findBody(bodies, outputs, *input);
    EvaluatedBody& referenceBody = findBody(referenceBodies, outputs, *input);
    // M03: a refused Boolean names its target body and regions; the body stays as it was.
    const auto named = [&](auto&& run) {
        try {
            return run();
        } catch (const kernel::KernelError& error) {
            if (error.failure() == kernel::BooleanFailure::none)
                throw;
            std::string regions;
            for (const auto& region : parameters.regions)
                regions += (regions.empty() ? "" : ", ") + region.text();
            throw GeometryEvaluationError(
                std::string(error.what()) + " [body " + std::to_string(body.bodyId.value()) +
                    (regions.empty() ? std::string{} : ", regions " + regions) + "]",
                kernel::booleanFailureCode(error.failure()));
        }
    };
    const auto evaluated = named([&] { return combine(operation, body.shape, prism.shape); });
    const auto reference =
        named([&] { return combine(operation, referenceBody.shape, referencePrism.shape); });
    const auto& inputOutput = findFeature(outputs, *input);
    const auto supportOrigins = [](const auto& previous, const auto& result, auto operand) {
        return continueSupportOrigins(previous, result, operand);
    };
    const auto edgeOrigins = [](const auto& previous, const auto& result, auto operand) {
        return continueEdgeOrigins(previous, result, operand);
    };
    auto origins = continued(inputOutput.supportOrigins, prism.caps, evaluated, supportOrigins);
    auto referenceOrigins = continued(inputOutput.referenceSupportOrigins, referencePrism.caps,
                                      reference, supportOrigins);
    auto edges = continued(inputOutput.edgeOrigins, prism.edges, evaluated, edgeOrigins);
    auto referenceEdges =
        continued(inputOutput.referenceEdgeOrigins, referencePrism.edges, reference, edgeOrigins);
    outputs.push_back(EvaluatedFeature{descriptor.id,
                                       evaluated.shape,
                                       reference.shape,
                                       body.bodyId,
                                       std::move(origins),
                                       {},
                                       std::move(referenceOrigins),
                                       std::move(edges),
                                       std::move(referenceEdges)});
    body.sourceFeature = descriptor.id;
    body.shape = evaluated.shape;
    referenceBody.sourceFeature = descriptor.id;
    referenceBody.shape = reference.shape;
}

} // namespace voelund::features::detail
