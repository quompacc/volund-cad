#include "PrimitivePlacement.hpp"
#include <voelund/features/CylinderFeature.hpp>
#include <voelund/features/GeometryEvaluationError.hpp>

#include <voelund/document/DocumentSnapshot.hpp>
#include <voelund/kernel/ShapeFactory.hpp>

#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>

#include <algorithm>
#include <stdexcept>

namespace voelund::features {

CylinderFeature::CylinderFeature(document::FeatureId featureId) noexcept : featureId_(featureId) {}

TopoDS_Shape CylinderFeature::evaluateTool(const document::ModelDocument& document,
                                           const TopoDS_Shape& inputShape,
                                           bool applyPlacement) const {
    return evaluateTool(document::DocumentSnapshot::capture(document), inputShape, applyPlacement);
}

TopoDS_Shape CylinderFeature::evaluateTool(const document::DocumentSnapshot& document,
                                           const TopoDS_Shape& inputShape,
                                           bool applyPlacement) const {
    return evaluateToolWithHistory(document, inputShape, applyPlacement).shape;
}

kernel::ShapeOperationResult
CylinderFeature::evaluateToolWithHistory(const document::DocumentSnapshot& document,
                                         const TopoDS_Shape& inputShape,
                                         bool applyPlacement) const {
    const document::CylinderParameters& cylinder = document.cylinderParameters(featureId_);
    const gp_Pnt origin = attachmentOrigin(inputShape, cylinder.height);
    const kernel::CylinderDefinition definition{cylinder.radius, cylinder.height, origin.X(),
                                                origin.Y(), origin.Z()};
    const auto cylinderShape = kernel::ShapeFactory::makeCylinderWithHistory(definition);
    if (!applyPlacement) {
        return cylinderShape;
    }
    return detail::placePrimitive(cylinderShape, origin.X(), origin.Y(),
                                  origin.Z() + cylinder.height * 0.5, cylinder.placement);
}

gp_Pnt CylinderFeature::attachmentOrigin(const TopoDS_Shape& inputShape, double cylinderHeight) {
    Bnd_Box bounds;
    BRepBndLib::AddOptimal(inputShape, bounds, false, false);
    if (bounds.IsVoid()) {
        throw GeometryEvaluationError("Cylinder input feature has no geometry");
    }
    double minimumX = 0.0;
    double minimumY = 0.0;
    [[maybe_unused]] double minimumZ = 0.0;
    double maximumX = 0.0;
    double maximumY = 0.0;
    double maximumZ = 0.0;
    bounds.Get(minimumX, minimumY, minimumZ, maximumX, maximumY, maximumZ);
    const double overlap = std::min(2.0, cylinderHeight * 0.25);
    const double originX = (minimumX + maximumX) * 0.5;
    const double originY = (minimumY + maximumY) * 0.5;
    const double originZ = maximumZ - overlap;
    return gp_Pnt(originX, originY, originZ);
}

} // namespace voelund::features
