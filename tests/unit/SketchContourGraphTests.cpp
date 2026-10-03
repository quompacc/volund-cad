#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <algorithm>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <voelund/features/SketchContourGraph.hpp>
#include <voelund/features/SketchEvaluation.hpp>
#include <voelund/features/SketchProfiles.hpp>
#include <voelund/kernel/NumericPolicy.hpp>
using namespace voelund;
using namespace document;
using namespace features;
namespace {
void require(bool ok, const char* why) {
    if (!ok)
        throw std::runtime_error(why);
}
SketchElement line(std::uint64_t id, double x, double y, double u, double v) {
    return {SketchElementId{id}, SketchLine{{x, y}, {u, v}}};
}
bool issue(const SketchContourGraph& g, ContourIssue kind) {
    return std::ranges::any_of(
        g.diagnostics, [&](const auto& d) { return d.issue == kind && !d.elements.empty(); });
}
using Signature = std::vector<std::pair<SketchPoint, SketchPoint>>;
Signature signature(const SketchContourGraph& g) {
    Signature s;
    for (const auto& l : g.loops)
        for (auto e : l.boundary) {
            const auto& p = g.pieces[e.piece];
            s.emplace_back(e.reversed ? p.end : p.start, e.reversed ? p.start : p.end);
        }
    return s;
}
double area(const std::vector<SketchElement>& elements) {
    SketchParameters p{{0, 0, 0, 0, 0, 1, 1, 0, 0}, {elements, 10000}, {}};
    auto shape = evaluateSketchProfileFaces(p);
    GProp_GProps props;
    BRepGProp::SurfaceProperties(shape, props);
    return props.Mass();
}
void permutations() {
    std::vector<SketchElement> original{line(40, 0, 0, 4, 0), line(3, 4, 0, 4, 3),
                                        line(18, 4, 3, 0, 3), line(7, 0, 3, 0, 0)};
    const auto expected = signature(buildSketchContourGraph(original));
    std::vector<int> order{0, 1, 2, 3};
    unsigned count = 0;
    do {
        for (unsigned mask = 0; mask < 16; ++mask) {
            std::vector<SketchElement> input;
            for (auto j : order) {
                auto e = original[static_cast<std::size_t>(j)];
                if (mask & (1u << j)) {
                    auto& l = std::get<SketchLine>(e.geometry);
                    std::swap(l.start, l.end);
                }
                e.id = SketchElementId{static_cast<std::uint64_t>(100 + 17 * j)};
                input.push_back(e);
            }
            const auto g = buildSketchContourGraph(input);
            require(g.loops.size() == 1 && g.diagnostics.empty() && signature(g) == expected,
                    "Permutation/orientation changes graph");
            require(std::abs(area(input) - 12) < 1e-8, "Orientation changes face area");
            ++count;
        }
    } while (std::next_permutation(order.begin(), order.end()));
    std::cout << "rectangle permutations and directions: " << count
              << " graph/face checks passed\n";
}
void analytic() {
    std::vector<SketchElement> elements{
        {SketchElementId{9}, SketchArc{{0, 0}, 2, 0, std::numbers::pi}}, line(2, -2, 0, 2, 0)};
    for (int i = 0; i < 4; ++i) {
        auto g = buildSketchContourGraph(elements);
        require(g.loops.size() == 1 && g.diagnostics.empty(), "Arc/line loop");
        require(std::abs(area(elements) - 2 * std::numbers::pi) < 1e-7, "Arc face area");
        std::ranges::reverse(elements);
        for (auto& e : elements)
            if (auto* l = std::get_if<SketchLine>(&e.geometry))
                std::swap(l->start, l->end);
    }
    elements = {{SketchElementId{1}, SketchRectangle{0, 0, 2, 3}},
                {SketchElementId{2}, SketchCircle{{10, 0}, 2}},
                {SketchElementId{3}, SketchEllipse{{20, 0}, 3, 1, .3}},
                line(4, 0, 0, 2, 3)};
    elements.back().construction = true;
    auto g = buildSketchContourGraph(elements);
    require(g.pieces.size() == 6 && g.loops.size() == 3 && g.diagnostics.empty(),
            "Primitive conversion or construction");
    require(std::abs(area(elements) - (6 + 7 * std::numbers::pi)) < 1e-7, "Primitive areas");
    const auto original = elements;
    std::ranges::reverse(elements);
    require(signature(buildSketchContourGraph(elements)) ==
                signature(buildSketchContourGraph(original)),
            "Primitive order depends on input");
    std::vector<SketchElement> halves{
        {SketchElementId{8}, SketchArc{{0, 0}, 2, 0, std::numbers::pi}},
        {SketchElementId{2}, SketchArc{{0, 0}, 2, std::numbers::pi, 2 * std::numbers::pi}}};
    require(buildSketchContourGraph(halves).loops.size() == 1 &&
                std::abs(area(halves) - 4 * std::numbers::pi) < 1e-7,
            "Complementary arcs mistaken for duplicates");
    std::vector<SketchElement> boxes{{SketchElementId{5}, SketchRectangle{2, 3, 0, 0}},
                                     {SketchElementId{2}, SketchRectangle{5, 0, 7, 3}}};
    require(buildSketchContourGraph(boxes).loops.size() == 2 && std::abs(area(boxes) - 12) < 1e-7,
            "Reversed legacy rectangle corners");
    std::cout << "analytic line/arc, rectangle, circle, ellipse and construction passed\n";
}
void invalid() {
    auto g = buildSketchContourGraph(std::vector{line(1, 0, 0, 1, 0), line(2, 1, 0, 1, 1)});
    require(g.loops.empty() && issue(g, ContourIssue::openEnd), "Open contour");
    g = buildSketchContourGraph(std::vector{line(1, 0, 0, 2, 0), line(2, 1, 0, 1, 1)});
    require(g.loops.empty() && issue(g, ContourIssue::branch), "Interior T branch");
    std::vector<SketchElement> square{line(1, 0, 0, 2, 0), line(2, 2, 0, 2, 2), line(3, 2, 2, 0, 2),
                                      line(4, 0, 2, 0, 0)};
    square.push_back(line(5, 0, 0, -1, 0));
    g = buildSketchContourGraph(square);
    require(g.loops.empty() && issue(g, ContourIssue::branch),
            "Branch cannot silently yield a region");
    square.back() = line(5, .5, 0, 1.5, 0);
    g = buildSketchContourGraph(square);
    require(g.loops.empty() && issue(g, ContourIssue::overlap), "Partial line overlap");
    square.back() = line(5, 2, 0, 0, 0);
    g = buildSketchContourGraph(square);
    require(g.loops.empty() && issue(g, ContourIssue::overlap), "Reversed duplicate");
    square.push_back({SketchElementId{6}, SketchCircle{{10, 0}, 1}});
    require(buildSketchContourGraph(square).loops.size() == 1,
            "Invalid component masks independent contour");
    std::vector<SketchElement> arcs{{SketchElementId{1}, SketchArc{{0, 0}, 2, 0, 2}},
                                    {SketchElementId{2}, SketchArc{{0, 0}, 2, 1, 3}}};
    require(issue(buildSketchContourGraph(arcs), ContourIssue::overlap), "Partial arc overlap");
    std::vector<SketchElement> ellipses{
        {SketchElementId{1}, SketchEllipse{{0, 0}, 2, 1, 0}},
        {SketchElementId{2}, SketchEllipse{{0, 0}, 2, 1, std::numbers::pi}}};
    require(buildSketchContourGraph(ellipses).loops.empty() &&
                issue(buildSketchContourGraph(ellipses), ContourIssue::overlap),
            "Periodic duplicates");
    std::cout
        << "localized open, branch, line/arc/ellipse overlap and independent component passed\n";
}
void tolerance() {
    const double eps = kernel::connectionTolerance(1).millimetres;
    std::vector<SketchElement> e{line(1, 0, 0, 1, 0), line(2, 1 + eps * .5, 0, 1, 1),
                                 line(3, 1, 1, 0, 0)};
    require(buildSketchContourGraph(e).loops.size() == 1, "Within tolerance");
    std::get<SketchLine>(e[1].geometry).start.x = 1 + eps * 2;
    require(buildSketchContourGraph(e).loops.empty(), "Outside tolerance bridged");
    e = {line(1, 0, 0, 1, 0), line(2, 1 + eps * .75, 0, 2, 1), line(3, 1 + eps * 1.5, 0, 2, -1)};
    require(issue(buildSketchContourGraph(e), ContourIssue::ambiguousConnection),
            "Transitive tolerance bridge");
    std::ranges::reverse(e);
    require(issue(buildSketchContourGraph(e), ContourIssue::ambiguousConnection),
            "Tolerance depends on order");
    std::cout << "tolerance boundary and non-transitive ambiguity passed\n";
}
} // namespace
int main() {
    try {
        permutations();
        analytic();
        invalid();
        tolerance();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
