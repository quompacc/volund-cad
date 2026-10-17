#pragma once
#include <voelund/document/BodyAttributes.hpp>
#include <voelund/document/ModelDocument.hpp>

#include <functional>

namespace voelund::document {
namespace detail {
class NativePrimitiveCodec;
class NativeModelState;
} // namespace detail
class LegacyDocumentImporter;
// Validated, read-only interchange value; no OCAF labels or mutable document authority.
class DocumentSnapshot final {
  public:
    [[nodiscard]] static DocumentSnapshot capture(const ModelDocument& document);
    // Model content equality deliberately excludes the monotonic allocation watermark.
    [[nodiscard]] bool matchesContent(const ModelDocument& document) const;
    [[nodiscard]] std::span<const FeatureDescriptor> features() const noexcept { return features_; }
    [[nodiscard]] std::span<const FeatureParameters> parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] std::uint64_t nextFeatureId() const noexcept { return nextFeatureId_; }
    [[nodiscard]] const ParameterTable& parameterTable() const noexcept { return parameterTable_; }
    [[nodiscard]] const BodyAttributeTable& bodyAttributes() const noexcept {
        return bodyAttributes_;
    }
    // M14: the rollback marker stands after this feature; later ones are not evaluated. None
    // when the whole history is evaluated. Stored, but no model content (see matchesContent).
    [[nodiscard]] std::optional<FeatureId> rollbackAfter() const noexcept { return rollbackAfter_; }
    // Borrowed read-only views remain valid while this snapshot is alive and unchanged.
    [[nodiscard]] const FeatureParameters& featureParameters(FeatureId id) const;
    [[nodiscard]] const BoxParameters& boxParameters(FeatureId id) const;
    [[nodiscard]] const CylinderParameters& cylinderParameters(FeatureId id) const;
    [[nodiscard]] const PrimitiveParameters& primitiveParameters(FeatureId id) const; // M27
    [[nodiscard]] const DraftParameters& draftParameters(FeatureId id) const;         // M25
    [[nodiscard]] const MirrorParameters& mirrorParameters(FeatureId id) const;       // M09
    [[nodiscard]] const PatternParameters& patternParameters(FeatureId id) const;     // M10
    [[nodiscard]] const PatternCopyParameters& patternCopyParameters(FeatureId id) const;
    [[nodiscard]] const MoveParameters& moveParameters(FeatureId id) const;     // M11
    [[nodiscard]] const ImportParameters& importParameters(FeatureId id) const; // M17
    [[nodiscard]] const ImportPartParameters& importPartParameters(FeatureId id) const;
    [[nodiscard]] const SketchParameters& sketchParameters(FeatureId id) const;
    [[nodiscard]] const ExtrusionParameters& extrusionParameters(FeatureId id) const;
    [[nodiscard]] const FilletParameters& filletParameters(FeatureId id) const;
    [[nodiscard]] const ChamferParameters& chamferParameters(FeatureId id) const;
    [[nodiscard]] const HoleParameters& holeParameters(FeatureId id) const;
    [[nodiscard]] const CombineParameters& combineParameters(FeatureId id) const;
    [[nodiscard]] const DatumPlaneParameters& datumPlaneParameters(FeatureId id) const;
    [[nodiscard]] const DatumAxisParameters& datumAxisParameters(FeatureId id) const;
    [[nodiscard]] const ShellParameters& shellParameters(FeatureId id) const;
    [[nodiscard]] const kernel::BoxDimensions& boxDimensions(FeatureId id) const;
    [[nodiscard]] ModelDocument toDocument() const;
    [[nodiscard]] ModelDocument toDocument(DocumentId id) const;
    [[nodiscard]] DocumentSnapshot remapSupportNamespace(DocumentId from, DocumentId to) const;
    [[nodiscard]] DocumentSnapshot markLegacySketchReferencesForRepair() const;
    // Maps a legacy minimum-element profile of the given sketch to regions, or nullopt.
    using LegacyProfileMapper = std::function<std::optional<std::vector<SketchRegionId>>(
        const SketchParameters& sketch, SketchProfileId profile)>;
    // R09 Open migration: an extrusion whose every legacy profile maps is rewritten to the
    // union of the mapped regions; otherwise it keeps its profiles and needs repair.
    [[nodiscard]] DocumentSnapshot
    migrateLegacyExtrusionProfiles(const LegacyProfileMapper& mapper) const;
    [[nodiscard]] bool operator==(const DocumentSnapshot&) const = default;

  private:
    friend class LegacyDocumentImporter;
    friend class ModelDocument;
    friend class detail::NativePrimitiveCodec;
    friend class detail::NativeModelState;
    DocumentSnapshot(std::vector<FeatureDescriptor> features,
                     std::vector<FeatureParameters> parameters, std::uint64_t nextFeatureId,
                     ParameterTable parameterTable = {}, BodyAttributeTable bodyAttributes = {},
                     std::optional<FeatureId> rollbackAfter = std::nullopt)
        : features_(std::move(features)), parameters_(std::move(parameters)),
          nextFeatureId_(nextFeatureId), parameterTable_(std::move(parameterTable)),
          bodyAttributes_(std::move(bodyAttributes)), rollbackAfter_(rollbackAfter) {}
    std::vector<FeatureDescriptor> features_;
    std::vector<FeatureParameters> parameters_;
    std::uint64_t nextFeatureId_;
    ParameterTable parameterTable_;
    BodyAttributeTable bodyAttributes_;
    std::optional<FeatureId> rollbackAfter_;
};
} // namespace voelund::document
