// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "FaceReplacement.hpp"
#include "OperationHistoryDetail.hpp"
#include <voelund/kernel/KernelError.hpp>

#include <BRepAdaptor_Curve.hxx>
#include <BRepBuilderAPI_ModifyShape.hxx>
#include <BRepLib.hxx>
#include <BRepTools_Modification.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <Geom2d_Curve.hxx>
#include <GeomAPI_IntCS.hxx>
#include <GeomAPI_IntSS.hxx>
#include <GeomAPI_ProjectPointOnCurve.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <GeomLib_Tool.hxx>
#include <GeomProjLib.hxx>
#include <Geom_Curve.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <NCollection_IndexedDataMap.hxx>
#include <NCollection_List.hxx>
#include <Precision.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS.hxx>
#include <gp_Vec2d.hxx>

#include <algorithm>
#include <cmath>
#include <map>
#include <unordered_map>
#include <utility>

namespace voelund::kernel::detail {
namespace {

constexpr auto operation = KernelOperation::evaluation;
constexpr double meetTolerance = 1e-6;
const char* const noMeeting = "the faces around cannot follow";

using ShapeMap = NCollection_DataMap<TopoDS_Shape, int, TopTools_ShapeMapHasher>;

using AncestorMap = NCollection_IndexedDataMap<TopoDS_Shape, NCollection_List<TopoDS_Shape>,
                                               TopTools_ShapeMapHasher>;

class FaceModification final : public BRepTools_Modification {
  public:
    FaceModification(const TopoDS_Shape& input, std::span<const FaceReplacement> faces);

    bool NewSurface(const TopoDS_Face& face, occ::handle<Geom_Surface>& surface,
                    TopLoc_Location& location, double& tolerance, bool& reverseWires,
                    bool& reverseFace) override;
    bool NewCurve(const TopoDS_Edge& edge, occ::handle<Geom_Curve>& curve,
                  TopLoc_Location& location, double& tolerance) override;
    bool NewPoint(const TopoDS_Vertex& vertex, gp_Pnt& point, double& tolerance) override;
    bool NewCurve2d(const TopoDS_Edge& edge, const TopoDS_Face& face, const TopoDS_Edge& newEdge,
                    const TopoDS_Face& newFace, occ::handle<Geom2d_Curve>& curve,
                    double& tolerance) override;
    bool NewParameter(const TopoDS_Vertex& vertex, const TopoDS_Edge& edge, double& parameter,
                      double& tolerance) override;
    GeomAbs_Shape Continuity(const TopoDS_Edge& edge, const TopoDS_Face& first,
                             const TopoDS_Face& second, const TopoDS_Edge&, const TopoDS_Face&,
                             const TopoDS_Face&) override {
        return BRep_Tool::Continuity(edge, first, second);
    }

  private:
    struct NewEdge final {
        occ::handle<Geom_Curve> curve;
        bool rigid{false}; // pcurves stay as they are
    };

