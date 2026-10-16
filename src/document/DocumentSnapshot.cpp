#include "ModelDocumentImpl.hpp"
#include <algorithm>
#include <stdexcept>
#include <voelund/document/DocumentSnapshot.hpp>

namespace voelund::document {
DocumentSnapshot DocumentSnapshot::markLegacySketchReferencesForRepair() const {
    auto result = *this;
    for (auto& parameter : result.parameters_) {
        auto* sketch = std::get_if<SketchParameters>(&parameter);
        if (!sketch || sketch->repairReason || sketch->datum || sketch->constructionPlane)
            continue; // Base-plane sketches have no support to repair.
        if (!sketch->supportBinding)
            sketch->repairReason = SketchRepairReason::legacySupport;
        else if (std::ranges::any_of(sketch->constraints.values, [](const auto& c) {
                     return (c.kind == SketchConstraintKind::coordinateX ||
                             c.kind == SketchConstraintKind::coordinateY) &&
                            c.auxiliaryValues.size() == 2;
                 }))
            sketch->repairReason = SketchRepairReason::legacyEdges;
    }
    return result;
}
DocumentSnapshot
DocumentSnapshot::migrateLegacyExtrusionProfiles(const LegacyProfileMapper& mapper) const {
    auto result = *this;
    for (std::size_t index = 0; index < result.features_.size(); ++index) {
        auto* extrusion = std::get_if<ExtrusionParameters>(&result.parameters_[index]);
        const auto& input = result.features_[index].inputFeature;
        if (!extrusion || extrusion->profiles.empty() || !input)
            continue;
        const auto sketch = std::ranges::find(result.features_, *input, &FeatureDescriptor::id);
        if (sketch == result.features_.end())
            continue;
        const auto* parameters = std::get_if<SketchParameters>(
            &result.parameters_[static_cast<std::size_t>(sketch - result.features_.begin())]);
        if (!parameters)
            continue;
        std::vector<SketchRegionId> regions;
        bool mapped = true;
        for (const SketchProfileId profile : extrusion->profiles) {
            const auto target = mapper(*parameters, profile);
            if (!target) {
                mapped = false;
                break;
            }
            regions.insert(regions.end(), target->begin(), target->end());
        }
        if (!mapped)
            continue;
        std::ranges::sort(regions);
        const auto duplicates = std::ranges::unique(regions);
        regions.erase(duplicates.begin(), duplicates.end());
        extrusion->profiles.clear();
        extrusion->regions = std::move(regions);
    }
    return result;
}
DocumentSnapshot DocumentSnapshot::capture(const ModelDocument& document) {
    return document.impl_->view();
}
bool DocumentSnapshot::matchesContent(const ModelDocument& document) const {
    const auto& view = document.impl_->view();
    return std::ranges::equal(features_, view.features()) &&
           std::ranges::equal(parameters_, view.parameters()) &&
           parameterTable_ == view.parameterTable() &&
           bodyAttributes_ == view.bodyAttributes();
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
const PrimitiveParameters& DocumentSnapshot::primitiveParameters(FeatureId id) const {
    return std::get<PrimitiveParameters>(featureParameters(id));
}
const DraftParameters& DocumentSnapshot::draftParameters(FeatureId id) const {
    return std::get<DraftParameters>(featureParameters(id));
}
const MirrorParameters& DocumentSnapshot::mirrorParameters(FeatureId id) const {
    return std::get<MirrorParameters>(featureParameters(id));
}
const PatternParameters& DocumentSnapshot::patternParameters(FeatureId id) const {
    return std::get<PatternParameters>(featureParameters(id));
}
const PatternCopyParameters& DocumentSnapshot::patternCopyParameters(FeatureId id) const {
    return std::get<PatternCopyParameters>(featureParameters(id));
}
const MoveParameters& DocumentSnapshot::moveParameters(FeatureId id) const {
    return std::get<MoveParameters>(featureParameters(id));
}
const ImportParameters& DocumentSnapshot::importParameters(FeatureId id) const {
    return std::get<ImportParameters>(featureParameters(id));
}
const ImportPartParameters& DocumentSnapshot::importPartParameters(FeatureId id) const {
    return std::get<ImportPartParameters>(featureParameters(id));
}
const SketchParameters& DocumentSnapshot::sketchParameters(FeatureId id) const {
    return std::get<SketchParameters>(featureParameters(id));
}
const ExtrusionParameters& DocumentSnapshot::extrusionParameters(FeatureId id) const {
    return std::get<ExtrusionParameters>(featureParameters(id));
}
const FilletParameters& DocumentSnapshot::filletParameters(FeatureId id) const {
    return std::get<FilletParameters>(featureParameters(id));
}
const ChamferParameters& DocumentSnapshot::chamferParameters(FeatureId id) const {
    return std::get<ChamferParameters>(featureParameters(id));
}
const HoleParameters& DocumentSnapshot::holeParameters(FeatureId id) const {
    return std::get<HoleParameters>(featureParameters(id));
}
const CombineParameters& DocumentSnapshot::combineParameters(FeatureId id) const {
    return std::get<CombineParameters>(featureParameters(id));
}
const DatumPlaneParameters& DocumentSnapshot::datumPlaneParameters(FeatureId id) const {
    return std::get<DatumPlaneParameters>(featureParameters(id));
}
const DatumAxisParameters& DocumentSnapshot::datumAxisParameters(FeatureId id) const {
    return std::get<DatumAxisParameters>(featureParameters(id));
}
const ShellParameters& DocumentSnapshot::shellParameters(FeatureId id) const {
    return std::get<ShellParameters>(featureParameters(id));
}
const kernel::BoxDimensions& DocumentSnapshot::boxDimensions(FeatureId id) const {
    return boxParameters(id).dimensions;
}
ModelDocument DocumentSnapshot::toDocument() const {
    for (const auto& parameter : parameters_)
        if (const auto* sketch = std::get_if<SketchParameters>(&parameter);
            sketch && sketch->supportBinding)
            throw std::invalid_argument(
                "Bound snapshot requires its document identity or explicit duplication");
    return toDocument(generateDocumentId());
}
DocumentSnapshot DocumentSnapshot::remapSupportNamespace(DocumentId from, DocumentId to) const {
    auto result = *this;
    for (auto& parameter : result.parameters_)
        if (auto* sketch = std::get_if<SketchParameters>(&parameter);
            sketch && sketch->supportBinding) {
            if (sketch->supportBinding->reference.document != from)
                throw std::invalid_argument("Cannot remap a foreign support namespace");
            sketch->supportBinding->reference.document = to;
            for (auto& constraint : sketch->constraints.values)
                if (constraint.projectedEdge) {
                    if (constraint.projectedEdge->document != from)
                        throw std::invalid_argument(
                            "Cannot remap a foreign projected edge namespace");
                    constraint.projectedEdge->document = to;
                }
        }
    return result;
}
ModelDocument DocumentSnapshot::toDocument(DocumentId id) const { return ModelDocument(*this, id); }
} // namespace voelund::document
