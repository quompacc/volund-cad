// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <cstdint>
#include <optional>
#include <utility>
#include <variant>
#include <vector>
#include <voelund/document/EdgeReference.hpp>
#include <voelund/document/ParameterId.hpp>
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
    // N14c2 (schema 55): flat, clamped knots (each repeated by its multiplicity, poles + 4 of
    // them) of a trimmed or split spline; empty for the uniform knots above.
    std::vector<double> knots;

    [[nodiscard]] bool operator==(const SketchBSpline&) const = default;
};
// N14c2: the spline's flat knots, the stored ones or the uniform 0 (four times), 1, ...,
// poles - 3 (four times).
[[nodiscard]] std::vector<double> bsplineKnots(const SketchBSpline& spline);

// N14c (schema 54, agent decision 2026-10-29): a piece of an ellipse, made by trimming or
// splitting one. The parameters are eccentric anomalies from the major axis (the point is
// center + a (cos t) major + b (sin t) minor, as in OCCT and PlaneGCS), counterclockwise from
// start to end; 0 < end - start < 2 pi.
struct SketchEllipticArc final {
    SketchPoint center;
    double majorRadius;
    double minorRadius;
    double rotation; // radians
    double startParameter;
    double endParameter;

    [[nodiscard]] SketchEllipse ellipse() const noexcept {
        return {center, majorRadius, minorRadius, rotation};
    }
    [[nodiscard]] bool operator==(const SketchEllipticArc&) const = default;
};

// The point of an ellipse at the eccentric anomaly `parameter`.
[[nodiscard]] SketchPoint ellipsePoint(const SketchEllipse& ellipse, double parameter) noexcept;
// The eccentric anomaly in (-pi, pi] of the ellipse point on the ray from the center through
// `point` scaled to the ellipse (exact for points on it).
[[nodiscard]] double ellipseParameter(const SketchEllipse& ellipse, SketchPoint point) noexcept;

using SketchGeometry =
    std::variant<SketchLine, SketchRectangle, SketchCircle, SketchArc, SketchEllipse,
                 SketchPointGeometry, SketchBSpline, SketchEllipticArc>;

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

// The point a relation names on an element: the ends of a line or B-spline, the center of a
// circle, ellipse or point, center and ends of an arc or elliptic arc; none for any other
// position and for a legacy rectangle (N14c: one place for every curve kind).
[[nodiscard]] std::optional<SketchPoint> sketchReferencePoint(const SketchGeometry& geometry,
                                                              SketchPointPosition point);
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
    // W21 (schema 41, as SolidCraft): where the user dragged the dimension line, the signed
    // distance from the middle of what it measures across it (linear), its radius about the
    // vertex (angle, mm) or the angle of its line about the centre (radius and diameter,
    // radians); unset keeps the automatic place. Display only, the solve ignores it.
    std::optional<double> dimensionPosition{};

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
