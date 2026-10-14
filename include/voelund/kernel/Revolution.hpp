#pragma once

#include <voelund/kernel/OcctCompatibility.hpp>

#include <TopoDS_Shape.hxx>

#include <array>
#include <memory>
#include <vector>

namespace voelund::kernel {

// M20: a planar profile (a face or a compound of faces) turned about an axis by an angle in
// (0, 2 pi]; the counterpart of Prism with the same history. A full turn has no start or end
// face (startOf/endOf are null then). Throws KernelError for an empty profile, an axis that is
// not a unit vector, an angle out of range, a failed turn, or a result that is invalid or has
// no solid (e.g. when the axis crosses the profile).
class Revolution final {
  public:
    Revolution(const TopoDS_Shape& profile, const std::array<double, 3>& axisOrigin,
               const std::array<double, 3>& axisDirection, double angle);
    ~Revolution();
    Revolution(Revolution&&) noexcept;
    Revolution& operator=(Revolution&&) noexcept;
    Revolution(const Revolution&) = delete;
    Revolution& operator=(const Revolution&) = delete;

    [[nodiscard]] const TopoDS_Shape& shape() const noexcept;
    [[nodiscard]] bool full() const noexcept;
    [[nodiscard]] TopoDS_Shape startOf(const TopoDS_Shape& profileShape) const;
    [[nodiscard]] TopoDS_Shape endOf(const TopoDS_Shape& profileShape) const;
    [[nodiscard]] std::vector<TopoDS_Shape> generated(const TopoDS_Shape& profileShape) const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace voelund::kernel
