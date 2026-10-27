// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <TopoDS_Shape.hxx>

#include <cstddef>
#include <span>
#include <vector>

namespace voelund::kernel {

// N08: where two of the given bodies overlap: their indices, the common volume (mm³) and the
// common solid. Bodies that only touch (a shared face or edge, no volume) do not interfere.
struct Interference final {
    std::size_t first;
    std::size_t second;
    double volume;
    TopoDS_Shape common;
};
[[nodiscard]] std::vector<Interference> findInterferences(std::span<const TopoDS_Shape> bodies);

} // namespace voelund::kernel
