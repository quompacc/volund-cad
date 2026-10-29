// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchSplineFit.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

namespace voelund::application::detail {
namespace {

using document::SketchPoint;
constexpr int degree = 3;

// The clamped uniform knots of a cubic spline over `poles` poles: 0 four times, 1, 2, ...,
// poles - 3 four times.
std::vector<double> knotsFor(std::size_t poles) {
    const double last = static_cast<double>(poles) - degree;
    std::vector<double> knots(degree + 1, 0.0);
    for (std::size_t inner = 1; inner < poles - degree; ++inner)
        knots.push_back(static_cast<double>(inner));
    knots.insert(knots.end(), degree + 1, last);
    return knots;
}

double ratio(double value, double span) { return span > 0.0 ? value / span : 0.0; }

// Basis function i of degree p at t, or its `derivative` (Cox-de Boor). The last parameter
// belongs to the last non-empty span.
double basis(const std::vector<double>& u, std::size_t i, int p, double t, int derivative) {
    if (derivative > 0) {
        if (p == 0)
            return 0.0;
        return p * (ratio(basis(u, i, p - 1, t, derivative - 1), u[i + p] - u[i]) -
                    ratio(basis(u, i + 1, p - 1, t, derivative - 1), u[i + p + 1] - u[i + 1]));
    }
    if (p == 0) {
        const bool last = t >= u.back() && i + degree + 2 == u.size();
        return (u[i] <= t && t < u[i + 1]) || last ? 1.0 : 0.0;
    }
    return ratio((t - u[i]) * basis(u, i, p - 1, t, 0), u[i + p] - u[i]) +
           ratio((u[i + p + 1] - t) * basis(u, i + 1, p - 1, t, 0), u[i + p + 1] - u[i + 1]);
}

// Solves rows * x = right (both columns at once) by elimination with partial pivoting.
std::vector<SketchPoint> solve(std::vector<std::vector<double>> rows,
                               std::vector<SketchPoint> right) {
    const std::size_t n = rows.size();
    for (std::size_t column = 0; column < n; ++column) {
        std::size_t pivot = column;
        for (std::size_t row = column + 1; row < n; ++row)
            if (std::abs(rows[row][column]) > std::abs(rows[pivot][column]))
                pivot = row;
        if (std::abs(rows[pivot][column]) < 1e-12)
            return {};
        std::swap(rows[pivot], rows[column]);
        std::swap(right[pivot], right[column]);
        for (std::size_t row = column + 1; row < n; ++row) {
            const double factor = rows[row][column] / rows[column][column];
            for (std::size_t k = column; k < n; ++k)
                rows[row][k] -= factor * rows[column][k];
            right[row].x -= factor * right[column].x;
            right[row].y -= factor * right[column].y;
        }
    }
    std::vector<SketchPoint> x(n);
    for (std::size_t row = n; row-- > 0;) {
        SketchPoint sum = right[row];
        for (std::size_t k = row + 1; k < n; ++k) {
            sum.x -= rows[row][k] * x[k].x;
            sum.y -= rows[row][k] * x[k].y;
        }
        x[row] = {sum.x / rows[row][row], sum.y / rows[row][row]};
    }
    return x;
}

// The spline through `fit` whose `derivative`-th derivatives at both ends are `ends`.
std::vector<SketchPoint> fitted(std::span<const SketchPoint> fit, int derivative,
                                std::pair<SketchPoint, SketchPoint> ends) {
    const std::size_t poles = fit.size() + 2;
    if (fit.size() < 2 || poles > document::maximumBSplinePoles)
        return {};
    for (std::size_t index = 1; index < fit.size(); ++index)
        if (std::hypot(fit[index].x - fit[index - 1].x, fit[index].y - fit[index - 1].y) < 1e-9)
            return {};
    const auto u = knotsFor(poles);
    std::vector<std::vector<double>> rows;
    std::vector<SketchPoint> right(fit.begin(), fit.end());
    const auto row = [&](double t, int order) {
        std::vector<double> values(poles);
        for (std::size_t j = 0; j < poles; ++j)
            values[j] = basis(u, j, degree, t, order);
        rows.push_back(std::move(values));
    };
    for (std::size_t index = 0; index < fit.size(); ++index)
        row(static_cast<double>(index), 0);
    row(0.0, derivative);
    right.push_back(ends.first);
    row(u.back(), derivative);
    right.push_back(ends.second);
    return solve(std::move(rows), std::move(right));
}

} // namespace

std::vector<SketchPoint> splineThroughPoints(std::span<const SketchPoint> fit) {
    return fitted(fit, 2, {{0.0, 0.0}, {0.0, 0.0}}); // natural ends
}

std::vector<SketchPoint> splineThroughPoints(std::span<const SketchPoint> fit,
                                             SketchPoint startDerivative,
                                             SketchPoint endDerivative) {
    return fitted(fit, 1, {startDerivative, endDerivative});
}

SketchPoint splinePoint(std::span<const SketchPoint> poles, double parameter) {
    const auto u = knotsFor(poles.size());
    SketchPoint point{0.0, 0.0};
    for (std::size_t j = 0; j < poles.size(); ++j) {
        const double weight = basis(u, j, degree, parameter, 0);
        point.x += weight * poles[j].x;
        point.y += weight * poles[j].y;
    }
    return point;
}

} // namespace voelund::application::detail
