#include <voelund/kernel/PlaneNormal.hpp>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <algorithm>
#include <cmath>
#include <gp_Ax3.hxx>
#include <gp_Pln.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>
#include <voelund/document/SketchPlaneValidation.hpp>
#include <voelund/features/GeometryEvaluationError.hpp>
#include <voelund/features/SketchSupportBinding.hpp>
#include <voelund/kernel/NumericPolicy.hpp>

namespace voelund::features {
namespace {
struct Frame {
    gp_Pnt origin;
    gp_Vec x, y, z;
};
Frame frame(const TopoDS_Shape& face) {
    if (face.IsNull() || face.ShapeType() != TopAbs_FACE)
        throw GeometryEvaluationError("Support frame is missing");
    const BRepAdaptor_Surface surface(TopoDS::Face(face));
    if (surface.GetType() != GeomAbs_Plane)
        throw GeometryEvaluationError("Sketch support must be planar");
    const auto plane = surface.Plane();
    const gp_Vec z(kernel::planeNormal(plane, face.Orientation()));
    const gp_Vec x(plane.XAxis().Direction());
    return {plane.Location(), x, z.Crossed(x), z};
}
document::SketchPlane convert(const document::SketchPlane& p, const Frame& f, bool toLocal) {
    const auto vector = [&](const gp_Vec& v) {
        return toLocal ? gp_Vec(v.Dot(f.x), v.Dot(f.y), v.Dot(f.z))
                       : f.x * v.X() + f.y * v.Y() + f.z * v.Z();
    };
    const gp_Vec position =
        toLocal
            ? vector(gp_Vec(f.origin, gp_Pnt(p.originX, p.originY, p.originZ)))
            : gp_Vec(gp_Pnt(0, 0, 0), f.origin) + vector(gp_Vec(p.originX, p.originY, p.originZ));
    const auto n = vector(gp_Vec(p.normalX, p.normalY, p.normalZ));
    const auto x = vector(gp_Vec(p.xDirectionX, p.xDirectionY, p.xDirectionZ));
    document::SketchPlane result{position.X(), position.Y(), position.Z(), n.X(), n.Y(),
                                 n.Z(),        x.X(),        x.Y(),        x.Z()};
    document::validateCanonicalSketchPlane(result);
    return result;
}
const SupportProvenance& requireOrigin(const EvaluatedFeature& support,
                                       const SupportOrigin& origin) {
    const auto found =
        std::ranges::find(support.supportOrigins, origin, &SupportProvenance::origin);
    if (found == support.supportOrigins.end())
        throw GeometryEvaluationError("Sketch support origin is missing");
    if (found->coverage != kernel::HistoryCoverage::complete)
        throw GeometryEvaluationError("Sketch support history is unavailable");
    if (found->faces.empty())
        throw GeometryEvaluationError("Sketch support face was deleted");
    if (found->faces.size() != 1)
        throw GeometryEvaluationError("Sketch support face is ambiguous");
    return *found;
}
// History can name a face in its construction orientation (an extrusion's end cap), while the
// solid uses it reversed. The outward side is that of the body's own face.
TopoDS_Shape inBody(const TopoDS_Shape& face, const EvaluatedFeature& support) {
    for (const TopoDS_Shape* body : {&support.shape, &support.referenceShape}) {
        if (body->IsNull())
            continue;
        for (TopExp_Explorer faces(*body, TopAbs_FACE); faces.More(); faces.Next())
            if (faces.Current().IsSame(face))
                return faces.Current();
    }
    return face;
}
void requirePlane(const document::SketchPlane& p, const TopoDS_Shape& face) {
    const auto f = frame(face);
    const auto scale = std::max({std::abs(p.originX), std::abs(p.originY), std::abs(p.originZ)});
    if (std::abs(gp_Vec(f.origin, gp_Pnt(p.originX, p.originY, p.originZ)).Dot(f.z)) >
            kernel::connectionTolerance(scale).millimetres ||
        gp_Vec(p.normalX, p.normalY, p.normalZ).Angle(f.z) > kernel::angularTolerance.radians)
        throw GeometryEvaluationError("Sketch frame no longer lies on its named support face");
}
} // namespace
std::vector<SupportFaceFrame> supportFaceFrames(const EvaluatedFeature& support) {
    std::vector<SupportFaceFrame> result;
    for (const auto& item : support.supportOrigins) {
        if (item.coverage != kernel::HistoryCoverage::complete || item.faces.size() != 1 ||
            BRepAdaptor_Surface(TopoDS::Face(item.faces.front())).GetType() != GeomAbs_Plane)
            continue;
        const auto& face = item.faces.front();
        if (std::ranges::count_if(support.supportOrigins, [&](const SupportProvenance& other) {
                return std::ranges::any_of(other.faces,
                                           [&](const auto& shape) { return shape.IsSame(face); });
            }) != 1)
            continue;
        // The reported frame faces out of the body: provenance keeps the face without the
        // orientation of its occurrence in the solid (T04). Stored bindings use frame(face)
        // and are unaffected.
        auto f = frame(face);
        for (TopExp_Explorer explorer(support.shape, TopAbs_FACE); explorer.More();
             explorer.Next())
            if (explorer.Current().IsSame(face)) {
                if (explorer.Current().Orientation() != face.Orientation()) {
                    f.z.Reverse();
                    f.y.Reverse();
                }
                break;
            }
        gp_Trsf toFrame;
        toFrame.SetTransformation(gp_Ax3(f.origin, gp_Dir(f.z), gp_Dir(f.x)));
        Bnd_Box bounds;
        BRepBndLib::AddOptimal(face.Moved(TopLoc_Location(toFrame)), bounds, false, false);
        double xMin = 0, yMin = 0, zMin = 0, xMax = 0, yMax = 0, zMax = 0;
        bounds.Get(xMin, yMin, zMin, xMax, yMax, zMax);
        result.push_back({item.origin,
                          face,
                          {f.origin.X(), f.origin.Y(), f.origin.Z(), f.z.X(), f.z.Y(), f.z.Z(),
                           f.x.X(), f.x.Y(), f.x.Z()},
                          xMin,
                          yMin,
                          xMax,
                          yMax});
    }
    return result;
}
document::SketchSupportBinding bindSketchSupport(document::DocumentId document,
                                                 const EvaluatedFeature& support,
                                                 SupportOrigin origin,
                                                 const TopoDS_Shape& selectedFace,
                                                 const document::SketchPlane& sketchPlane) {
    document::validateCanonicalSketchPlane(sketchPlane);
    document::validateSubshapeOrigin(origin);
    if (!support.bodyId)
        throw GeometryEvaluationError("Sketch support has no body identity");
    const auto& resolved = requireOrigin(support, origin);
    if (!resolved.faces.front().IsSame(selectedFace))
        throw GeometryEvaluationError(
            "Selected support face is stale or belongs to another origin");
    requirePlane(sketchPlane, selectedFace);
    return {{document, *support.bodyId, std::move(origin)},
            convert(sketchPlane, frame(resolved.frameFace), true)};
}
void resolveBoundSketchSupport(document::SketchParameters& sketch,
                               const EvaluatedFeature& support) {
    if (sketch.repairReason)
        throw GeometryEvaluationError(
            "MigrationNeedsRepair: explicit sketch reference repair required");
    if (!sketch.supportBinding)
        throw std::logic_error("Missing explicit sketch binding");
    const auto& binding = *sketch.supportBinding;
    if (support.bodyId != binding.reference.body)
        throw GeometryEvaluationError("Sketch support body identity changed");
    const auto& resolved = requireOrigin(support, binding.reference.origin);
    const auto current = convert(binding.localPlane, frame(resolved.frameFace), false);
    requirePlane(current, inBody(resolved.faces.front(), support));
    for (const auto& constraint : sketch.constraints.values)
        if ((constraint.kind == document::SketchConstraintKind::coordinateX ||
             constraint.kind == document::SketchConstraintKind::coordinateY) &&
            constraint.auxiliaryValues.size() == 2)
            throw GeometryEvaluationError(
                "Bound sketch has legacy projected edges; stable edge migration is required");
    sketch.plane = current;
    const gp_Pnt origin(current.originX, current.originY, current.originZ);
    const gp_Vec x(current.xDirectionX, current.xDirectionY, current.xDirectionZ);
    const gp_Vec normal(current.normalX, current.normalY, current.normalZ);
    const auto y = normal.Crossed(x);
    for (auto& constraint : sketch.constraints.values) {
        if (!constraint.projectedEdge)
            continue;
        const auto point = resolveBoundEdgePoint(binding, support, *constraint.projectedEdge);
        const gp_Vec offset(origin, point);
        constraint.value = constraint.kind == document::SketchConstraintKind::coordinateX
                               ? offset.Dot(x)
                               : offset.Dot(y);
    }
}
gp_Pnt resolveBoundEdgePoint(const document::SketchSupportBinding& binding,
                             const EvaluatedFeature& support,
                             const document::ProjectedEdgeReference& ref) {
    document::validateProjectedEdgeReference(ref);
    if (support.bodyId != binding.reference.body)
        throw GeometryEvaluationError("Sketch support body identity changed");
    const auto& resolved = requireOrigin(support, binding.reference.origin);
    if (ref.document != binding.reference.document || ref.body != binding.reference.body)
        throw GeometryEvaluationError("Projected edge namespace differs from sketch support");
    const auto point = resolveEdgePoint(support.edgeOrigins, ref.origin, ref.selection);
    if (point.status != EdgeResolutionStatus::resolved || !point.point) {
        const char* reason = point.status == EdgeResolutionStatus::missing     ? "missing"
                             : point.status == EdgeResolutionStatus::ambiguous ? "ambiguous"
                             : point.status == EdgeResolutionStatus::parameterOutside
                                 ? "parameter outside retained edge"
                                 : "history or parameter mapping unavailable";
        throw GeometryEvaluationError(std::string("Projected edge is ") + reason);
    }
    const auto found = std::ranges::find(support.edgeOrigins, ref.origin, &EdgeProvenance::origin);
    bool onFace = false;
    for (TopExp_Explorer it(resolved.faces.front(), TopAbs_EDGE); it.More(); it.Next())
        onFace |= it.Current().IsSame(found->edges.front());
    if (!onFace)
        throw GeometryEvaluationError("Projected edge is not a boundary of its named support face");
    return *point.point;
}
} // namespace voelund::features
