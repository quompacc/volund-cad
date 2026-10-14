#include "SketchConnection.hpp"
#include "SketchRegionGeometry.hpp"
#include "SketchRegionIntervalFaces.hpp"
#include "SketchBSplineCurves.hpp"
#include <voelund/features/GeometryEvaluationError.hpp>

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepGProp.hxx>
#include <BRep_Builder.hxx>
#include <GProp_GProps.hxx>
#include <Geom_Circle.hxx>
#include <Geom_Ellipse.hxx>
#include <Geom_Line.hxx>
#include <Precision.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Ax3.hxx>
#include <gp_Pln.hxx>
#include <gp_Vec2d.hxx>

#include <algorithm>
#include <cmath>
#include <map>
#include <numbers>
#include <optional>
#include <set>
#include <string>

namespace voelund::features::detail {
namespace {

struct Context final {
    const document::SketchParameters& sketch;
    const SketchRegionResult& regions;
    std::vector<PlanarCurve> planar;
    std::map<std::size_t, Handle(Geom_Curve)> curves; // Shared per piece: one source curve.
};

gp_Pnt worldPoint(const document::SketchPlane& plane, document::SketchPoint point) {
    const gp_Dir normal(plane.normalX, plane.normalY, plane.normalZ);
    const gp_Dir x(plane.xDirectionX, plane.xDirectionY, plane.xDirectionZ);
    const gp_Vec y = gp_Vec(normal.Crossed(x));
    return gp_Pnt(plane.originX, plane.originY, plane.originZ)
        .Translated(gp_Vec(x) * point.x + y * point.y);
}

// The 3D source curve carries the same parameters as the R08b planar curve: line lengths
// from the start point, conic angles from the sketch x axis (ellipse: from its major axis).
Handle(Geom_Curve) sourceCurve(Context& context, std::size_t piece) {
    if (const auto found = context.curves.find(piece); found != context.curves.end())
        return found->second;
    const auto& plane = context.sketch.plane;
    const auto& geometry = context.regions.split.pieces[piece].geometry;
    const gp_Dir normal(plane.normalX, plane.normalY, plane.normalZ);
    const gp_Dir x(plane.xDirectionX, plane.xDirectionY, plane.xDirectionZ);
    Handle(Geom_Curve) curve;
    if (const auto* line = std::get_if<document::SketchLine>(&geometry)) {
        const gp_Pnt start = worldPoint(plane, line->start);
        curve = new Geom_Line(gp_Ax1(start, gp_Dir(gp_Vec(start, worldPoint(plane, line->end)))));
    } else if (const auto* circle = std::get_if<document::SketchCircle>(&geometry)) {
        curve = new Geom_Circle(gp_Ax2(worldPoint(plane, circle->center), normal, x),
                                circle->radius);
    } else if (const auto* arc = std::get_if<document::SketchArc>(&geometry)) {
        curve = new Geom_Circle(gp_Ax2(worldPoint(plane, arc->center), normal, x), arc->radius);
    } else if (const auto* spline = std::get_if<document::SketchBSpline>(&geometry)) {
        curve = bsplineCurve3d(*spline, plane); // S21: same parameters as the planar curve
    } else {
        const auto& ellipse = std::get<document::SketchEllipse>(geometry);
        const gp_Dir y = normal.Crossed(x);
        const gp_Dir major(gp_Vec(x) * std::cos(ellipse.rotation) +
                           gp_Vec(y) * std::sin(ellipse.rotation));
        curve = new Geom_Ellipse(gp_Ax2(worldPoint(plane, ellipse.center), normal, major),
                                 ellipse.majorRadius, ellipse.minorRadius);
    }
    context.curves.emplace(piece, curve);
    return curve;
}

ContourRole role(const Context& context, std::size_t piece) {
    const auto& source = context.regions.split.pieces[piece];
    const auto element = std::ranges::find(context.sketch.elements.values, source.element,
                                           &document::SketchElement::id);
    if (element == context.sketch.elements.values.end())
        throw GeometryEvaluationError("Region boundary references an unknown element");
    if (std::holds_alternative<document::SketchRectangle>(element->geometry))
        return static_cast<ContourRole>(static_cast<int>(ContourRole::rectangleFirst) +
                                        static_cast<int>(source.part));
    if (std::holds_alternative<document::SketchLine>(source.geometry))
        return ContourRole::line;
    if (std::holds_alternative<document::SketchCircle>(source.geometry))
        return ContourRole::circle;
    if (std::holds_alternative<document::SketchArc>(source.geometry))
        return ContourRole::arc;
    if (std::holds_alternative<document::SketchBSpline>(source.geometry))
        return ContourRole::bspline;
    return ContourRole::ellipse;
}

document::SketchPoint walkStart(const Context& context, RegionHalfEdge half) {
    const auto& interval = context.regions.split.intervals[half.interval];
    return half.reversed ? interval.end : interval.start;
}
document::SketchPoint walkEnd(const Context& context, RegionHalfEdge half) {
    const auto& interval = context.regions.split.intervals[half.interval];
    return half.reversed ? interval.start : interval.end;
}

// Planar direction in which the walk leaves (start) or arrives at (end) a half-edge.
double walkAngle(const Context& context, RegionHalfEdge half, bool atEnd) {
    const auto& interval = context.regions.split.intervals[half.interval];
    const bool last = atEnd != half.reversed;
    gp_Pnt2d point;
    gp_Vec2d direction;
    context.planar[interval.piece].curve->D1(last ? interval.lastParameter
                                                  : interval.firstParameter,
                                             point, direction);
    if (half.reversed)
        direction.Reverse();
    return std::atan2(direction.Y(), direction.X());
}

std::set<std::uint64_t> walkElements(const Context& context,
                                     const std::vector<RegionHalfEdge>& walk) {
    std::set<std::uint64_t> ids;
    for (const RegionHalfEdge half : walk)
        ids.insert(context.regions.split.pieces[context.regions.split.intervals[half.interval].piece]
                       .element.value());
    return ids;
}

std::string elementList(const Context& context, const std::vector<RegionHalfEdge>& walk) {
    const auto ids = walkElements(context, walk);
    std::string text = " [elements";
    for (const std::uint64_t id : ids)
        text += " " + std::to_string(id);
    return text + "]";
}

// Boundary half-edges of the union: an interval walked in both directions lies between two
// selected cells and cancels.
std::vector<RegionHalfEdge> unionBoundary(const SketchRegionResult& regions,
                                          std::span<const std::size_t> cells) {
    std::vector<RegionHalfEdge> halves;
    for (const std::size_t cell : cells) {
        for (const RegionHalfEdge half : regions.boundaries[regions.regions[cell].outer].edges)
            halves.push_back(half);
        for (const std::size_t hole : regions.regions[cell].holes) {
            const auto& edges = regions.boundaries[hole].edges;
            for (auto it = edges.rbegin(); it != edges.rend(); ++it)
                halves.push_back({it->interval, !it->reversed});
        }
    }
    std::map<std::size_t, std::vector<std::size_t>> byInterval;
    for (std::size_t index = 0; index < halves.size(); ++index)
        byInterval[halves[index].interval].push_back(index);
    std::vector<bool> cancelled(halves.size(), false);
    for (const auto& [interval, uses] : byInterval) {
        if (uses.size() == 1)
            continue;
        if (uses.size() != 2 || halves[uses[0]].reversed == halves[uses[1]].reversed)
            throw GeometryEvaluationError("Selected regions overlap along a boundary",
                                          "region-overlap");
        cancelled[uses[0]] = cancelled[uses[1]] = true;
    }
    std::vector<RegionHalfEdge> result;
    for (std::size_t index = 0; index < halves.size(); ++index)
        if (!cancelled[index])
            result.push_back(halves[index]);
    return result;
}

// Chains the union boundary into closed walks. Where several half-edges leave one point
// (cells touching at a vertex), the walk takes the first one clockwise from where it came
// from, the rule R08b uses for cells, so touching cells stay separate loops.
std::vector<std::vector<RegionHalfEdge>> loops(const Context& context,
                                               const std::vector<RegionHalfEdge>& halves) {
    constexpr double tau = 2 * std::numbers::pi;
    std::vector<bool> used(halves.size(), false);
    std::vector<std::vector<RegionHalfEdge>> result;
    for (std::size_t start = 0; start < halves.size(); ++start) {
        if (used[start])
            continue;
        std::vector<RegionHalfEdge> walk;
        std::size_t current = start;
        while (true) {
            used[current] = true;
            walk.push_back(halves[current]);
            const auto end = walkEnd(context, halves[current]);
            const double back = walkAngle(context, halves[current], true) + std::numbers::pi;
            std::optional<std::size_t> next;
            double best = tau + 1;
            for (std::size_t index = 0; index < halves.size(); ++index) {
                const bool closes = index == start;
                if ((used[index] && !closes) || !connected(end, walkStart(context, halves[index])))
                    continue;
                double turn = std::fmod(back - walkAngle(context, halves[index], false), tau);
                if (turn <= 0)
                    turn += tau;
                if (turn < best) {
                    best = turn;
                    next = index;
                }
            }
            if (!next) {
                const auto ids = walkElements(context, walk);
                throw GeometryEvaluationError("Region boundary is not closed" +
                                                  elementList(context, walk),
                                              "region-open", {ids.begin(), ids.end()});
            }
            if (*next == start)
                break;
            current = *next;
        }
        result.push_back(std::move(walk));
    }
    return result;
}

TopoDS_Wire wire(Context& context, const std::vector<RegionHalfEdge>& walk,
                 std::vector<EvaluatedContourEdge>& edges) {
    const auto& split = context.regions.split;
    const auto point = [&](RegionHalfEdge half, bool atEnd) {
        const auto& interval = split.intervals[half.interval];
        const bool last = atEnd != half.reversed;
        return sourceCurve(context, interval.piece)
            ->Value(last ? interval.lastParameter : interval.firstParameter);
    };
    // One shared vertex per junction, wide enough for both computed curve ends.
    BRep_Builder builder;
    std::vector<TopoDS_Vertex> junctions(walk.size());
    for (std::size_t k = 0; k < walk.size(); ++k) {
        const gp_Pnt arriving = point(walk[(k + walk.size() - 1) % walk.size()], true);
        const gp_Pnt leaving = point(walk[k], false);
        const gp_Pnt middle((arriving.XYZ() + leaving.XYZ()) / 2);
        builder.MakeVertex(junctions[k], middle,
                           std::max(Precision::Confusion(), arriving.Distance(leaving)));
    }
    TopoDS_Wire result;
    builder.MakeWire(result);
    for (std::size_t k = 0; k < walk.size(); ++k) {
        const RegionHalfEdge half = walk[k];
        const auto& interval = split.intervals[half.interval];
        const auto& start = junctions[k];
        const auto& end = junctions[(k + 1) % walk.size()];
        BRepBuilderAPI_MakeEdge make(sourceCurve(context, interval.piece),
                                     half.reversed ? end : start, half.reversed ? start : end,
                                     interval.firstParameter, interval.lastParameter);
        if (!make.IsDone())
            throw GeometryEvaluationError("Region boundary interval did not form an edge" +
                                          elementList(context, walk));
        const TopoDS_Edge edge = make.Edge();
        const auto& planar = context.planar[interval.piece];
        edges.push_back({split.pieces[interval.piece].element, role(context, interval.piece),
                         edge, interval.firstParameter, interval.lastParameter,
                         std::pair{planar.first, planar.last}});
        builder.Add(result, half.reversed ? TopoDS::Edge(edge.Reversed()) : edge);
    }
    result.Closed(true);
    return result;
}

double faceArea(const TopoDS_Face& face) {
    GProp_GProps properties;
    BRepGProp::SurfaceProperties(face, properties);
    return properties.Mass();
}

document::SketchPoint sample(const Context& context, const std::vector<RegionHalfEdge>& walk) {
    const auto& interval = context.regions.split.intervals[walk.front().interval];
    const auto value = context.planar[interval.piece].curve->Value(
        (interval.firstParameter + interval.lastParameter) / 2);
    return {value.X(), value.Y()};
}

} // namespace

bool wholeLoopCells(const SketchRegionResult& regions, std::span<const std::size_t> cells) {
    std::map<std::size_t, std::size_t> intervalsPerPiece;
    for (const auto& interval : regions.split.intervals)
        ++intervalsPerPiece[interval.piece];
    const auto whole = [&](std::size_t boundary) {
        return std::ranges::all_of(regions.boundaries[boundary].edges, [&](RegionHalfEdge half) {
            return intervalsPerPiece[regions.split.intervals[half.interval].piece] == 1;
        });
    };
    return std::ranges::all_of(cells, [&](std::size_t cell) {
        return whole(regions.regions[cell].outer) &&
               std::ranges::all_of(regions.regions[cell].holes, whole);
    });
}

std::vector<EvaluatedProfileFace> intervalRegionFaces(const document::SketchParameters& sketch,
                                                      const SketchRegionResult& regions,
                                                      std::span<const std::size_t> cells) {
    Context context{sketch, regions, {}, {}};
    for (const auto& piece : regions.split.pieces)
        context.planar.push_back(planarCurve(piece));
    const gp_Dir normal(sketch.plane.normalX, sketch.plane.normalY, sketch.plane.normalZ);
    const gp_Dir x(sketch.plane.xDirectionX, sketch.plane.xDirectionY, sketch.plane.xDirectionZ);
    const gp_Pln plane(gp_Ax3(worldPoint(sketch.plane, {0, 0}), normal, x));
    try {
        std::vector<std::vector<RegionHalfEdge>> outers, holes;
        for (auto& walk : loops(context, unionBoundary(regions, cells)))
            (boundaryArea(regions.split, walk) > 0 ? outers : holes).push_back(std::move(walk));
        struct Built {
            TopoDS_Face face;
            double area;
            std::vector<EvaluatedContourEdge> edges;
            std::vector<std::vector<RegionHalfEdge>> holes;
        };
        std::vector<Built> built;
        for (const auto& walk : outers) {
            std::vector<EvaluatedContourEdge> edges;
            const auto outerWire = wire(context, walk, edges);
            BRepBuilderAPI_MakeFace face(plane, outerWire);
            if (!face.IsDone())
                throw GeometryEvaluationError("Region did not produce a valid face" +
                                              elementList(context, walk));
            built.push_back({face.Face(), boundaryArea(regions.split, walk), std::move(edges), {}});
        }
        // Each hole belongs to the smallest outer loop around it.
        for (auto& walk : holes) {
            const gp_Pnt probe = worldPoint(sketch.plane, sample(context, walk));
            Built* owner = nullptr;
            for (Built& candidate : built)
                if (BRepClass_FaceClassifier(candidate.face, probe, Precision::Confusion())
                            .State() != TopAbs_OUT &&
                    (owner == nullptr || candidate.area < owner->area))
                    owner = &candidate;
            if (owner == nullptr)
                throw GeometryEvaluationError("Region hole lies outside its region" +
                                              elementList(context, walk));
            owner->holes.push_back(std::move(walk));
        }
        std::vector<EvaluatedProfileFace> faces;
        for (Built& item : built) {
            double expected = item.area;
            TopoDS_Face result = item.face;
            if (!item.holes.empty()) {
                BRepBuilderAPI_MakeFace face(item.face);
                for (const auto& walk : item.holes) {
                    face.Add(wire(context, walk, item.edges));
                    expected += boundaryArea(regions.split, walk);
                }
                if (!face.IsDone())
                    throw GeometryEvaluationError("Region holes could not be added to the face");
                result = face.Face();
            }
            if (!BRepCheck_Analyzer(result).IsValid())
                throw GeometryEvaluationError("Region face is not valid");
            const double actual = faceArea(result);
            if (std::abs(actual - expected) > 1e-7 * std::max(1.0, std::abs(expected)))
                throw GeometryEvaluationError("Region face area " + std::to_string(actual) +
                                              " differs from its boundary area " +
                                              std::to_string(expected));
            std::set<std::uint64_t> boundary;
            for (const auto& edge : item.edges) {
                bool present = false;
                for (TopExp_Explorer it(result, TopAbs_EDGE); it.More(); it.Next())
                    present |= it.Current().IsSame(edge.edge);
                if (!present)
                    throw GeometryEvaluationError("Region boundary edge is absent from its face");
                boundary.insert(edge.element.value());
            }
            std::vector<document::SketchElementId> ids;
            for (const std::uint64_t id : boundary)
                ids.emplace_back(id);
            faces.push_back({std::move(ids), result, std::move(item.edges)});
        }
        return faces;
    } catch (const Standard_Failure&) {
        // Message accessors differ between OCCT 7.9 and 8; the caller names the elements.
        throw GeometryEvaluationError("Region face construction failed in the kernel");
    }
}

} // namespace voelund::features::detail
