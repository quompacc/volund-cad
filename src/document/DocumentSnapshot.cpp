#include "ModelDocumentImpl.hpp"
#include <algorithm>
#include <stdexcept>
#include <voelund/document/DocumentSnapshot.hpp>

namespace voelund::document {
DocumentSnapshot DocumentSnapshot::capture(const ModelDocument& document) {
    return document.impl_->view();
}
bool DocumentSnapshot::matchesContent(const ModelDocument& document) const {
    const auto& view = document.impl_->view();
    return std::ranges::equal(features_, view.features()) && std::ranges::equal(parameters_, view.parameters());
}
const FeatureParameters& DocumentSnapshot::featureParameters(FeatureId id) const {
    const auto found = std::ranges::find(features_, id, &FeatureDescriptor::id);
    if (found == features_.end())
        throw std::out_of_range("Unknown feature ID");
    return parameters_.at(static_cast<std::size_t>(found - features_.begin()));
}
const BoxParameters& DocumentSnapshot::boxParameters(FeatureId id) const {
    return std::get<BoxParameters>(featureParameters(id));
}
const CylinderParameters& DocumentSnapshot::cylinderParameters(FeatureId id) const {
    return std::get<CylinderParameters>(featureParameters(id));
}
const SketchParameters& DocumentSnapshot::sketchParameters(FeatureId id) const {
    return std::get<SketchParameters>(featureParameters(id));
}
const ExtrusionParameters& DocumentSnapshot::extrusionParameters(FeatureId id) const {
    return std::get<ExtrusionParameters>(featureParameters(id));
}
const kernel::BoxDimensions& DocumentSnapshot::boxDimensions(FeatureId id) const {
    return boxParameters(id).dimensions;
}
ModelDocument DocumentSnapshot::toDocument() const {
    return toDocument(generateDocumentId());
}
ModelDocument DocumentSnapshot::toDocument(DocumentId id) const {
    return ModelDocument(*this, id);
}
} // namespace voelund::document