// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/kernel/OcctCompatibility.hpp>

#include <TopoDS_Shape.hxx>

#include <memory>
#include <vector>

namespace voelund::kernel {

// M02: the kernel's only prism. A planar profile (a face or a compound of faces) is swept
// from where it lies along a unit direction by a positive length: no signed distances, no
// padding. Throws KernelError for an empty profile, a direction that is not a unit vector, a
// length outside the model range, a failed sweep, or a result that is invalid or has no
// solid. The sweep keeps its history: caps per profile face, generated faces per edge.
class Prism final {
  public:
    Prism(const TopoDS_Shape& profile, double directionX, double directionY, double directionZ,
          double length);
    ~Prism();
    Prism(Prism&&) noexcept;
    Prism& operator=(Prism&&) noexcept;
    Prism(const Prism&) = delete;
    Prism& operator=(const Prism&) = delete;

    [[nodiscard]] const TopoDS_Shape& shape() const noexcept;
    // The copy of a profile face (or edge) at the start / end of the sweep.
    [[nodiscard]] TopoDS_Shape startOf(const TopoDS_Shape& profileShape) const;
    [[nodiscard]] TopoDS_Shape endOf(const TopoDS_Shape& profileShape) const;
    // The faces an edge (or the edges a vertex) of the profile sweeps into.
    [[nodiscard]] std::vector<TopoDS_Shape> generated(const TopoDS_Shape& profileShape) const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace voelund::kernel
