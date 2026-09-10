#include <voelund/features/BoxFeature.hpp>

#include <voelund/document/ModelDocument.hpp>
#include <voelund/kernel/ShapeFactory.hpp>

namespace voelund::features {

BoxFeature::BoxFeature(document::FeatureId featureId) noexcept
    : featureId_(featureId)
{
}

TopoDS_Shape BoxFeature::evaluate(const document::ModelDocument& document) const
{
    const document::BoxParameters& parameters = document.boxParameters(featureId_);
    const TopoDS_Shape shape = kernel::ShapeFactory::makeBox(parameters.dimensions);
    return kernel::ShapeFactory::applyTransform(
        shape,
        parameters.dimensions.width * 0.5,
        parameters.dimensions.depth * 0.5,
        parameters.dimensions.height * 0.5,
        parameters.placement);
}

} // namespace voelund::features
