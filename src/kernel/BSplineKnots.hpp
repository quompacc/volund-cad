// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <NCollection_Array1.hxx>

#include <cstddef>
#include <span>
#include <vector>

// S21/N14c2: the knots of a sketch B-spline in OCCT's form. A sketch stores them flat (each
// knot repeated by its multiplicity, clamped); none stored means uniform 0, 1, ..., n - degree.
namespace voelund::kernel::detail {

struct OcctKnots final {
    NCollection_Array1<double> values;
    NCollection_Array1<int> multiplicities;
};

inline std::vector<double> flatKnots(std::size_t poles, int degree,
                                     std::span<const double> stored) {
    if (!stored.empty())
        return {stored.begin(), stored.end()};
    const int spans = static_cast<int>(poles) - degree;
    std::vector<double> flat(static_cast<std::size_t>(degree) + 1, 0.0);
    for (int knot = 1; knot < spans; ++knot)
        flat.push_back(knot);
    flat.insert(flat.end(), static_cast<std::size_t>(degree) + 1, static_cast<double>(spans));
    return flat;
}

inline OcctKnots occtKnots(std::span<const double> flat) {
    std::vector<double> values;
    std::vector<int> counts;
    for (const double knot : flat) {
        if (!values.empty() && knot == values.back())
            ++counts.back();
        else {
            values.push_back(knot);
            counts.push_back(1);
        }
    }
    const int size = static_cast<int>(values.size());
    OcctKnots result{NCollection_Array1<double>(1, size), NCollection_Array1<int>(1, size)};
    for (int index = 0; index < size; ++index) {
        result.values.SetValue(index + 1, values[static_cast<std::size_t>(index)]);
        result.multiplicities.SetValue(index + 1, counts[static_cast<std::size_t>(index)]);
    }
    return result;
}

} // namespace voelund::kernel::detail
