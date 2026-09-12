#pragma once

#include <cstdint>
#include <variant>
#include <vector>

namespace voelund::document {

class SketchElementId final {
  public:
    constexpr explicit SketchElementId(std::uint64_t value) noexcept : value_(value) {}

    [[nodiscard]] constexpr std::uint64_t value() const noexcept { return value_; }
    [[nodiscard]] constexpr bool operator==(const SketchElementId&) const noexcept = default;

  private:
    std::uint64_t value_;
};

struct SketchPoint final {
    double x;
    double y;

    [[nodiscard]] bool operator==(const SketchPoint&) const = default;
};

enum class SketchLineConstraint { none, horizontal, vertical };

struct SketchLine final {
    SketchPoint start;
    SketchPoint end;
    SketchLineConstraint constraint{SketchLineConstraint::none};

    [[nodiscard]] bool operator==(const SketchLine&) const = default;
};

struct SketchRectangle final {
    SketchPoint first;
    SketchPoint second;

    constexpr SketchRectangle() noexcept = default;
    constexpr SketchRectangle(double firstX, double firstY, double secondX, double secondY) noexcept
        : first{firstX, firstY}, second{secondX, secondY} {}

    [[nodiscard]] bool operator==(const SketchRectangle&) const = default;
};

struct SketchCircle final {
    SketchPoint center;
    double radius;

    [[nodiscard]] bool operator==(const SketchCircle&) const = default;
};

using SketchGeometry = std::variant<SketchLine, SketchRectangle, SketchCircle>;

struct SketchElement final {
    SketchElementId id;
    SketchGeometry geometry;

    [[nodiscard]] bool operator==(const SketchElement&) const = default;
};

struct SketchElements final {
    std::vector<SketchElement> values;
    std::uint64_t nextId{1};

    [[nodiscard]] bool operator==(const SketchElements&) const = default;
};

void validateSketchElements(const SketchElements& elements);

} // namespace voelund::document
