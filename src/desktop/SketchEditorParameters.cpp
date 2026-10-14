#include "SketchEditorToolController.hpp"

#include "ToolPanelHost.hpp"

#include <cmath>

// A value typed into the sketch panel: tool sizes are kept for the next command, element
// dimensions and translations apply at once.
namespace voelund::desktop {

void SketchEditorToolController::applyParameter(const std::string& id, double value) {
    if (id == "translation_x" || id == "translation_y")
        applyTranslation(id, value);
    else if (id == "polygon_sides")
        static_cast<void>(editor_->setPolygonSides(static_cast<int>(std::lround(value))));
    else if (id == "fillet_radius")
        filletRadius_ = value;
    else if (id == "chamfer_distance")
        chamferDistance_ = value;
    else if (id == "transform_center_x") // S20
        transformCenter_.x = value;
    else if (id == "transform_center_y")
        transformCenter_.y = value;
    else if (id == "rotate_angle")
        rotateAngle_ = value;
    else if (id == "scale_factor")
        scaleFactor_ = value;
    else if (id == "offset_distance") {
        offsetDistance_ = value;
        // S09: the preview is the geometry "Versetzen" adds.
        static_cast<void>(editor_->previewOffset(value));
        refreshPresentation();
    } else if (id == "pattern_count")
        patternCount_ = value;
    else if (id == "pattern_spacing")
        patternSpacing_ = value;
    else if (id == "pattern_angle")
        patternAngle_ = value;
    else
        applyElementDimension(id, value);
}

} // namespace voelund::desktop
