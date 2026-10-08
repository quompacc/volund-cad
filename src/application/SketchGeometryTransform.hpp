#pragma once

#include <voelund/document/SketchGeometry.hpp>

#include <cstddef>
#include <vector>

// S08: rigid transforms of sketch geometry (translate, rotate, mirror) for every geometry
// variant. A transform that a variant cannot represent is not silently approximated: a legacy
// rectangle turned off its axes becomes four lines held by constraints.
namespace voelund::application::detail {

// p -> (xx * x + xy * y + tx, yx * x + yy * y + ty); the linear part is orthogonal.
struct PlanarTransform final {
    double xx{1.0}, xy{0.0}, yx{0.0}, yy{1.0}, tx{0.0}, ty{0.0};

    [[nodiscard]] static PlanarTransform translation(double dx, double dy);
    [[nodiscard]] static PlanarTransform rotation(double angle); // about the sketch origin
    [[nodiscard]] static PlanarTransform reflection(const document::SketchLine& axis);
    [[nodiscard]] document::SketchPoint apply(document::SketchPoint point) const noexcept;
    [[nodiscard]] bool mirrors() const noexcept;
    [[nodiscard]] bool translatesOnly() const noexcept;
};

// A constraint between two elements of one transformed copy, by their index in `elements`.
struct CopyConstraint final {
    document::SketchConstraintKind kind;
    std::size_t first;
    document::SketchPointPosition firstPoint;
    std::size_t second;
    document::SketchPointPosition secondPoint;
};

struct TransformedCopy final {
    std::vector<document::SketchGeometry> elements;
    std::vector<CopyConstraint> constraints;
};

// The image of `geometry`. Horizontal/vertical line marks follow the transform when it keeps
// the sketch axes and are dropped otherwise.
[[nodiscard]] TransformedCopy transformedCopy(const document::SketchGeometry& geometry,
                                              const PlanarTransform& transform);

} // namespace voelund::application::detail
