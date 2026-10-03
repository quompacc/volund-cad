#include <algorithm>
#include <cmath>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <voelund/features/SketchRegions.hpp>
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
auto areas(const SketchRegionResult& r) {
    std::vector<double> a;
    for (const auto& region : r.regions)
        a.push_back(region.area);
    std::ranges::sort(a);
    return a;
}
void valid(const SketchRegionResult& r) {
    if (!r.valid()) {
        std::string s = "Invalid regions:";
        for (auto d : r.diagnostics)
            s += " " + std::to_string(static_cast<int>(d.issue));
        for (const auto& d : r.split.diagnostics) {
            s += " split=" + std::to_string(static_cast<int>(d.issue));
            for (auto id : d.elements)
                s += "/" + std::to_string(id.value());
        }
        throw std::runtime_error(s);
    }
    for (const auto& b : r.boundaries)
        require(b.area > 0 && !b.edges.empty(), "Unoriented boundary");
}
SketchRegionResult regions(const std::vector<SketchElement>& elements) {
    std::uint64_t next = 1;
    for (const auto& e : elements)
        next = std::max(next, e.id.value() + 1);
    if (!elements.empty())
        validateSketchElements({elements, next});
    return buildSketchRegions(elements);
}
void nested() {
    constexpr double pi = std::numbers::pi;
    std::vector<SketchElement> e{{SketchElementId{8}, SketchCircle{{0, 0}, 20}},
                                 {SketchElementId{2}, SketchCircle{{0, 0}, 10}}};
    auto r = regions(e);
    valid(r);
    auto a = areas(r);
    require(a.size() == 2 && std::abs(a[0] - 100 * pi) < 1e-7 && std::abs(a[1] - 300 * pi) < 1e-7,
            "Ring and explicit inner region");
    require(std::ranges::count_if(r.regions, [](auto v) { return v.holes.size() == 1; }) == 1,
            "Missing hole");
    e.push_back({SketchElementId{20}, SketchCircle{{0, 0}, 5}});
    r = regions(e);
    valid(r);
    a = areas(r);
    require(a.size() == 3 && std::abs(a[0] - 25 * pi) < 1e-7 && std::abs(a[1] - 75 * pi) < 1e-7 &&
                std::abs(a[2] - 300 * pi) < 1e-7,
            "Three levels containment");
    std::ranges::reverse(e);
    auto reversed = regions(e);
    valid(reversed);
    require(areas(reversed) == a, "Nested input permutation");
    std::cout << "concentric ring, separate inner region and three containment levels passed\n";
}
void arrangements() {
    std::vector<SketchElement> e{{SketchElementId{1}, SketchCircle{{0, 0}, 2}},
                                 line(2, -2, 0, 2, 0)};
    auto r = regions(e);
    valid(r);
    auto a = areas(r);
    require(a.size() == 2 && std::abs(a[0] - 2 * std::numbers::pi) < 1e-7 &&
                std::abs(a[1] - a[0]) < 1e-7,
            "Circle divided by chord");
    e = {line(1, 0, 0, 2, 2), line(2, 2, 2, 0, 2), line(3, 0, 2, 2, 0), line(4, 2, 0, 0, 0)};
    r = regions(e);
    valid(r);
    a = areas(r);
    require(a.size() == 2 && std::abs(a[0] - 1) < 1e-8 && std::abs(a[1] - 1) < 1e-8,
            "Self crossing cells");
    e = {{SketchElementId{1}, SketchCircle{{0, 0}, 2}},
         {SketchElementId{2}, SketchCircle{{2, 0}, 2}}};
    r = regions(e);
    valid(r);
    a = areas(r);
    const double lens = 8 * std::numbers::pi / 3 - 2 * std::sqrt(3.);
    require(a.size() == 3 && std::abs(a[0] - lens) < 1e-7 &&
                std::abs(a[1] - (4 * std::numbers::pi - lens)) < 1e-7,
            "Crossing circles");
    std::cout << "line/circle chord, self crossing and crossing-circle cells passed\n";
}
void primitives() {
    std::vector<SketchElement> e{{SketchElementId{5}, SketchRectangle{0, 0, 8, 6}},
                                 {SketchElementId{2}, SketchEllipse{{4, 3}, 2, 1, .4}},
                                 {SketchElementId{1}, SketchRectangle{10, 0, 12, 2}},
                                 line(9, 0, 0, 8, 6)};
    e.back().construction = true;
    auto r = regions(e);
    valid(r);
    auto a = areas(r);
    require(a.size() == 3 && std::abs(a[0] - 4) < 1e-8 &&
                std::abs(a[1] - 2 * std::numbers::pi) < 1e-7 &&
                std::abs(a[2] - (48 - 2 * std::numbers::pi)) < 1e-7,
            "Rectangle/ellipse containment");
    e = {{SketchElementId{2}, SketchArc{{0, 0}, 2, 0, std::numbers::pi}},
         {SketchElementId{1}, SketchArc{{0, 0}, 2, std::numbers::pi, 2 * std::numbers::pi}}};
    r = regions(e);
    valid(r);
    require(r.regions.size() == 1 && std::abs(r.regions[0].area - 4 * std::numbers::pi) < 1e-7,
            "Smooth arc joins");
    std::cout << "rectangle/ellipse, separate islands, construction and smooth arc joins passed\n";
}
void stability() {
    std::vector<SketchElement> original{line(40, 0, 0, 4, 0),
                                        line(3, 4, 0, 4, 3),
                                        line(18, 4, 3, 0, 3),
                                        line(7, 0, 3, 0, 0),
                                        {SketchElementId{90}, SketchCircle{{2, 1.5}, .5}}};
    const auto snapshot = original;
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
            input.push_back(original.back());
            auto r = regions(input);
            valid(r);
            auto a = areas(r);
            require(a.size() == 2 && std::abs(a[0] - .25 * std::numbers::pi) < 1e-7 &&
                        std::abs(a[1] - (12 - .25 * std::numbers::pi)) < 1e-7,
                    "Permutation/direction changes region");
            ++count;
        }
    } while (std::next_permutation(order.begin(), order.end()));
    require(original == snapshot, "Region construction mutates input");
    for (double radius : {.002, 100000.}) {
        std::vector<SketchElement> e{
            {SketchElementId{1}, SketchCircle{{300000, -300000}, radius}},
            {SketchElementId{2}, SketchCircle{{300000, -300000}, radius / 2}}};
        auto r = regions(e);
        valid(r);
        auto a = areas(r);
        require(a.size() == 2 && std::abs(a[1] - .75 * std::numbers::pi * radius * radius) <
                                     1e-7 * std::max(1., a[1]),
                "Translated/scaled ring");
    }
    std::cout << "region permutation/direction/IDs: " << count
              << " cases plus translated scale limits passed\n";
}
void rejected() {
    auto r = regions(std::vector{line(1, 0, 0, 1, 0)});
    require(!r.valid() && r.regions.empty() && !r.diagnostics.front().elements.empty(),
            "Open boundary accepted");
    r = regions(std::vector{SketchElement{SketchElementId{1}, SketchCircle{{0, 0}, 1}},
                            SketchElement{SketchElementId{2}, SketchCircle{{2, 0}, 1}}});
    require(!r.valid() && r.regions.empty(), "Tangent circles accepted");
    r = regions(std::vector{line(1, 0, 0, 3, 0), line(2, 1, 0, 2, 0)});
    require(!r.valid() && r.regions.empty(), "Overlap accepted");
    r = regions(std::vector{SketchElement{SketchElementId{1}, SketchCircle{{0, 0}, 1}},
                            SketchElement{SketchElementId{2}, SketchCircle{{4, 0}, 1}},
                            line(3, 1, 0, 3, 0)});
    require(!r.valid() && r.regions.empty(), "Bridge between cycles accepted");
    r = regions(std::vector{
        SketchElement{SketchElementId{1}, SketchArc{{0, 0}, 1, 0, std::numbers::pi}},
        SketchElement{SketchElementId{2},
                      SketchArc{{0, 0}, 1, std::numbers::pi - 1e-10, 2 * std::numbers::pi}}});
    require(!r.valid() && r.regions.empty(), "Tiny arc overlap accepted as smooth join");
    std::cout << "open boundaries, tangent contacts and overlaps explicitly rejected\n";
}
} // namespace
int main() {
    try {
        nested();
        arrangements();
        primitives();
        rejected();
        stability();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
