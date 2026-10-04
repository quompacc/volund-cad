#include "SketchConnection.hpp"
#include "SketchRegionGeometry.hpp"
#include <Standard_Failure.hxx>
#include <algorithm>
#include <cmath>
#include <gp_Vec2d.hxx>
#include <numbers>
#include <numeric>
#include <stdexcept>
namespace voelund::features {
namespace {
struct Sets {
    std::vector<std::size_t> p;
    explicit Sets(std::size_t n) : p(n) { std::iota(p.begin(), p.end(), 0); }
    std::size_t root(std::size_t i) {
        while (p[i] != i) {
            p[i] = p[p[i]];
            i = p[i];
        }
        return i;
    }
    void join(std::size_t i, std::size_t j) {
        i = root(i);
        j = root(j);
        p[std::max(i, j)] = std::min(i, j);
    }
};
} // namespace
SketchRegionResult buildSketchRegions(std::span<const document::SketchElement> elements) {
    SketchRegionResult r;
    r.split = splitSketchIntersections(elements);
    auto diagnose = [&](SketchRegionIssue issue, std::vector<std::size_t> pieces) {
        if (pieces.empty())
            for (std::size_t i = 0; i < r.split.pieces.size(); ++i)
                pieces.push_back(i);
        std::vector<document::SketchElementId> ids;
        for (auto p : pieces)
            ids.push_back(r.split.pieces[p].element);
        std::ranges::sort(ids, {}, &document::SketchElementId::value);
        ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
        r.diagnostics.push_back({issue, std::move(ids)});
    };
    if (!r.split.valid()) {
        for (const auto& d : r.split.diagnostics)
            r.diagnostics.push_back({SketchRegionIssue::intersectionFailure, d.elements});
        return r;
    }
    std::vector<detail::PlanarCurve> curves;
    for (const auto& p : r.split.pieces)
        curves.push_back(detail::planarCurve(p));
    // Smooth endpoint-to-endpoint joins are allowed. Other tangencies need a
    // higher-order topology rule and are rejected explicitly for v1.
    for (const auto& hit : r.split.intersections)
        if (hit.kind == SketchIntersectionKind::tangent) {
            auto endpoint = [](const auto& c, double u) {
                return !c.periodic && (u == c.first || u == c.last);
            };
            if (!endpoint(curves[hit.firstPiece], hit.firstParameter) ||
                !endpoint(curves[hit.secondPiece], hit.secondParameter))
                diagnose(SketchRegionIssue::tangentContact, {hit.firstPiece, hit.secondPiece});
        }
    if (!r.valid())
        return r;
    const auto n = r.split.intervals.size();
    if (n == 0)
        return r;
    Sets vertices(2 * n);
    auto point = [&](std::size_t h) {
        const auto& i = r.split.intervals[h / 2];
        return h % 2 ? i.end : i.start;
    };
    for (std::size_t i = 0; i < 2 * n; ++i)
        for (std::size_t j = i + 1; j < 2 * n; ++j)
            if (detail::connected(point(i), point(j)))
                vertices.join(i, j);
    std::vector<std::vector<std::size_t>> outgoing(2 * n);
    for (std::size_t h = 0; h < 2 * n; ++h)
        outgoing[vertices.root(h)].push_back(h);
    // K07: a sketch may be open. Dangling chains bound no material, so they are pruned
    // iteratively instead of invalidating every region; bridges between cycles remain
    // rejected below because their walk traverses an interval twice.
    std::vector<bool> pruned(n, false);
    for (bool changed = true; changed;) {
        changed = false;
        for (auto& fan : outgoing) {
            if (fan.size() != 1)
                continue;
            const std::size_t interval = fan.front() / 2;
            pruned[interval] = true;
            changed = true;
            for (const std::size_t h : {2 * interval, 2 * interval + 1})
                std::erase(outgoing[vertices.root(h)], h);
        }
    }
    std::vector<double> angle(2 * n);
    try {
        for (std::size_t h = 0; h < 2 * n; ++h) {
            const auto& i = r.split.intervals[h / 2];
            gp_Pnt2d p;
            gp_Vec2d d;
            curves[i.piece].curve->D1(h % 2 ? i.lastParameter : i.firstParameter, p, d);
            if (h % 2)
                d.Reverse();
            angle[h] = std::atan2(d.Y(), d.X());
        }
        for (auto& fan : outgoing) {
            if (fan.empty())
                continue;
            std::vector<std::size_t> pieces;
            for (auto h : fan)
                pieces.push_back(r.split.intervals[h / 2].piece);
            if (fan.size() < 2)
                diagnose(SketchRegionIssue::openBoundary, pieces);
            bool ambiguous = false;
            for (auto a : fan)
                for (auto b : fan)
                    ambiguous |= !detail::connected(point(a), point(b));
            if (ambiguous)
                diagnose(SketchRegionIssue::ambiguousConnection, pieces);
            std::ranges::sort(fan, [&](auto a, auto b) { return angle[a] < angle[b]; });
            for (std::size_t k = 0; k < fan.size(); ++k) {
                const auto a = fan[k], b = fan[(k + 1) % fan.size()];
                if (a != b && std::abs(std::remainder(angle[a] - angle[b], 2 * std::numbers::pi)) <=
                                  kernel::angularTolerance.radians)
                    diagnose(SketchRegionIssue::tangentContact, pieces);
            }
        }
        if (!r.valid())
            return r;
        std::vector<std::size_t> next(2 * n);
        for (std::size_t h = 0; h < 2 * n; ++h) {
            if (pruned[h / 2])
                continue;
            const auto twin = h ^ 1u;
            const auto& fan = outgoing[vertices.root(twin)];
            const auto at = static_cast<std::size_t>(std::ranges::find(fan, twin) - fan.begin());
            next[h] = fan[(at + fan.size() - 1) % fan.size()];
        }
        std::vector<bool> visited(2 * n, false);
        for (std::size_t start = 0; start < 2 * n; ++start) {
            if (visited[start] || pruned[start / 2])
                continue;
            std::vector<RegionHalfEdge> walk;
            std::vector<std::size_t> intervals;
            auto h = start;
            do {
                if (visited[h])
                    throw std::runtime_error("Broken half-edge permutation");
                visited[h] = true;
                walk.push_back({h / 2, h % 2 != 0});
                intervals.push_back(h / 2);
                h = next[h];
            } while (h != start);
            auto sorted = intervals;
            std::ranges::sort(sorted);
            if (std::ranges::adjacent_find(sorted) != sorted.end()) {
                std::vector<std::size_t> pieces;
                for (auto i : sorted)
                    pieces.push_back(r.split.intervals[i].piece);
                diagnose(SketchRegionIssue::openBoundary, pieces);
                continue; // Bridge traversed twice.
            }
            const auto area = detail::boundaryArea(r.split, walk);
            if (area == 0) {
                std::vector<std::size_t> pieces;
                for (auto i : sorted)
                    pieces.push_back(r.split.intervals[i].piece);
                diagnose(SketchRegionIssue::degenerateBoundary, pieces);
            } else if (area > 0)
                r.boundaries.push_back({std::move(walk), area});
        }
        if (r.valid())
            detail::classifyRegionContainment(r, curves);
    } catch (const Standard_Failure&) {
        diagnose(SketchRegionIssue::kernelFailure, {});
    } catch (const std::runtime_error&) {
        diagnose(SketchRegionIssue::kernelFailure, {});
    }
    if (!r.valid()) {
        r.boundaries.clear();
        r.regions.clear();
    }
    return r;
}
} // namespace voelund::features
