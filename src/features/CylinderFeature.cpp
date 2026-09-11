#include <voelund/features/CylinderFeature.hpp>

#include <voelund/document/ModelDocument.hpp>
#include <voelund/kernel/ShapeFactory.hpp>

#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>

#include <algorithm>
#include <stdexcept>

namespace voelund::features {

CylinderFeature::CylinderFeature(document::FeatureId featureId) noexcept
    : featureId_(featureId)
{
}

TopoDS_Shape CylinderFeature::evaluateTool(
    const document::ModelDocument& document,
    const TopoDS_Shape& inputShape,
    bool applyPlacement) const
{
    const document::CylinderParameters& cylinder = document.cylinderParameters(featureId_);
    const gp_Pnt origin = attachmentOrigin(inputShape, cylinder.height);
    const kernel::CylinderDefinition definition{
        cylinder.radius,
        cylinder.height,
        origin.X(),
        origin.Y(),
        origin.Z()};
    const TopoDS_Shape cylinderShape = kernel::ShapeFactory::makeCylinder(definition);
    if (!applyPlacement) {
        return cylinderShape;
    }
    return kernel::ShapeFactory::applyTransform(
        cylinderShape,
        origin.X(),
        origin.Y(),
        origin.Z() + cylinder.height * 0.5,
        cylinder.placement);
}

gp_Pnt CylinderFeature::attachmentOrigin(
    const TopoDS_Shape& inputShape,
    double cylinderHeight)
{
    Bnd_Box bounds;
    BRepBndLib::AddOptimal(inputShape, bounds, false, false);
    if (bounds.IsVoid()) {
        throw std::runtime_error("Cylinder input feature has no geometry");
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
