#include "../../src/features/SketchRegionIntervalFaces.hpp"
#include <voelund/features/GeometryEvaluationError.hpp>
#include <voelund/features/SketchEvaluation.hpp>
#include <voelund/features/ContourProvenance.hpp>
#include <voelund/features/SketchRegionIdentity.hpp>

#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <voelund/kernel/Prism.hpp>
#include <GProp_GProps.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>

// A-REG-01 / R10 / R10b: selected regions become valid OCCT faces with the right inner wires and
// analytic areas; invalid selections fail with element-localized errors, never empty.
namespace {
using namespace voelund;
using namespace document;
using namespace features;
constexpr double pi = std::numbers::pi;
void require(bool ok, const std::string& why) {
    if (!ok)
        throw std::runtime_error(why);
}
SketchElement line(std::uint64_t id, double x, double y, double u, double v) {
    return {SketchElementId{id}, SketchLine{{x, y}, {u, v}}};
}
SketchElement circle(std::uint64_t id, double x, double y, double r) {
    return {SketchElementId{id}, SketchCircle{{x, y}, r}};
}
SketchParameters sketch(std::vector<SketchElement> elements) {
    std::uint64_t next = 1;
    for (const auto& e : elements)
        next = std::max(next, e.id.value() + 1);
    return {{0, 0, 0, 0, 0, 1, 1, 0, 0}, {std::move(elements), next}};
}
SketchRegionId id(const char* text) {
    const auto parsed = SketchRegionId::parse(text);
    require(parsed.has_value(), std::string("Fixture region ") + text);
    return *parsed;
}
struct Face {
    double area;
    std::size_t wires;
};
std::vector<Face> faces(const SketchParameters& s, const std::vector<SketchRegionId>& regions) {
    std::vector<Face> result;
    for (const auto& face : evaluateRegionFaces(s, regions)) {
        require(BRepCheck_Analyzer(face.face).IsValid(), "Invalid region face");
        GProp_GProps properties;
        BRepGProp::SurfaceProperties(face.face, properties);
        std::size_t wires = 0;
        for (TopExp_Explorer it(face.face, TopAbs_WIRE); it.More(); it.Next())
            ++wires;
        result.push_back({properties.Mass(), wires});
    }
    return result;
}
double area(const TopoDS_Face& face) {
    require(BRepCheck_Analyzer(face).IsValid(), "Invalid region face");
    GProp_GProps properties;
    BRepGProp::SurfaceProperties(face, properties);
    return properties.Mass();
}
bool near(double actual, double expected) {
    return std::abs(actual - expected) <= 1e-9 * std::max(1.0, std::abs(expected));
}
// Every region of the sketch, in canonical order.
std::vector<SketchRegionId> all(const SketchParameters& s) {
    auto ids = identifySketchRegions(s.elements.values).ids;
    std::ranges::sort(ids);
    return ids;
}
std::string failure(const SketchParameters& s, const std::vector<SketchRegionId>& regions) {
    try {
        (void)evaluateRegionFaces(s, regions);
    } catch (const GeometryEvaluationError& error) {
        return error.what();
    }
    return {};
}

void primitives() {
    const auto rectangle = sketch({{SketchElementId{7}, SketchRectangle{1, 2, 9, 7}}});
    auto f = faces(rectangle, all(rectangle));
    require(f.size() == 1 && near(f[0].area, 40) && f[0].wires == 1, "Rectangle");
    const auto mixed =
        sketch({{SketchElementId{3}, SketchArc{{0, 0}, 2, 0, pi}}, line(9, -2, 0, 2, 0)});
    f = faces(mixed, all(mixed));
    require(f.size() == 1 && near(f[0].area, 2 * pi), "Mixed line/arc boundary");
    const auto ellipse = sketch({{SketchElementId{4}, SketchEllipse{{3, 3}, 4, 2, 0.3}}});
    f = faces(ellipse, all(ellipse));
    require(f.size() == 1 && near(f[0].area, 8 * pi), "Ellipse");
    const auto islands = sketch({{SketchElementId{2}, SketchRectangle{0, 0, 4, 3}},
                                 {SketchElementId{1}, SketchRectangle{10, 0, 12, 5}}});
    f = faces(islands, all(islands));
    require(f.size() == 2 && near(f[0].area + f[1].area, 22), "Two separate rectangles");
    std::cout << "rectangle, line/arc, ellipse and separate regions passed\n";
}

void rings() {
    const auto concentric = sketch({circle(8, 0, 0, 20), circle(2, 0, 0, 10)});
    const auto ring = id("o:8.0+;h:2.0+"), inner = id("o:2.0+");
    auto f = faces(concentric, {ring});
    require(f.size() == 1 && near(f[0].area, 300 * pi) && f[0].wires == 2, "Ring 300 pi");
    f = faces(concentric, {inner});
    require(f.size() == 1 && near(f[0].area, 100 * pi) && f[0].wires == 1, "Inner 100 pi");
    f = faces(concentric, {inner, ring});
    require(f.size() == 1 && near(f[0].area, 400 * pi) && f[0].wires == 1,
            "Both selected fill the hole as one face");
    const auto nested = sketch({circle(1, 0, 0, 20), circle(5, 0, 0, 10), circle(3, 0, 0, 5)});
    f = faces(nested, {id("o:1.0+;h:5.0+"), id("o:3.0+")});
    require(f.size() == 2 && f[0].wires + f[1].wires == 3 &&
                near(f[0].area + f[1].area, 300 * pi + 25 * pi),
            "Outer ring and innermost disc stay separate");
    std::cout << "ring, inner region, filled pair and three nesting levels passed\n";
}

void invariance() {
    std::vector<SketchElement> e{line(40, 0, 0, 6, 0), line(3, 6, 0, 6, 4), line(18, 6, 4, 0, 4),
                                 line(7, 0, 4, 0, 0),  circle(90, 3, 2, 1), line(55, 0, 0, 6, 4)};
    e.back().construction = true; // Construction through a region bounds no material.
    const auto expected = faces(sketch(e), all(sketch(e)));
    require(expected.size() == 1 && near(expected[0].area, 24),
            "Square with its selected inner disc is one filled face");
    std::ranges::reverse(e);
    std::swap(std::get<SketchLine>(e[2].geometry).start, std::get<SketchLine>(e[2].geometry).end);
    const auto permuted = faces(sketch(e), all(sketch(e)));
    require(permuted.size() == 1 && near(permuted[0].area, expected[0].area),
            "Permutation, reversed line and construction change geometry");
    for (const double scale : {0.002, 100000.0}) {
        const auto s =
            sketch({circle(1, 300000, -300000, scale), circle(2, 300000, -300000, scale / 2)});
        const auto f = faces(s, {all(s).front()});
        const double ring = 0.75 * pi * scale * scale;
        require(f.size() == 1 && std::abs(f[0].area - ring) <= 1e-7 * std::max(1.0, ring),
                "Translated ring at scale " + std::to_string(scale));
    }
    std::cout << "permutation, orientation, construction and scale matrix passed\n";
}

void localizedFailures() {
    const auto overlap = sketch({line(1, 0, 0, 3, 0), line(2, 1, 0, 2, 0), circle(5, 9, 9, 1)});
    auto message = failure(overlap, {id("o:5.0+")});
    require(message.find("invalid") != std::string::npos &&
                message.find("elements 1 2") != std::string::npos,
            "Overlap must name its elements: " + message);
    const auto tangent = sketch({circle(1, 0, 0, 1), circle(2, 2, 0, 1)});
    message = failure(tangent, {id("o:1.0+")});
    require(message.find("elements 1 2") != std::string::npos,
            "Tangent contact must name its elements: " + message);
    const auto disc = sketch({circle(1, 0, 0, 2)});
    message = failure(disc, {id("o:4.0+")});
    require(message.find("removed") != std::string::npos &&
                message.find("elements 4") != std::string::npos,
            "Missing boundary element must be named: " + message);
    std::cout << "overlap, tangency and removed element localized passed\n";
}
// Cells of the sketch with their R08b areas, smallest first.
std::vector<std::pair<double, SketchRegionId>> cells(const SketchParameters& s) {
    const auto identities = identifySketchRegions(s.elements.values);
    require(identities.regions.valid() && !identities.ids.empty(), "Fixture regions");
    std::vector<std::pair<double, SketchRegionId>> result;
    for (std::size_t i = 0; i < identities.ids.size(); ++i)
        result.push_back({identities.regions.regions[i].area, identities.ids[i]});
    std::ranges::sort(result, {}, &std::pair<double, SketchRegionId>::first);
    return result;
}

void crossings() {
    // Two circles r = 2 whose centres lie 2 apart: lens and two crescents.
    const double lens = 8 * pi / 3 - 2 * std::sqrt(3.0);
    const auto pair = sketch({circle(1, 0, 0, 2), circle(2, 2, 0, 2)});
    const auto c = cells(pair);
    require(c.size() == 3 && near(c[0].first, lens), "Lens and two crescents");
    auto f = faces(pair, {c[0].second});
    require(f.size() == 1 && near(f[0].area, lens) && f[0].wires == 1, "Lens face");
    f = faces(pair, {c[1].second});
    require(f.size() == 1 && near(f[0].area, 4 * pi - lens), "Crescent face");
    f = faces(pair, {c[0].second, c[1].second});
    require(f.size() == 1 && near(f[0].area, 4 * pi) && f[0].wires == 1,
            "Lens and crescent merge into one disc");
    f = faces(pair, all(pair));
    require(f.size() == 1 && near(f[0].area, 8 * pi - lens), "Union of both discs");
    // A chord through a circle: two halves; the dangling line ends bound nothing. The
    // halves are mirror cells, so R09 keeps their RegionId ambiguous; the faces are built
    // from the cells directly.
    const auto chord = sketch({circle(1, 0, 0, 2), line(2, -3, 0, 3, 0)});
    const auto arrangement = buildSketchRegions(chord.elements.values);
    require(arrangement.regions.size() == 2, "Chord halves");
    for (const std::size_t half : {std::size_t{0}, std::size_t{1}}) {
        const std::size_t selected[]{half};
        const auto built = detail::intervalRegionFaces(chord, arrangement, selected);
        require(built.size() == 1 && near(area(built[0].face), 2 * pi), "Half disc");
    }
    const std::size_t both[]{0, 1};
    const auto disc = detail::intervalRegionFaces(chord, arrangement, both);
    require(disc.size() == 1 && near(area(disc[0].face), 4 * pi), "Halves merge into a disc");
    require(failure(chord, {all(chord).front()}).find("several cells") != std::string::npos,
            "Chord halves stay ambiguous by RegionId");
    // Self-intersection: a bow-tie's lobes touch in one point and stay two faces.
    const auto bowTie = sketch(
        {line(1, 0, 0, 2, 2), line(2, 2, 2, 0, 2), line(3, 0, 2, 2, 0), line(4, 2, 0, 0, 0)});
    f = faces(bowTie, {cells(bowTie)[0].second});
    require(f.size() == 1 && near(f[0].area, 1), "Bow-tie lobe");
    f = faces(bowTie, all(bowTie));
    require(f.size() == 2 && near(f[0].area + f[1].area, 2), "Touching lobes stay separate");
    // A rectangle cut by its diagonal: two triangles from split rectangle sides.
    const auto cut =
        sketch({{SketchElementId{5}, SketchRectangle{0, 0, 4, 2}}, line(6, 0, 0, 4, 2)});
    f = faces(cut, {cells(cut)[0].second});
    require(f.size() == 1 && near(f[0].area, 4), "Triangle of a cut rectangle");
    // Crossing contours inside a ring: the hole is the outline of both discs.
    const auto ring = sketch({circle(1, 0, 0, 2), circle(2, 2, 0, 2), circle(3, 1, 0, 10)});
    f = faces(ring, {cells(ring).back().second});
    require(f.size() == 1 && f[0].wires == 2 && near(f[0].area, 100 * pi - (8 * pi - lens)),
            "Ring around crossing discs");
    std::cout << "lens, crescents, chord halves, bow-tie, cut rectangle and ring passed\n";
}

// R10b: cap references of a split element use its whole circle and survive moving cuts.
void capReferences() {
    const auto resolve = [](const SketchParameters& s, const SketchRegionId& region, double t) {
        const auto profiles = evaluateRegionFaces(s, std::vector{region});
        for (const auto& profile : profiles)
            for (const auto& edge : profile.edges)
                require(edge.element != SketchElementId{1} ||
                            (edge.role == ContourRole::circle && edge.domain &&
                             near(edge.domain->second - edge.domain->first, 2 * pi)),
                        "Fragment keeps its element, role and whole domain");
        const voelund::kernel::Prism prism(profiles.front().face, 0, 0, 1, 5);
        const auto origins = extrusionEdgeOrigins(FeatureId{2}, FeatureId{9}, profiles, prism);
        const ContourEdgeOrigin cap{FeatureId{9}, SketchElementId{1}, ContourRole::circle,
                                    FeatureId{2}, ContourStage::extrusionEnd};
        return resolveEdgePoint(origins, cap, {EdgePointKind::parameter, t});
    };
    // The left crescent is the cell whose arc of circle 1 passes angle pi, i.e. (-2, 0).
    const auto before = sketch({circle(1, 0, 0, 2), circle(2, 2, 0, 2)});
    const auto left = [&](const SketchParameters& s) {
        for (const auto& cell : cells(s))
            for (const auto& profile : evaluateRegionFaces(s, std::vector{cell.second}))
                if (std::ranges::any_of(profile.edges, [](const auto& e) {
                        return e.element == SketchElementId{1} && e.startParameter < pi &&
                               e.endParameter > pi;
                    }))
                    return cell.second;
        throw std::runtime_error("No left crescent");
    };
    const auto region = left(before);
    auto point = resolve(before, region, 0.5);
    require(point.status == EdgeResolutionStatus::resolved && point.point &&
                point.point->Distance(gp_Pnt(-2, 0, 5)) < 1e-9,
            "Cap point on the crescent arc");
    require(resolve(before, region, 0).status == EdgeResolutionStatus::parameterOutside,
            "A point inside the lens is not on the crescent");
    // Dimension change: the second circle moves, the cuts move, the reference stays.
    const auto after = sketch({circle(1, 0, 0, 2), circle(2, 2.5, 0, 2)});
    const auto ids = identifySketchRegions(after.elements.values).ids;
    require(std::ranges::find(ids, region) != ids.end(), "Crescent keeps its RegionId");
    point = resolve(after, region, 0.5);
    require(point.status == EdgeResolutionStatus::resolved && point.point &&
                point.point->Distance(gp_Pnt(-2, 0, 5)) < 1e-9,
            "Cap point survives the dimension change");
    std::cout << "cap references on split elements passed\n";
}
} // namespace

int main() {
    try {
        primitives();
        rings();
        invariance();
        localizedFailures();
        crossings();
        capReferences();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
