#pragma once

#include <voelund/document/ModelDocument.hpp>

#include <gp_Pnt.hxx>

#include <optional>
#include <string>
#include <vector>

namespace voelund::features {

// S07: a geometric relation shows a symbol, a dimension its value.
enum class SketchAnnotationCategory { relation, dimension };

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
    std::string label{}; // sketchConstraintLabel(kind, value)
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

} // namespace voelund::features
