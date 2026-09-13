#pragma once

#include <voelund/document/DocumentTransaction.hpp>
#include <voelund/document/ModelDocument.hpp>
#include <voelund/features/FeatureGraph.hpp>
#include <voelund/features/SketchEvaluation.hpp>

#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace voelund::application {

enum class FeatureRuntimeStatus { current, error };

struct FeatureRuntimeState final {
    document::FeatureId id;
    FeatureRuntimeStatus status;
    std::string message;
};

class CadSession final {
  public:
    CadSession();

    [[nodiscard]] TopoDS_Shape currentShape() const;
    [[nodiscard]] std::optional<TopoDS_Shape> featureShape(document::FeatureId featureId) const;
    [[nodiscard]] std::span<const features::EvaluatedBody> currentBodies() const noexcept;
    [[nodiscard]] std::span<const document::FeatureDescriptor> features() const noexcept;
    [[nodiscard]] std::optional<document::FeatureId> selectedFeature() const noexcept;
    [[nodiscard]] bool selectFeature(document::FeatureId featureId) noexcept;
    [[nodiscard]] std::optional<document::SketchElementId> selectedSketchElement() const noexcept;
    [[nodiscard]] bool selectSketchElement(document::SketchElementId elementId) noexcept;
    [[nodiscard]] std::span<const document::SketchProfileId>
    selectedSketchProfiles() const noexcept;
    [[nodiscard]] bool selectSketchProfileByElement(document::SketchElementId elementId,
                                                    bool additive);
    [[nodiscard]] std::vector<document::SketchElementId> selectedSketchProfileElements() const;
    [[nodiscard]] bool canExtrudeSelectedSketchProfiles() const noexcept;
    [[nodiscard]] const document::SketchParameters& selectedSketchParameters() const;
    [[nodiscard]] const document::SketchParameters&
    sketchParameters(document::FeatureId featureId) const;
    [[nodiscard]] bool setSketchParameters(document::FeatureId featureId,
                                           const document::SketchParameters& parameters);
    [[nodiscard]] std::vector<features::EvaluatedSketchElement> selectedSketchElements() const;
    [[nodiscard]] document::FeatureId addBoxFeature();
    [[nodiscard]] document::FeatureId addCylinderFeature(
        document::CylinderResultMode resultMode = document::CylinderResultMode::separate);
    [[nodiscard]] document::FeatureId addSketchExtrusion(document::FeatureId supportFeature,
                                                         const document::SketchPlane& plane,
                                                         const document::SketchRectangle& rectangle,
                                                         double distance = 10.0);
    [[nodiscard]] document::FeatureId addSketchFeature(document::FeatureId supportFeature,
                                                       const document::SketchParameters& sketch);
    [[nodiscard]] document::FeatureId addSketchExtrusion(document::FeatureId supportFeature,
                                                         const document::SketchParameters& sketch,
                                                         double distance = 10.0);
    [[nodiscard]] document::FeatureId
    addSketchProfileExtrusion(document::FeatureId sketchFeature,
                              std::span<const document::SketchProfileId> profileIds,
                              double distance = 10.0);
    [[nodiscard]] bool canAddCylinder() const noexcept;
    [[nodiscard]] bool canTransformFeature(document::FeatureId featureId) const noexcept;
    [[nodiscard]] std::optional<document::FeatureId>
    transformBodyForFeature(document::FeatureId featureId) const noexcept;
    [[nodiscard]] const document::FeatureDescriptor& selectedFeatureDescriptor() const;
    [[nodiscard]] const kernel::BoxDimensions& selectedBoxDimensions() const;
    [[nodiscard]] const document::BoxParameters& selectedBoxParameters() const;
    [[nodiscard]] bool setSelectedBoxDimensions(const kernel::BoxDimensions& dimensions);
    [[nodiscard]] bool setSelectedBoxParameters(const document::BoxParameters& parameters);
    [[nodiscard]] const document::CylinderParameters& selectedCylinderParameters() const;
    [[nodiscard]] const document::CylinderParameters&
    cylinderParameters(document::FeatureId featureId) const;
    [[nodiscard]] bool
    setSelectedCylinderParameters(const document::CylinderParameters& parameters);
    [[nodiscard]] bool setCylinderParameters(document::FeatureId featureId,
                                             const document::CylinderParameters& parameters);
    [[nodiscard]] const document::ExtrusionParameters& selectedExtrusionParameters() const;
    [[nodiscard]] bool
    setSelectedExtrusionParameters(const document::ExtrusionParameters& parameters);
    [[nodiscard]] const kernel::RigidTransform& selectedFeaturePlacement() const;
    [[nodiscard]] bool setSelectedFeaturePlacement(const kernel::RigidTransform& placement);
    [[nodiscard]] std::span<const FeatureRuntimeState> featureStates() const noexcept;
    [[nodiscard]] bool canUndo() const noexcept;
    [[nodiscard]] bool canRedo() const noexcept;
    [[nodiscard]] bool undo();
    [[nodiscard]] bool redo();
    [[nodiscard]] bool isModified() const;
    void newDocument();
    void saveDocument(const std::filesystem::path& path);
    void loadDocument(const std::filesystem::path& path);
    [[nodiscard]] const std::filesystem::path& currentFile() const noexcept { return currentFile_; }

  private:
    friend class CadSessionPersistenceTestAccess;
    using DocumentWriter = std::function<void(const std::filesystem::path&, std::string_view)>;
    void saveDocumentUsing(const std::filesystem::path& path, const DocumentWriter& writer);
    [[nodiscard]] bool commitCandidate(document::DocumentTransaction& transaction,
                                       std::optional<document::FeatureId> selection,
                                       bool clearSketchSelection = false);
    [[nodiscard]] bool recompute();
    void markFeaturesCurrent();
    void markFeatureError(document::FeatureId featureId, const std::string& message);
    void normalizeSelection() noexcept;
    [[nodiscard]] std::string serializedDocument() const;

    document::ModelDocument document_;
    features::FeatureGraph featureGraph_;
    TopoDS_Shape currentShape_;
    std::vector<features::EvaluatedBody> currentBodies_;
    std::vector<features::EvaluatedFeature> currentFeatures_;
    std::optional<document::FeatureId> selectedFeature_{document::ModelDocument::initialBoxId()};
    std::optional<document::SketchElementId> selectedSketchElement_;
    std::vector<document::SketchProfileId> selectedSketchProfiles_;
    std::vector<FeatureRuntimeState> featureStates_;
    std::string cleanDocumentState_;
    std::filesystem::path currentFile_;
};

} // namespace voelund::application
