#include <algorithm>
#include <voelund/application/CadSession.hpp>
#include <voelund/features/SketchSupportBinding.hpp>
#include <voelund/features/SketchSupportResolver.hpp>

namespace voelund::application {
document::SketchSupportBinding
CadSession::bindSelectedSupport(document::FeatureId support, const TopoDS_Shape& face,
                                const document::SketchPlane& plane) const {
    if (!evaluation_ || face.IsNull())
        throw std::invalid_argument("Select a current planar face");
    const auto& outputs = evaluation_->result.features;
    const auto found = std::ranges::find(outputs, support, &features::EvaluatedFeature::id);
    if (found == outputs.end())
        throw std::invalid_argument("Support is not current");
    std::optional<features::SupportOrigin> origin;
    for (const auto& item : found->supportOrigins) {
        if (item.coverage != kernel::HistoryCoverage::complete || item.faces.size() != 1 ||
            !item.faces.front().IsSame(face))
            continue;
        if (origin)
            throw std::invalid_argument("Selected face has ambiguous ancestry");
        origin = item.origin;
    }
    if (!origin)
        throw std::invalid_argument("Selected face has no unique stable support origin");
    return features::bindSketchSupport(documentId(), *found, *origin, face, plane);
}
document::SketchParameters
CadSession::sketchParametersForEditing(document::FeatureId featureId) const {
    auto sketch = document_.sketchParameters(featureId);
    if (sketch.repairReason)
        throw std::invalid_argument(
            "MigrationNeedsRepair: repair sketch references before editing");
    const auto descriptors = document_.features();
    const auto descriptor =
        std::ranges::find(descriptors, featureId, &document::FeatureDescriptor::id);
    if (descriptor == descriptors.end() || !descriptor->inputFeature || !evaluation_)
        throw std::invalid_argument("Sketch support is not current");
    const auto& outputs = evaluation_->result.features;
    const auto support =
        std::ranges::find(outputs, *descriptor->inputFeature, &features::EvaluatedFeature::id);
    if (support == outputs.end())
        throw std::invalid_argument("Sketch support is not current");
    if (sketch.supportBinding)
        features::resolveBoundSketchSupport(sketch, *support);
    else if (!features::updateSketchSupportReferences(sketch, support->shape))
        throw std::invalid_argument("Legacy sketch support is no longer available");
    return sketch;
}
} // namespace voelund::application
