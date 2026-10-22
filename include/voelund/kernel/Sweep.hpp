// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/kernel/OcctCompatibility.hpp>

#include <TopoDS_Shape.hxx>

#include <memory>
#include <vector>

namespace voelund::kernel {

// M23: a planar profile (a face or a compound of faces) swept along a path (a connected wire,
// open or closed) with a corrected Frenet frame; the profile stays where it is and moves with
// the path from its start. startOf/endOf map a profile face to its first and last cap (none on
// a closed path), generated maps a profile edge to the side faces it sweeps. Throws KernelError
// for an empty profile or path, a failed sweep, or a result without valid volume.
class Sweep final {
  public:
    Sweep(const TopoDS_Shape& profile, const TopoDS_Shape& path);
    ~Sweep();
    Sweep(Sweep&&) noexcept;
    Sweep& operator=(Sweep&&) noexcept;
    Sweep(const Sweep&) = delete;
    Sweep& operator=(const Sweep&) = delete;

    [[nodiscard]] const TopoDS_Shape& shape() const noexcept;
    [[nodiscard]] bool closed() const noexcept;
    [[nodiscard]] TopoDS_Shape startOf(const TopoDS_Shape& profileFace) const;
    [[nodiscard]] TopoDS_Shape endOf(const TopoDS_Shape& profileFace) const;
    [[nodiscard]] std::vector<TopoDS_Shape> generated(const TopoDS_Shape& profileEdge) const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace voelund::kernel