    [[nodiscard]] const FaceReplacement* replacement(const TopoDS_Shape& face) const;
    [[nodiscard]] occ::handle<Geom_Surface> surfaceOf(const TopoDS_Face& face) const;
    [[nodiscard]] std::vector<TopoDS_Face> facesOf(const TopoDS_Shape& edge) const;
    [[nodiscard]] std::optional<gp_Trsf> commonRigid(const std::vector<TopoDS_Face>& faces) const;
    [[nodiscard]] gp_Trsf movedAlong(const std::vector<TopoDS_Face>& faces) const;
    [[nodiscard]] bool touched(const std::vector<TopoDS_Face>& faces) const;
    [[nodiscard]] occ::handle<Geom_Curve> cutCurve(const TopoDS_Edge& edge,
                                                   const std::vector<TopoDS_Face>& faces,
                                                   const gp_Trsf& along) const;
    [[nodiscard]] occ::handle<Geom_Curve> tangentCurve(const TopoDS_Edge& edge,
                                                       const std::vector<TopoDS_Face>& faces,
                                                       const gp_Trsf& along) const;
    [[nodiscard]] std::optional<gp_Pnt> cutPoint(const TopoDS_Vertex& vertex, const gp_Pnt& start,
                                                 const std::vector<TopoDS_Face>& faces) const;
    [[nodiscard]] NewEdge meetingCurve(const TopoDS_Edge& edge,
                                       const std::vector<TopoDS_Face>& faces) const;
    [[nodiscard]] occ::handle<Geom_Curve> seamCurve(const TopoDS_Edge& edge,
                                                    const TopoDS_Face& face) const;
    [[nodiscard]] bool onAll(const gp_Pnt& point, const std::vector<TopoDS_Face>& faces) const;
    [[nodiscard]] gp_Pnt meetingPoint(const TopoDS_Vertex& vertex) const;
    [[nodiscard]] occ::handle<Geom_Curve> curveOf(const TopoDS_Edge& edge) const;
    [[nodiscard]] gp_Pnt pointOf(const TopoDS_Vertex& vertex) const;
    [[nodiscard]] double parameterOf(const TopoDS_Vertex& vertex, const TopoDS_Edge& edge) const;
    [[nodiscard]] std::pair<double, double> rangeOf(const TopoDS_Edge& edge) const;
    void requireOrder(const TopoDS_Edge& edge) const;

