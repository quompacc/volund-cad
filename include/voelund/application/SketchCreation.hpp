#pragma once
#include <voelund/document/EdgeReference.hpp>
#include <voelund/document/SketchGeometry.hpp>

#include <cstdint>
#include <optional>
#include <vector>

// U08: one way to create sketch elements. The interactive editor and any other caller hand
// the points with their snaps to SketchEditorSession::create; the session decides the
// automatic relations and reports what it created by role, so no caller derives element IDs
// from insertion order.
namespace voelund::application {

// What a picked point snapped to.
enum class SketchSnapKind {
    start,
    end,
    center,
    midpoint,
    origin,
    xAxis,
    yAxis,
    projected,
    onObject
};
struct SketchSnap final {
    SketchSnapKind kind;
    document::SketchElementId element{0}; // 0 for origin, axes and support edges
    std::uint64_t supportReference{0};
    double supportParameter{0.0};
    std::optional<document::ProjectedEdgeReference> projectedEdge{};
};

// The relation the pointer suggested while drawing (preview feedback).
enum class SketchAutoRelation {
    horizontal,
    vertical,
    parallel,
    perpendicular,
    pointOnObject,
    tangent,
    equal,
    concentric
};

struct SketchCreationInput final {
    document::SketchPoint first;
    document::SketchPoint second;
    std::optional<document::SketchPoint> third{};
    std::optional<SketchSnap> firstSnap{};
    std::optional<SketchSnap> secondSnap{};
    std::optional<SketchSnap> thirdSnap{};
    std::optional<SketchAutoRelation> relation{};
    std::optional<document::SketchElementId> relationTarget{};
    // Rectangle tool only (S04a): `first` is the center, `second` a corner.
    bool centered{false};
    // S04: false adds no relation of its own (no suggested and no inferred parallel or
    // perpendicular); snaps the caller passes still bind.
    bool automaticRelations{true};
};

enum class SketchCreationRole {
    element, // the single element of a line, circle, arc or ellipse
    // Rectangle edges in contour order: edge 1 starts at the first corner, edge 3 at the
    // opposite one.
    rectangleFromFirstCorner,
    rectangleToSecondCorner,
    rectangleFromSecondCorner,
    rectangleToFirstCorner,
    // Centered rectangle: construction diagonal and center-to-corner line.
    centerDiagonal,
    centerHalfDiagonal,
    // Slot: side from the first center, cap around the second center, side back, cap around
    // the first center.
    slotFirstSide,
    slotSecondCap,
    slotSecondSide,
    slotFirstCap,
    // S06: construction axis from the first to the second cap center; its length is the
    // slot's center distance, a horizontal/vertical relation on it orients the slot.
    slotAxis,
    polygonSide, // `index` counts the sides from the picked vertex, counterclockwise
    // S06: construction circle through all vertices; its radius is the polygon's
    // circumradius, its center the polygon's center.
    polygonCircle
};
struct SketchCreatedElement final {
    document::SketchElementId id;
    SketchCreationRole role;
    std::uint32_t index{0};
    bool operator==(const SketchCreatedElement&) const = default;
};

// S01: the points of a step by meaning. They follow the picks, not the order in which the
// elements or their endpoints are stored: a clockwise three-point arc is stored
// counterclockwise, so its arcStart is the stored end.
enum class SketchCreationPoint {
    lineStart, // line and polyline segment, in drawing order
    lineEnd,
    center,   // circle, arc, three-point arc and ellipse
    arcStart, // the arc's first and last picked end
    arcEnd,
    rectangleFirstCorner,
    rectangleSecondCorner,
    rectangleCenter, // centered rectangle only
    slotFirstCenter, // cap centers, at the first and second pick
    slotSecondCenter,
    polygonVertex, // the picked vertex
    polygonCenter  // the center of the construction circle (S06)
};
struct SketchCreatedPoint final {
    SketchCreationPoint role;
    document::SketchGeometryReference reference;
    bool operator==(const SketchCreatedPoint&) const = default;
};
enum class SketchArcDirection { counterclockwise, clockwise };

// Why a constraint of the step exists: the tool's own shape (rectangle corners, slot
// tangencies, polygon sides), a snapped pick, or the suggested relation.
//
// S06: what the shape relations keep under any dimension change:
// - Regular polygon: consecutive sides joined, all sides equal, every vertex on the
//   construction circle. Equal sides alone would allow a rhombus; with the vertices on one
//   circle the polygon stays regular. Free: center, circumradius, rotation (4 DOF).
// - Slot: sides and caps joined, caps equal, each side tangent to both caps, the axis ends on
//   the cap centers. Free: both centers and the radius (5 DOF); width = 2 x cap radius,
//   length = center distance (axis length).
enum class SketchConstraintOrigin { shape, snap, relation };
struct SketchCreatedConstraint final {
    document::SketchConstraintId id;
    SketchConstraintOrigin origin;
    bool operator==(const SketchCreatedConstraint&) const = default;
};

struct SketchCreationResult final {
    std::vector<SketchCreatedElement> elements{};
    std::vector<SketchCreatedPoint> points{};
    std::vector<SketchCreatedConstraint> constraints{};
    // Arcs: the direction from arcStart to arcEnd.
    std::optional<SketchArcDirection> arcDirection{};
    [[nodiscard]] document::SketchElementId primary() const { return elements.front().id; }
    [[nodiscard]] std::optional<document::SketchElementId> find(SketchCreationRole role,
                                                                std::uint32_t index = 0) const {
        for (const auto& element : elements)
            if (element.role == role && element.index == index)
                return element.id;
        return std::nullopt;
    }
    [[nodiscard]] std::optional<document::SketchGeometryReference>
    point(SketchCreationPoint role) const {
        for (const auto& item : points)
            if (item.role == role)
                return item.reference;
        return std::nullopt;
    }
    bool operator==(const SketchCreationResult&) const = default;
};

} // namespace voelund::application
