// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <voelund/features/SketchIntersections.hpp>
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
void checkDomains(const SketchIntersectionResult& r) {
    require(r.valid(), "Expected valid arrangement");
    for (auto s : r.intervals)
        require(s.lastParameter > s.firstParameter, "Empty/reversed interval");
}
void crossings() {
    std::vector<SketchElement> e{line(1, -3, 0, 3, 0),
                                 {SketchElementId{2}, SketchCircle{{0, 0}, 2}}};
    auto r = splitSketchIntersections(e);
    checkDomains(r);
    require(r.intersections.size() == 2 && r.intervals.size() == 5, "Line/circle split");
    for (auto h : r.intersections)
        require(h.kind == SketchIntersectionKind::crossing &&
                    std::abs(std::abs(h.point.x) - 2) < 1e-8,
                "Line/circle coordinates");
    std::ranges::reverse(e);
    auto reversed = splitSketchIntersections(e);
    require(reversed.intervals.size() == r.intervals.size() &&
                reversed.intersections.size() == r.intersections.size(),
            "Permutation");
    std::swap(std::get<SketchLine>(e[1].geometry).start, std::get<SketchLine>(e[1].geometry).end);
    require(splitSketchIntersections(e).intersections.size() == 2, "Line reversal");
    e = {line(1, -4, 0, 4, 0), {SketchElementId{2}, SketchEllipse{{0, 0}, 3, 1, 0}}};
    r = splitSketchIntersections(e);
    checkDomains(r);
    require(r.intersections.size() == 2 && r.intervals.size() == 5, "Line/ellipse split");
    e = {{SketchElementId{1}, SketchCircle{{0, 0}, 2}},
         {SketchElementId{2}, SketchCircle{{2, 0}, 2}}};
    r = splitSketchIntersections(e);
    checkDomains(r);
    require(r.intersections.size() == 2, "Circle/circle");
    e = {{SketchElementId{1}, SketchEllipse{{0, 0}, 3, 1, 0}},
         {SketchElementId{2}, SketchEllipse{{0, 0}, 3, 1, std::numbers::pi / 2}}};
    r = splitSketchIntersections(e);
    checkDomains(r);
    require(r.intersections.size() == 4, "Ellipse/ellipse");
    std::cout << "line/circle/ellipse and conic intersections passed\n";
}
void tangency() {
    auto r = splitSketchIntersections(std::vector{
        line(1, -3, 2, 3, 2), SketchElement{SketchElementId{2}, SketchCircle{{0, 0}, 2}}});
    checkDomains(r);
    require(r.intersections.size() == 1 &&
                r.intersections.front().kind == SketchIntersectionKind::tangent,
            "Line tangent");
    r = splitSketchIntersections(std::vector{line(1, 0, 0, 1, 0), line(2, 1, 0, 1, 1)});
    checkDomains(r);
    require(r.intersections.size() == 1 &&
                r.intersections.front().kind == SketchIntersectionKind::endpoint &&
                r.intervals.size() == 2,
            "Endpoint contact");
    r = splitSketchIntersections(std::vector{line(1, -1, 0, 1, 0), line(2, 0, 0, 0, 1)});
    checkDomains(r);
    require(r.intervals.size() == 3, "T junction split");
    std::cout << "tangent and endpoint classification passed\n";
}
void sourceDomains() {
    constexpr double pi = std::numbers::pi;
    std::vector<SketchElement> e{{SketchElementId{9}, SketchArc{{0, 0}, 2, 1.5 * pi, 2.5 * pi}},
                                 line(2, -3, 0, 3, 0)};
    const auto original = e;
    auto r = splitSketchIntersections(e);
    checkDomains(r);
    require(r.intersections.size() == 1 && std::abs(r.intersections.front().point.x - 2) < 1e-8 &&
                e == original,
            "Arc source domain or mutation");
    e = {{SketchElementId{1}, SketchRectangle{0, 0, 2, 2}}, line(2, -1, 1, 3, 1)};
    r = splitSketchIntersections(e);
    checkDomains(r);
    require(r.pieces.size() == 5 && r.intersections.size() == 6 && r.intervals.size() == 9,
            "Rectangle side lineage");
    for (auto p : r.pieces)
        if (p.element == SketchElementId{1})
            require(p.part < 4, "Rectangle part");
    e.back().construction = true;
    r = splitSketchIntersections(e);
    checkDomains(r);
    require(r.pieces.size() == 4 && r.intervals.size() == 4, "Construction included");
    e = {line(1, 0, 0, 2, 2), line(2, 2, 2, 0, 2), line(3, 0, 2, 2, 0), line(4, 2, 0, 0, 0)};
    r = splitSketchIntersections(e);
    checkDomains(r);
    require(r.intervals.size() == 6 && r.intersections.size() == 5,
            "Bowtie self intersection split");
    std::cout << "source domains, rectangle lineage, construction and self crossing passed\n";
}
void stability() {
    auto signature = [](const SketchIntersectionResult& r) {
        std::vector<std::array<long long, 4>> keys;
        for (auto s : r.intervals) {
            auto a = s.start, b = s.end;
            if (std::pair{a.x, a.y} > std::pair{b.x, b.y})
                std::swap(a, b);
            keys.push_back({std::llround(a.x * 1e8), std::llround(a.y * 1e8),
                            std::llround(b.x * 1e8), std::llround(b.y * 1e8)});
        }
        std::ranges::sort(keys);
        return keys;
    };
    std::vector<SketchElement> e{line(50, -3, 0, 3, 0),
                                 line(4, 0, -3, 0, 3),
                                 {SketchElementId{17}, SketchCircle{{0, 0}, 2}}};
    auto base = splitSketchIntersections(e);
    checkDomains(base);
    const auto expected = signature(base);
    for (unsigned mask = 0; mask < 4; ++mask) {
        auto input = e;
        for (std::size_t j = 0; j < 2; ++j)
            if (mask & (1u << j)) {
                auto& l = std::get<SketchLine>(input[j].geometry);
                std::swap(l.start, l.end);
            }
        std::ranges::reverse(input);
        for (std::size_t j = 0; j < input.size(); ++j)
            input[j].id = SketchElementId{100 + 31 * j};
        auto r = splitSketchIntersections(input);
        checkDomains(r);
        require(signature(r) == expected, "IDs/directions alter split geometry");
    }
    e = {line(1, -1, -1, 1, 1), line(2, -1, 0, 1, 0), line(3, 0, -1, 0, 1)};
    auto r = splitSketchIntersections(e);
    checkDomains(r);
    require(r.intervals.size() == 6, "Multiway intersection");
    for (double radius : {.001, 100000.}) {
        e = {line(1, -2 * radius, 0, 2 * radius, 0),
             {SketchElementId{2}, SketchCircle{{0, 0}, radius}}};
        r = splitSketchIntersections(e);
        checkDomains(r);
        require(r.intersections.size() == 2, "Scale range intersection");
    }
    e = {line(1, -1, 0, 1, 0), line(2, 0, -1, 0, 1), line(3, -.5, -1, .5 + 1e-7, 1)};
    r = splitSketchIntersections(e);
    require(!r.valid() && r.intervals.empty() &&
                std::ranges::any_of(r.diagnostics,
                                    [](const auto& d) {
                                        return d.issue ==
                                               SketchIntersectionIssue::unresolvedInterval;
                                    }),
            "Unresolved close split cluster accepted");
    std::cout << "permutation, direction, IDs, multiway crossings and scale range passed\n";
}
void rejection() {
    auto r = splitSketchIntersections(std::vector{line(1, 0, 0, 3, 0), line(2, 1, 0, 2, 0)});
    require(!r.valid() && r.intervals.empty() && r.diagnostics.front().elements.size() == 2,
            "Overlap accepted");
    r = splitSketchIntersections(
        std::vector{SketchElement{SketchElementId{1}, SketchCircle{{0, 0}, 2}},
                    SketchElement{SketchElementId{2}, SketchCircle{{0, 0}, 2}}});
    require(!r.valid() && r.intervals.empty(), "Coincident periodic curves");
    r = splitSketchIntersections(std::vector{line(1, 0, 0, 1, 0), line(2, 2, 0, 3, 0)});
    checkDomains(r);
    require(r.intersections.empty() && r.intervals.size() == 2, "Disjoint finite lines");
    std::cout << "overlap rejection and finite domains passed\n";
}
} // namespace
int main() {
    try {
        crossings();
        tangency();
        sourceDomains();
        rejection();
        stability();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
