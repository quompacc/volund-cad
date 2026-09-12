#pragma once

#include <cstdint>
#include <optional>
#include <utility>
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

class SketchConstraintId final {
  public:
    constexpr explicit SketchConstraintId(std::uint64_t value) noexcept : value_(value) {}

    [[nodiscard]] constexpr std::uint64_t value() const noexcept { return value_; }
    [[nodiscard]] constexpr bool operator==(const SketchConstraintId&) const noexcept = default;

  private:
    std::uint64_t value_;
};

class SketchProfileId final {
  public:
    constexpr explicit SketchProfileId(std::uint64_t value) noexcept : value_(value) {}

    [[nodiscard]] constexpr std::uint64_t value() const noexcept { return value_; }
    [[nodiscard]] constexpr bool operator==(const SketchProfileId&) const noexcept = default;

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
    bool construction{false};

    SketchElement(SketchElementId elementId, SketchGeometry elementGeometry,
                  bool isConstruction = false) noexcept
        : id(elementId), geometry(std::move(elementGeometry)), construction(isConstruction) {}

    [[nodiscard]] bool operator==(const SketchElement&) const = default;
};

struct SketchElements final {
    std::vector<SketchElement> values;
    std::uint64_t nextId{1};

    [[nodiscard]] bool operator==(const SketchElements&) const = default;
};

enum class SketchPointPosition { start, end, center };
enum class SketchConstraintKind {
    horizontal,
    vertical,
    distance,
    radius,
    coincident,
    parallel,
    perpendicular,
    coordinateX,
    coordinateY,
    pointOnObject,
    equal,
    tangent,
    symmetric,
    pointDistance,
    angle,
    pointDistanceX,
    pointDistanceY,
    midpoint,
    perpendicularBisector
};

struct SketchGeometryReference final {
    SketchElementId element;
    SketchPointPosition point{SketchPointPosition::start};

    [[nodiscard]] bool operator==(const SketchGeometryReference&) const = default;
};

struct SketchConstraint final {
    SketchConstraintId id;
    SketchConstraintKind kind;
    SketchGeometryReference first;
    std::optional<SketchGeometryReference> second;
    double value{0.0};
    std::optional<SketchGeometryReference> third;

    SketchConstraint(SketchConstraintId constraintId, SketchConstraintKind constraintKind,
                     SketchGeometryReference firstReference,
                     std::optional<SketchGeometryReference> secondReference,
                     double drivingValue = 0.0,
                     std::optional<SketchGeometryReference> thirdReference = std::nullopt) noexcept
        : id(constraintId), kind(constraintKind), first(firstReference), second(secondReference),
          value(drivingValue), third(thirdReference) {}

    [[nodiscard]] bool operator==(const SketchConstraint&) const = default;
};

struct SketchConstraints final {
    std::vector<SketchConstraint> values;
    std::uint64_t nextId{1};

    [[nodiscard]] bool operator==(const SketchConstraints&) const = default;
};

void validateSketchElements(const SketchElements& elements);
void validateSketchConstraints(const SketchElements& elements,
                               const SketchConstraints& constraints);

} // namespace voelund::document