    std::vector<FaceReplacement> faces_;
    AncestorMap edgeFaces_;
    AncestorMap vertexFaces_;
    AncestorMap vertexEdges_;
    NCollection_DataMap<TopoDS_Shape, NewEdge, TopTools_ShapeMapHasher> edges_;
    NCollection_DataMap<TopoDS_Shape, gp_Pnt, TopTools_ShapeMapHasher> vertices_;
};

const FaceReplacement* FaceModification::replacement(const TopoDS_Shape& face) const {
    for (const auto& item : faces_)
        if (item.face.IsSame(face))
            return &item;
    return nullptr;
}

occ::handle<Geom_Surface> FaceModification::surfaceOf(const TopoDS_Face& face) const {
    const auto* item = replacement(face);
    return item != nullptr ? item->surface : BRep_Tool::Surface(face);
}

std::vector<TopoDS_Face> FaceModification::facesOf(const TopoDS_Shape& shape) const {
    const auto& map = shape.ShapeType() == TopAbs_EDGE ? edgeFaces_ : vertexFaces_;
    std::vector<TopoDS_Face> result;
    for (const auto& face : map.FindFromKey(shape)) {
        const auto& candidate = TopoDS::Face(face);
        bool known = false;
        for (const auto& other : result)
            known = known || other.IsSame(candidate);
        if (!known)
            result.push_back(candidate);
    }
    return result;
}

// The transformation when all the faces move as one rigid part.
std::optional<gp_Trsf> FaceModification::commonRigid(const std::vector<TopoDS_Face>& faces) const {
    const FaceReplacement* first = nullptr;
    for (const auto& face : faces) {
        const auto* item = replacement(face);
        if (item == nullptr || !item->rigid || (first != nullptr && first->group != item->group))
            return std::nullopt;
        first = item;
    }
    return first != nullptr ? first->rigid : std::nullopt;
}

double distanceToCurve(const gp_Pnt& point, const occ::handle<Geom_Curve>& curve) {
    double parameter = 0;
    if (GeomLib_Tool::Parameter(curve, point, 1e100, parameter))
        return point.Distance(curve->Value(parameter));
    GeomAPI_ProjectPointOnCurve projection(point, curve);
    return projection.NbPoints() > 0 ? projection.LowerDistance() : 1e100;
}

// A seam stays on its parameter line on the new surface.
occ::handle<Geom_Curve> FaceModification::seamCurve(const TopoDS_Edge& edge,
                                                    const TopoDS_Face& face) const {
    double first = 0;
    double last = 0;
    const auto pcurve = BRep_Tool::CurveOnSurface(edge, face, first, last);
    if (pcurve.IsNull())
        throw KernelError(operation, noMeeting);
    return surfaceOf(face)->UIso(pcurve->Value((first + last) / 2).X());
}

// Keeps the direction of the old edge: wires and pcurves rely on it. `along` is how the part next
// to it moved: the old middle only says something once it has moved along too.
occ::handle<Geom_Curve> withDirectionOf(const TopoDS_Edge& edge,
                                        const occ::handle<Geom_Curve>& curve,
                                        const gp_Trsf& along) {
    const BRepAdaptor_Curve old(edge);
    const double middle = (old.FirstParameter() + old.LastParameter()) / 2;
    gp_Pnt point;
    gp_Vec tangent;
    old.D1(middle, point, tangent);
    point.Transform(along);
    tangent.Transform(along);
    double parameter = 0;
    GeomAPI_ProjectPointOnCurve projection(point, curve);
    if (projection.NbPoints() > 0)
        parameter = projection.LowerDistanceParameter();
    else if (!GeomLib_Tool::Parameter(curve, point, 1e100, parameter))
        throw KernelError(operation, noMeeting);
    gp_Pnt onNew;
    gp_Vec newTangent;
    curve->D1(parameter, onNew, newTangent);
    return newTangent.Dot(tangent) < 0 ? curve->Reversed() : curve;
}

// How a rigid part next to these faces moves; identity when none does.
gp_Trsf FaceModification::movedAlong(const std::vector<TopoDS_Face>& faces) const {
    gp_Trsf along;
    for (const auto& face : faces)
        if (const auto* item = replacement(face); item != nullptr && item->rigid)
            along = *item->rigid;
    return along;
}

// The intersection of the two new surfaces nearest the old edge; null when they do not cut.
occ::handle<Geom_Curve> FaceModification::cutCurve(const TopoDS_Edge& edge,
                                                   const std::vector<TopoDS_Face>& faces,
                                                   const gp_Trsf& along) const {
    const BRepAdaptor_Curve old(edge);
    const gp_Pnt middle =
        old.Value((old.FirstParameter() + old.LastParameter()) / 2).Transformed(along);
    GeomAPI_IntSS meeting(surfaceOf(faces[0]), surfaceOf(faces[1]), Precision::Confusion());
    occ::handle<Geom_Curve> best;
    double bestDistance = 1e100;
    if (!meeting.IsDone())
        return best;
    for (int index = 1; index <= meeting.NbLines(); ++index) {
        // A whole circle comes trimmed: only the circle itself knows its period.
        auto candidate = meeting.Line(index);
        if (const auto trimmed = occ::handle<Geom_TrimmedCurve>::DownCast(candidate))
            candidate = trimmed->BasisCurve();
        const double distance = distanceToCurve(middle, candidate);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = candidate;
        }
    }
    return best;
}

// Tangent faces where one part moved along the other: the edge moves with the part, if it then
// still lies on both.
occ::handle<Geom_Curve> FaceModification::tangentCurve(const TopoDS_Edge& edge,
                                                       const std::vector<TopoDS_Face>& faces,
                                                       const gp_Trsf& along) const {
    double first = 0;
    double last = 0;
    const auto moved =
        occ::handle<Geom_Curve>::DownCast(BRep_Tool::Curve(edge, first, last)->Transformed(along));
    if (!onAll(moved->Value((first + last) / 2), faces))
        throw KernelError(operation, noMeeting);
    return moved;
}

FaceModification::NewEdge
FaceModification::meetingCurve(const TopoDS_Edge& edge,
                               const std::vector<TopoDS_Face>& faces) const {
    if (const auto rigid = commonRigid(faces)) {
        double first = 0;
        double last = 0;
        const auto curve = BRep_Tool::Curve(edge, first, last);
        return {occ::handle<Geom_Curve>::DownCast(curve->Transformed(*rigid)), true};
    }
    if (faces.size() != 2)
        return {seamCurve(edge, faces.front()), false};
    const gp_Trsf along = movedAlong(faces);
    const auto cut = cutCurve(edge, faces, along);
    if (!cut.IsNull())
        return {withDirectionOf(edge, cut, along), false};
    return {tangentCurve(edge, faces, along), false};
}

occ::handle<Geom_Curve> FaceModification::curveOf(const TopoDS_Edge& edge) const {
    if (const auto* changed = edges_.Seek(edge))
        return changed->curve;
    double first = 0;
    double last = 0;
    return BRep_Tool::Curve(edge, first, last);
}

gp_Pnt FaceModification::pointOf(const TopoDS_Vertex& vertex) const {
    if (const auto* changed = vertices_.Seek(vertex))
        return *changed;
    return BRep_Tool::Pnt(vertex);
}

bool FaceModification::onAll(const gp_Pnt& point, const std::vector<TopoDS_Face>& faces) const {
    for (const auto& face : faces) {
        GeomAPI_ProjectPointOnSurf projection(point, surfaceOf(face));
        if (projection.NbPoints() == 0 || projection.LowerDistance() > meetTolerance)
            return false;
    }
    return true;
}

// The point on all the faces nearest `start` among what the vertex's edges and faces cut.
std::optional<gp_Pnt> FaceModification::cutPoint(const TopoDS_Vertex& vertex, const gp_Pnt& start,
                                                 const std::vector<TopoDS_Face>& faces) const {
    std::optional<gp_Pnt> best;
    const auto consider = [&](const gp_Pnt& point) {
        if ((!best || point.Distance(start) < best->Distance(start)) && onAll(point, faces))
            best = point;
    };
    for (const auto& shape : vertexEdges_.FindFromKey(vertex)) {
        const auto& edge = TopoDS::Edge(shape);
        if (BRep_Tool::Degenerated(edge))
            continue;
        const auto curve = curveOf(edge);
        for (const auto& face : faces) {
            GeomAPI_IntCS meeting(curve, surfaceOf(face));
            for (int index = 1; meeting.IsDone() && index <= meeting.NbPoints(); ++index)
                consider(meeting.Point(index));
        }
        GeomAPI_ProjectPointOnCurve projection(start, curve);
        if (projection.NbPoints() > 0)
            consider(projection.NearestPoint());
    }
    return best;
}

// Where the vertex's faces meet, nearest to where it would be if it only moved along. First a new
// edge through a third face; tangent faces give no clean cut, so then step between the surfaces.
gp_Pnt FaceModification::meetingPoint(const TopoDS_Vertex& vertex) const {
    const auto faces = facesOf(vertex);
    const gp_Pnt old = BRep_Tool::Pnt(vertex);
    if (const auto rigid = commonRigid(faces))
        return old.Transformed(*rigid);
    const gp_Pnt start = old.Transformed(movedAlong(faces));
    if (const auto cut = cutPoint(vertex, start, faces))
        return *cut;
    gp_Pnt point = start;
    for (int step = 0; step < 200; ++step)
        for (const auto& face : faces) {
            GeomAPI_ProjectPointOnSurf projection(point, surfaceOf(face));
            if (projection.NbPoints() > 0)
                point = projection.NearestPoint();
        }
    if (!onAll(point, faces))
        throw KernelError(operation, noMeeting);
    return point;
}

bool FaceModification::touched(const std::vector<TopoDS_Face>& faces) const {
    return std::ranges::any_of(faces,
                               [&](const auto& face) { return replacement(face) != nullptr; });
}

FaceModification::FaceModification(const TopoDS_Shape& input,
                                   std::span<const FaceReplacement> faces)
    : faces_(faces.begin(), faces.end()) {
    TopExp::MapShapesAndAncestors(input, TopAbs_EDGE, TopAbs_FACE, edgeFaces_);
    TopExp::MapShapesAndAncestors(input, TopAbs_VERTEX, TopAbs_FACE, vertexFaces_);
    TopExp::MapShapesAndAncestors(input, TopAbs_VERTEX, TopAbs_EDGE, vertexEdges_);
    for (int index = 1; index <= edgeFaces_.Extent(); ++index) {
        const auto& edge = TopoDS::Edge(edgeFaces_.FindKey(index));
        if (const auto around = facesOf(edge); !BRep_Tool::Degenerated(edge) && touched(around))
            edges_.Bind(edge, meetingCurve(edge, around));
    }
    for (int index = 1; index <= vertexFaces_.Extent(); ++index) {
        const auto& vertex = TopoDS::Vertex(vertexFaces_.FindKey(index));
        if (touched(facesOf(vertex)))
            vertices_.Bind(vertex, meetingPoint(vertex));
    }
    for (int index = 1; index <= edgeFaces_.Extent(); ++index)
        requireOrder(TopoDS::Edge(edgeFaces_.FindKey(index)));
}

// An edge that turned around means a neighbour vanished: the faces moved too far.
void FaceModification::requireOrder(const TopoDS_Edge& edge) const {
    if (BRep_Tool::Degenerated(edge))
        return;
    TopoDS_Vertex first;
    TopoDS_Vertex last;
    TopExp::Vertices(edge, first, last);
    if (first.IsNull() || last.IsNull() || first.IsSame(last))
        return;
    if (!vertices_.IsBound(first) && !vertices_.IsBound(last))
        return;
    // Curved edges can legally change their chord; BRepCheck and the volume judge those.
    if (BRepAdaptor_Curve(edge).GetType() != GeomAbs_Line)
        return;
    const gp_Vec before(BRep_Tool::Pnt(first), BRep_Tool::Pnt(last));
    const gp_Vec after(pointOf(first), pointOf(last));
    if (after.Dot(before) <= 0)
        throw KernelError(operation, "the faces cannot be moved this far");
}

bool FaceModification::NewSurface(const TopoDS_Face& face, occ::handle<Geom_Surface>& surface,
                                  TopLoc_Location& location, double& tolerance, bool& reverseWires,
                                  bool& reverseFace) {
    const auto* item = replacement(face);
    if (item == nullptr)
        return false;
    surface = item->surface;
    location = TopLoc_Location();
    tolerance = BRep_Tool::Tolerance(face);
    reverseWires = false;
    reverseFace = false;
    return true;
}

bool FaceModification::NewCurve(const TopoDS_Edge& edge, occ::handle<Geom_Curve>& curve,
                                TopLoc_Location& location, double& tolerance) {
    const auto* changed = edges_.Seek(edge);
    if (changed == nullptr)
        return false;
    curve = changed->curve;
    location = TopLoc_Location();
    tolerance = std::max(BRep_Tool::Tolerance(edge), meetTolerance);
    return true;
}

bool FaceModification::NewPoint(const TopoDS_Vertex& vertex, gp_Pnt& point, double& tolerance) {
    const auto* changed = vertices_.Seek(vertex);
    if (changed == nullptr)
        return false;
    point = *changed;
    tolerance = std::max(BRep_Tool::Tolerance(vertex), meetTolerance);
    return true;
}

// A new pcurve on the same turn of a periodic surface as the old one: the wire stays closed.
occ::handle<Geom2d_Curve> onSameTurn(occ::handle<Geom2d_Curve> curve, double first,
                                     const occ::handle<Geom2d_Curve>& old, double oldFirst,
                                     const occ::handle<Geom_Surface>& surface) {
    const gp_Pnt2d now = curve->Value(first);
    const gp_Pnt2d before = old->Value(oldFirst);
    gp_Vec2d shift(0, 0);
    if (surface->IsUPeriodic())
        shift.SetX(std::round((before.X() - now.X()) / surface->UPeriod()) * surface->UPeriod());
    if (surface->IsVPeriodic())
        shift.SetY(std::round((before.Y() - now.Y()) / surface->VPeriod()) * surface->VPeriod());
    if (shift.Magnitude() > 0)
        curve->Translate(shift);
    return curve;
}

bool FaceModification::NewCurve2d(const TopoDS_Edge& edge, const TopoDS_Face& face,
                                  const TopoDS_Edge& newEdge, const TopoDS_Face&,
                                  occ::handle<Geom2d_Curve>& curve, double& tolerance) {
    if (BRep_Tool::Degenerated(edge))
        return false;
    const auto* changed = edges_.Seek(edge);
    const auto* item = replacement(face);
    // The new edge's range is not reliable yet: the modifier sets its vertices later.
    const auto [first, last] = rangeOf(edge);
    BRep_Builder().Range(newEdge, first, last);
    double oldFirst = 0;
    double oldLast = 0;
    const auto old = BRep_Tool::CurveOnSurface(edge, face, oldFirst, oldLast);
    // Pcurves belong to a surface: a rigid move keeps the parameters, but the new surface still
    // has to be told.
    if (changed != nullptr && changed->rigid) {
        curve = old;
        tolerance = BRep_Tool::Tolerance(edge);
        return !old.IsNull();
    }
    if (changed == nullptr && item == nullptr && oldFirst <= first && last <= oldLast)
        return false;
    const auto surface = surfaceOf(face);
    const occ::handle<Geom_Curve> trimmed = new Geom_TrimmedCurve(curveOf(edge), first, last);
    tolerance = std::max(BRep_Tool::Tolerance(edge), meetTolerance);
    curve = GeomProjLib::Curve2d(trimmed, first, last, surface, tolerance);
    if (curve.IsNull())
        throw KernelError(operation, noMeeting);
    if (!old.IsNull())
        curve = onSameTurn(curve, first, old, oldFirst, surface);
    return true;
}

double FaceModification::parameterOf(const TopoDS_Vertex& vertex, const TopoDS_Edge& edge) const {
    const auto curve = curveOf(edge);
    double parameter = 0;
    if (GeomLib_Tool::Parameter(curve, pointOf(vertex), 1e100, parameter))
        return parameter;
    GeomAPI_ProjectPointOnCurve projection(pointOf(vertex), curve);
    if (projection.NbPoints() == 0)
        throw KernelError(operation, noMeeting);
    return projection.LowerDistanceParameter();
}

// The new edge's parameters in curve direction; on a closed curve the end lies a turn on if need
// be.
std::pair<double, double> FaceModification::rangeOf(const TopoDS_Edge& edge) const {
    TopoDS_Vertex first;
    TopoDS_Vertex last;
    TopExp::Vertices(edge, first, last);
    const double start = parameterOf(first, edge);
    double end = parameterOf(last, edge);
    const auto curve = curveOf(edge);
    if (curve->IsClosed() && end <= start + Precision::PConfusion())
        end = curve->IsPeriodic() ? end + curve->Period() : curve->LastParameter();
    return {start, end};
}

bool FaceModification::NewParameter(const TopoDS_Vertex& vertex, const TopoDS_Edge& edge,
                                    double& parameter, double& tolerance) {
    if ((!vertices_.IsBound(vertex) && !edges_.IsBound(edge)) || curveOf(edge).IsNull())
        return false;
    tolerance = std::max(BRep_Tool::Tolerance(vertex), meetTolerance);
    // The vertex comes oriented relative to the edge as given.
    TopoDS_Vertex first = TopExp::FirstVertex(edge);
    first.Orientation(edge.Orientation() == TopAbs_REVERSED ? TopAbs_REVERSED : TopAbs_FORWARD);
    const auto [start, end] = rangeOf(edge);
    parameter = vertex.IsEqual(first) ? start : end;
    return true;
}

class FaceModifier final : public BRepBuilderAPI_ModifyShape {
  public:
    FaceModifier(const TopoDS_Shape& input,
                 const occ::handle<BRepTools_Modification>& modification) {
        DoModif(input, modification);
    }
};

} // namespace

ShapeOperationResult replaceFaces(const TopoDS_Shape& input,
                                  std::span<const FaceReplacement> faces) {
    const occ::handle<FaceModification> modification = new FaceModification(input, faces);
    FaceModifier modifier(input, modification);
    if (!modifier.IsDone())
        throw KernelError(operation, noMeeting);
    const TopoDS_Shape shape = modifier.Shape();
    BRepLib::SameParameter(shape, 1e-5, true);
    ShapeOperationResult result{shape, {}, {}};
    appendHistory(result, modifier, true, input, HistoryOperand::first);
    return result;
}

} // namespace voelund::kernel::detail
