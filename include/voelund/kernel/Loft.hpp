// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/kernel/OcctCompatibility.hpp>

#include <TopoDS_Shape.hxx>

#include <memory>
#include <vector>

namespace voelund::kernel {

// M24: a solid through closed section wires, in order, ruled (straight between sections) or
// smooth. startFace/endFace are the caps on the first and last section, generated maps an edge
// of the first section to the side face it starts. Throws KernelError for fewer than two
// sections, a failed loft, or a result without valid volume.
class Loft final {
  public:
    Loft(const std::vector<TopoDS_Shape>& sections, bool ruled);
    ~Loft();
    Loft(Loft&&) noexcept;
    Loft& operator=(Loft&&) noexcept;
    Loft(const Loft&) = delete;
    Loft& operator=(const Loft&) = delete;

    [[nodiscard]] const TopoDS_Shape& shape() const noexcept;
    [[nodiscard]] TopoDS_Shape startFace() const;
    [[nodiscard]] TopoDS_Shape endFace() const;
    [[nodiscard]] TopoDS_Shape generated(const TopoDS_Shape& firstSectionEdge) const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace voelund::kernel
