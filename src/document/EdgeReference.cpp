#include <cmath>
#include <stdexcept>
#include <voelund/document/EdgeReference.hpp>
#include <voelund/document/IdCapacity.hpp>

namespace voelund::document {
void validateProjectedEdgeReference(const ProjectedEdgeReference& ref) {
    const auto require = [](bool ok) {
        if (!ok)
            throw std::invalid_argument("Invalid projected edge reference");
    };
    // Roundtrip through the canonical representation also validates the namespace.
    require(documentIdFromHex(documentIdToHex(ref.document)) == ref.document);
    require(hasIdCapacity(ref.body.value(), 1));
    require(std::isfinite(ref.selection.parameter) && ref.selection.parameter >= 0 &&
            ref.selection.parameter <= 1);
    require(ref.selection.kind == EdgePointKind::start ||
            ref.selection.kind == EdgePointKind::end ||
            ref.selection.kind == EdgePointKind::parameter);
    require(ref.selection.kind == EdgePointKind::parameter || ref.selection.parameter == 0);
    if (const auto* primitive = std::get_if<PrimitiveEdgeOrigin>(&ref.origin.value)) {
        require(hasIdCapacity(primitive->feature.value(), 1));
        const int a = static_cast<int>(primitive->first), b = static_cast<int>(primitive->second);
        const bool box = a >= 0 && a < b && b <= 5 && a / 2 != b / 2;
        const bool cylinder = (a == 6 || a == 7) && b == 8;
        require(box || cylinder);
    } else {
        const auto& contour = std::get<ContourEdgeOrigin>(ref.origin.value);
        require(hasIdCapacity(contour.sketch.value(), 1) &&
                hasIdCapacity(contour.element.value(), 1));
        require(contour.role >= ContourRole::line && contour.role <= ContourRole::rectangleFourth);
        require(contour.stage == ContourStage::sketch ||
                contour.stage == ContourStage::extrusionStart ||
                contour.stage == ContourStage::extrusionEnd);
        require((contour.stage == ContourStage::sketch) == !contour.extrusion.has_value());
        if (contour.extrusion)
            require(hasIdCapacity(contour.extrusion->value(), 1));
    }
}
} // namespace voelund::document
