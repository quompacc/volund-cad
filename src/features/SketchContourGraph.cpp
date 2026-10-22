// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchConnection.hpp"
#include "SketchContourGeometry.hpp"
#include <algorithm>
#include <array>
#include <numeric>
#include <tuple>
namespace voelund::features {
namespace {
using document::SketchPoint;
auto pointKey(SketchPoint p) { return std::pair{p.x, p.y}; }
auto pieceKey(const ContourPiece& p) {
    auto a = pointKey(p.start), b = pointKey(p.end);
    if (b < a)
        std::swap(a, b);
    const auto middle = detail::contourMiddle(p);
    std::array<double, 5> shape{};
    if (auto* c = std::get_if<document::SketchCircle>(&p.geometry))
        shape = {c->center.x, c->center.y, c->radius, 0, 0};
    if (auto* e = std::get_if<document::SketchEllipse>(&p.geometry))
        shape = {e->center.x, e->center.y, e->majorRadius, e->minorRadius, e->rotation};
    return std::tuple{a, b, pointKey(middle), p.geometry.index(), shape};
}
struct Sets {
    std::vector<std::size_t> parent;
    explicit Sets(std::size_t n) : parent(n) { std::iota(parent.begin(), parent.end(), 0); }
    std::size_t root(std::size_t i) {
        while (parent[i] != i) {
            parent[i] = parent[parent[i]];
            i = parent[i];
        }
        return i;
    }
    void join(std::size_t a, std::size_t b) {
        a = root(a);
        b = root(b);
        parent[std::max(a, b)] = std::min(a, b);
    }
};
} // namespace
SketchContourGraph buildSketchContourGraph(std::span<const document::SketchElement> elements) {
    SketchContourGraph result;
    result.pieces = detail::contourPieces(elements);
    auto& pieces = result.pieces;
    std::ranges::sort(pieces, [](const auto& a, const auto& b) {
        if (pieceKey(a) != pieceKey(b))
            return pieceKey(a) < pieceKey(b);
        return std::pair{a.element.value(), a.part} < std::pair{b.element.value(), b.part};
    });
    const auto n = pieces.size();
    Sets vertices(2 * n), components(n);
    std::vector<bool> bad(n, false);
    auto point = [&](std::size_t i) { return i % 2 ? pieces[i / 2].end : pieces[i / 2].start; };
    auto diagnose = [&](ContourIssue issue, std::vector<std::size_t> indices) {
        ContourDiagnostic d{issue, {}};
        for (auto i : indices) {
            bad[i] = true;
            d.elements.push_back(pieces[i].element);
        }
        std::ranges::sort(d.elements, {}, &document::SketchElementId::value);
        d.elements.erase(std::unique(d.elements.begin(), d.elements.end()), d.elements.end());
        result.diagnostics.push_back(std::move(d));
    };
    for (std::size_t i = 0; i < n; ++i) {
        if (!pieces[i].periodic && detail::connected(pieces[i].start, pieces[i].end))
            diagnose(ContourIssue::degenerate, {i});
        for (std::size_t j = i + 1; j < n; ++j)
            if (detail::contourOverlap(pieces[i], pieces[j])) {
                components.join(i, j);
                diagnose(ContourIssue::overlap, {i, j});
            }
    }
    for (std::size_t i = 0; i < 2 * n; ++i) {
        if (pieces[i / 2].periodic)
            continue;
        for (std::size_t j = i + 1; j < 2 * n; ++j) {
            if (pieces[j / 2].periodic)
                continue;
            if (detail::connected(point(i), point(j))) {
                vertices.join(i, j);
                components.join(i / 2, j / 2);
            }
        }
    }
    for (std::size_t i = 0; i < n; ++i) {
        if (pieces[i].periodic)
            continue;
        for (std::size_t j = 0; j < n; ++j) {
            if (i == j)
                continue;
            if (detail::contourInterior(pieces[j], pieces[i].start) ||
                detail::contourInterior(pieces[j], pieces[i].end)) {
                components.join(i, j);
                diagnose(ContourIssue::branch, {i, j});
            }
        }
    }
    std::vector<std::vector<std::size_t>> incident(2 * n);
    for (std::size_t i = 0; i < 2 * n; ++i)
        if (!pieces[i / 2].periodic)
            incident[vertices.root(i)].push_back(i);
    for (const auto& ends : incident) {
        if (ends.empty())
            continue;
        std::vector<std::size_t> ids;
        for (auto e : ends)
            ids.push_back(e / 2);
        // A transitive tolerance chain must never bridge a gap beyond tolerance.
        bool ambiguous = false;
        for (auto a : ends)
            for (auto b : ends)
                ambiguous |= !detail::connected(point(a), point(b));
        if (ambiguous)
            diagnose(ContourIssue::ambiguousConnection, ids);
        if (ends.size() == 1)
            diagnose(ContourIssue::openEnd, ids);
        if (ends.size() > 2)
            diagnose(ContourIssue::branch, ids);
    }
    std::vector<bool> invalid(n, false), used(n, false);
    for (std::size_t i = 0; i < n; ++i)
        if (bad[i])
            invalid[components.root(i)] = true;
    for (std::size_t i = 0; i < n; ++i) {
        if (used[i] || invalid[components.root(i)])
            continue;
        if (pieces[i].periodic) {
            result.loops.push_back({{{i, false}}});
            used[i] = true;
            continue;
        }
        ContourLoop loop;
        std::size_t current = i;
        bool reversed = pointKey(pieces[i].end) < pointKey(pieces[i].start);
        while (!used[current]) {
            loop.boundary.push_back({current, reversed});
            used[current] = true;
            const auto endpoint = 2 * current + (reversed ? 0u : 1u);
            const auto& ends = incident[vertices.root(endpoint)];
            const auto next = ends[0] == endpoint ? ends[1] : ends[0];
            current = next / 2;
            reversed = next % 2 != 0;
        }
        result.loops.push_back(std::move(loop));
    }
    std::ranges::sort(result.diagnostics, [](const auto& a, const auto& b) {
        if (a.issue != b.issue)
            return a.issue < b.issue;
        return std::lexicographical_compare(a.elements.begin(), a.elements.end(),
                                            b.elements.begin(), b.elements.end(),
                                            [](auto x, auto y) { return x.value() < y.value(); });
    });
    result.diagnostics.erase(std::unique(result.diagnostics.begin(), result.diagnostics.end(),
                                         [](const auto& a, const auto& b) {
                                             return a.issue == b.issue && a.elements == b.elements;
                                         }),
                             result.diagnostics.end());
    return result;
}
} // namespace voelund::features
