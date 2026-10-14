#pragma once

#include <voelund/kernel/OcctCompatibility.hpp>

#include <TopoDS_Shape.hxx>

#include <array>
#include <memory>
#include <vector>

namespace voelund::kernel {

// M26: a planar profile face swept along a helix about an axis: it turns by `angle` (radians,
// more than one turn allowed) and advances `pitch` along the axis per turn; left-handed turns the
// other way. The helix lies on the cylinder through the profile's centre, the profile keeps its
// attitude to the axis (binormal mode). startOf/endOf/generated as for the other sweeps. Throws
// KernelError for a profile with holes or more than one face, a profile on the axis, invalid
// values, a failed sweep, or a result without valid volume.
class Helix final {
  public:
    Helix(const TopoDS_Shape& profileFace, const std::array<double, 3>& axisOrigin,
          const std::array<double, 3>& axisDirection, double pitch, double angle,
          bool leftHanded);
    ~Helix();
    Helix(Helix&&) noexcept;
    Helix& operator=(Helix&&) noexcept;
    Helix(const Helix&) = delete;
    Helix& operator=(const Helix&) = delete;

    [[nodiscard]] const TopoDS_Shape& shape() const noexcept;
    [[nodiscard]] TopoDS_Shape startFace() const;
    [[nodiscard]] TopoDS_Shape endFace() const;
    [[nodiscard]] std::vector<TopoDS_Shape> generated(const TopoDS_Shape& profileEdge) const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace voelund::kernel
