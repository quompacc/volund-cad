// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/document/ModelDocument.hpp>

#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

#include <optional>
#include <string>
#include <vector>

namespace voelund::features {

// S07: a geometric relation shows a symbol, a dimension its value.
enum class SketchAnnotationCategory { relation, dimension };

// W12 (as in Fusion and FreeCAD): a dimension drawn as in a technical drawing, in world
// coordinates. Linear: extension lines from `first` and `second`, the dimension line along
// `along`, `offset` beyond the farther point on the `outward` side. Radius: from the centre
// `first` to `second` on the curve. Diameter: across from `first` to `second`. Angle: an arc of
// radius `offset` about `vertex` from the leg through `first` to the leg through `second`.
enum class SketchDimensionStyle { linear, radius, diameter, angle };
struct SketchDimensionShape final {
    SketchDimensionStyle style;
    gp_Pnt first;
    gp_Pnt second;
    gp_Pnt vertex;
    gp_Dir along;
    gp_Dir outward;
    gp_Dir normal; // the sketch plane's
    double offset;
    std::string text; // "80 mm", "R 5 mm", "Ø 10 mm", "60°"; a reference in parentheses
    // An upright dimension line (along the sketch's Y) has its value beside it: +1 right,
    // -1 left (the outer side); 0 above the line.
    int textBeside{0};
};

// W18 (as SolidCraft): where a relation's symbol sits on the geometry, the direction along the
// curve there (world), and the elements it binds (highlighted when the symbol is hovered).
// Relations between two curves have one anchor on each.
struct SketchGlyphAnchor final {
    gp_Pnt at;
    gp_Dir along;
    std::vector<document::SketchElementId> elements;
};

struct SketchConstraintAnnotation final {
    document::SketchConstraintId id;
    document::SketchConstraintKind kind;
    gp_Pnt position;
    // Internal units: radians for angles, millimeters for lengths and coordinates; 0 for
    // relations.
    double value;
    SketchAnnotationCategory category{SketchAnnotationCategory::relation};
    // A dimension the editor may change (SketchEditorSession::editDimension); references to
    // projected or support edges and parameter-bound dimensions are not.
    bool editable{false};
    std::optional<document::ParameterId> parameter{};
    std::string label{};                             // sketchConstraintLabel(kind, value)
    std::optional<SketchDimensionShape> dimension{}; // W12: dimensions only
    std::vector<SketchGlyphAnchor> glyphs{};         // W18: relations only
};

[[nodiscard]] SketchAnnotationCategory
sketchAnnotationCategory(document::SketchConstraintKind kind) noexcept;
// A dimension whose value belongs to the sketch: not bound to a parameter, not following a
// projected or support edge.
[[nodiscard]] bool isEditableDimension(const document::SketchConstraint& constraint) noexcept;
// "[=]" for a relation; "L 12.000 mm", "R 5.000 mm", "Ø 10.000 mm", "A 60.000°" ... for a
// dimension (value in internal units).
[[nodiscard]] std::string sketchConstraintLabel(document::SketchConstraintKind kind, double value);

// One annotation per constraint, placed on the geometry it binds, from the given (solved)
// geometry.
[[nodiscard]] std::vector<SketchConstraintAnnotation>
evaluateSketchConstraintAnnotations(const document::SketchParameters& sketch);
// W21 (SketchDimensionPlacement.cpp): the dimensionPosition that puts a dimension's line through
// the pointer: across a linear one, the arc of an angle, the direction of a radius or diameter.
[[nodiscard]] std::optional<double> sketchDimensionPositionAt(document::SketchParameters sketch,
                                                              document::SketchConstraintId id,
                                                              const document::SketchPoint& pointer);
// W18: the symbol anchors of a relation (none for a dimension); `annotationPosition` is where a
// relation of points sits.
[[nodiscard]] std::vector<SketchGlyphAnchor>
sketchGlyphAnchors(const document::SketchParameters& sketch,
                   const document::SketchConstraint& constraint, const gp_Pnt& annotationPosition);

} // namespace voelund::features
