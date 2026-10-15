#include <voelund/kernel/PlaneNormal.hpp>
#include "PlanarFaceSelection.hpp"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace voelund::viewport {

std::optional<PlanarFaceSelection> planarFaceSelection(std::uint64_t bodyId,
                                                       const TopoDS_Shape& detectedShape) {
    if (detectedShape.IsNull() || detectedShape.ShapeType() != TopAbs_FACE) {
        return std::nullopt;
    }
    const TopoDS_Face face = TopoDS::Face(detectedShape);
    const BRepAdaptor_Surface surface(face);
    if (surface.GetType() != GeomAbs_Plane) {
        return std::nullopt;
    }
    const gp_Pln plane = surface.Plane();
    const gp_Dir normal = kernel::planeNormal(plane, face.Orientation());
    const gp_Pnt origin = plane.Location();
    const gp_Dir xDirection = plane.XAxis().Direction();
    const gp_Dir yDirection = normal.Crossed(xDirection);
    std::vector<PlanarReferenceSegment> references;
    for (TopExp_Explorer explorer(face, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        const BRepAdaptor_Curve curve(TopoDS::Edge(explorer.Current()));
        const int divisions = curve.GetType() == GeomAbs_Line ? 1 : 12;
        const double firstParameter = curve.FirstParameter();
        const double lastParameter = curve.LastParameter();
        gp_Pnt first = curve.Value(firstParameter);
        for (int index = 1; index <= divisions; ++index) {
            const gp_Pnt second =
                curve.Value(firstParameter + (lastParameter - firstParameter) * index / divisions);
            const gp_Vec firstOffset(origin, first);
            const gp_Vec secondOffset(origin, second);
            references.push_back(
                {firstOffset.Dot(gp_Vec(xDirection)), firstOffset.Dot(gp_Vec(yDirection)),
                 secondOffset.Dot(gp_Vec(xDirection)), secondOffset.Dot(gp_Vec(yDirection)),
                 first.X(), first.Y(), first.Z(), second.X(), second.Y(), second.Z(),
                 references.size() + 1});
            first = second;
        }
    }
    return PlanarFaceSelection{bodyId,
                               origin.X(),
                               origin.Y(),
                               origin.Z(),
                               normal.X(),
                               normal.Y(),
                               normal.Z(),
                               xDirection.X(),
                               xDirection.Y(),
                               xDirection.Z(),
                               std::move(references),
                               face};
}

std::optional<PlanarFaceSelection> resolvePlanarFaceSelection(const PlanarFaceSelection& previous,
                                                              const TopoDS_Shape& supportShape) {
    std::optional<PlanarFaceSelection> best;
    double bestDistance = std::numeric_limits<double>::max();
    const gp_Vec previousNormal(previous.normalX, previous.normalY, previous.normalZ);
    const gp_Pnt previousOrigin(previous.originX, previous.originY, previous.originZ);
    for (TopExp_Explorer explorer(supportShape, TopAbs_FACE); explorer.More(); explorer.Next()) {
        const auto candidate = planarFaceSelection(previous.bodyId, explorer.Current());
        if (!candidate.has_value())
            continue;
        const gp_Vec candidateNormal(candidate->normalX, candidate->normalY, candidate->normalZ);
        if (previousNormal.Dot(candidateNormal) < 0.999)
            continue;
        const gp_Pnt candidateOrigin(candidate->originX, candidate->originY, candidate->originZ);
        const double signedDistance = gp_Vec(previousOrigin, candidateOrigin).Dot(previousNormal);
        const double distance = std::abs(signedDistance);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = candidate;
            const gp_Pnt attachedOrigin =
                previousOrigin.Translated(previousNormal * signedDistance);
            best->originX = attachedOrigin.X();
            best->originY = attachedOrigin.Y();
            best->originZ = attachedOrigin.Z();
            best->normalX = previous.normalX;
            best->normalY = previous.normalY;
            best->normalZ = previous.normalZ;
            best->xDirectionX = previous.xDirectionX;
            best->xDirectionY = previous.xDirectionY;
            best->xDirectionZ = previous.xDirectionZ;
            const gp_Vec xDirection(previous.xDirectionX, previous.xDirectionY,
                                    previous.xDirectionZ);
            const gp_Vec yDirection = previousNormal.Crossed(xDirection);
            for (auto& segment : best->referenceSegments) {
                const gp_Vec firstOffset(
                    attachedOrigin,
                    gp_Pnt(segment.firstWorldX, segment.firstWorldY, segment.firstWorldZ));
                const gp_Vec secondOffset(
                    attachedOrigin,
                    gp_Pnt(segment.secondWorldX, segment.secondWorldY, segment.secondWorldZ));
                segment.firstX = firstOffset.Dot(xDirection);
                segment.firstY = firstOffset.Dot(yDirection);
                segment.secondX = secondOffset.Dot(xDirection);
                segment.secondY = secondOffset.Dot(yDirection);
            }
        }
    }
    return best;
}

} // namespace voelund::viewport
