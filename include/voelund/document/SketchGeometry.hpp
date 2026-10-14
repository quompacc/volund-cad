#pragma once

#include <cstdint>
#include <optional>
#include <utility>
#include <variant>
#include <vector>
#include <voelund/document/ParameterId.hpp>
#include <voelund/document/EdgeReference.hpp>
#include <voelund/kernel/Angle.hpp>

namespace voelund::document {

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

struct SketchArc final {
    SketchPoint center;
    double radius;
    // Geometry angles are radians; they are not legacy degree-valued constraints.
    double startAngle;
    double endAngle;

    [[nodiscard]] bool operator==(const SketchArc&) const = default;
};

struct SketchEllipse final {
    SketchPoint center;
    double majorRadius;
    double minorRadius;
    double rotation; // radians

    [[nodiscard]] bool operator==(const SketchEllipse&) const = default;
};

// S15 (schema 22): a single point, referenced as its center. It never bounds a region: a
// point element is always construction geometry.
struct SketchPointGeometry final {
    SketchPoint position;

    [[nodiscard]] bool operator==(const SketchPointGeometry&) const = default;
};

// S21 (schema 30, user decision 2026-10-14): an open, clamped, uniform cubic B-spline over its
// control points (4 to 64); it starts at the first and ends at the last one.
inline constexpr std::size_t minimumBSplinePoles = 4;
inline constexpr std::size_t maximumBSplinePoles = 64;
struct SketchBSpline final {
    std::vector<SketchPoint> poles;

    [[nodiscard]] bool operator==(const SketchBSpline&) const = default;
};

using SketchGeometry = std::variant<SketchLine, SketchRectangle, SketchCircle, SketchArc,
                                    SketchEllipse, SketchPointGeometry, SketchBSpline>;

struct SketchElement final {
    SketchElementId id;
    SketchGeometry geometry;
    bool construction{false};

    // S15: a point is always construction geometry.
    SketchElement(SketchElementId elementId, SketchGeometry elementGeometry,
                  bool isConstruction = false) noexcept
        : id(elementId), geometry(std::move(elementGeometry)),
          construction(isConstruction || std::holds_alternative<SketchPointGeometry>(geometry)) {}

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
    perpendicularBisector,
    concentric,
    diameter,
    collinear,
    horizontalPoints,
    verticalPoints,
    fixed,
    pointLineDistance
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
    // Non-angular dimensions/coordinates only; angles use angleValue.
    double value{0.0};
    kernel::Radians angleValue{};
    std::optional<SketchGeometryReference> third;
    std::vector<double> auxiliaryValues;
    std::optional<ProjectedEdgeReference> projectedEdge{};
    // PAR01: the value mirrors this document parameter while bound.
    std::optional<ParameterId> parameter{};
    // S18 (schema 24): a reference dimension determines nothing; the solve stores what the
    // geometry measures. Dimensions only, never bound to a parameter.
    bool reference{false};

    SketchConstraint(SketchConstraintId constraintId, SketchConstraintKind constraintKind,
                     SketchGeometryReference firstReference,
                     std::optional<SketchGeometryReference> secondReference,
                     double drivingValue = 0.0,
                     std::optional<SketchGeometryReference> thirdReference = std::nullopt,
                     std::vector<double> extraValues = {})
        : id(constraintId), kind(constraintKind), first(firstReference), second(secondReference),
          value(drivingValue), third(thirdReference), auxiliaryValues(std::move(extraValues)) {
        if (kind == SketchConstraintKind::angle && drivingValue != 0.0)
            throw std::invalid_argument("Angle constraint requires typed radians");
    }

    SketchConstraint(SketchConstraintId constraintId, SketchConstraintKind constraintKind,
                     SketchGeometryReference firstReference,
                     std::optional<SketchGeometryReference> secondReference, kernel::Radians angle)
        : SketchConstraint(constraintId, constraintKind, firstReference, secondReference) {
        if (kind != SketchConstraintKind::angle)
            throw std::invalid_argument("Radians require an angle constraint");
        angleValue = angle;
    }

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
