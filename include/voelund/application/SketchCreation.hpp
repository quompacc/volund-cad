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
enum class SketchSnapKind { start, end, center, midpoint, origin, xAxis, yAxis, projected, onObject };
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
    polygonSide
};
struct SketchCreatedElement final {
    document::SketchElementId id;
    SketchCreationRole role;
    bool operator==(const SketchCreatedElement&) const = default;
};
struct SketchCreationResult final {
    std::vector<SketchCreatedElement> elements;
    [[nodiscard]] document::SketchElementId primary() const { return elements.front().id; }
    [[nodiscard]] std::optional<document::SketchElementId> find(SketchCreationRole role) const {
        for (const auto& element : elements)
            if (element.role == role)
                return element.id;
        return std::nullopt;
    }
    bool operator==(const SketchCreationResult&) const = default;
};

} // namespace voelund::application
