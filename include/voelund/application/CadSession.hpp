#pragma once
#include <voelund/application/CommandTransaction.hpp>
#include <voelund/application/SceneStatus.hpp>

#include <voelund/document/DocumentTransaction.hpp>
#include <voelund/document/ModelDocument.hpp>
#include <voelund/document/IdentifiedDocumentSnapshot.hpp>
#include <voelund/document/LegacyDocumentImporter.hpp>
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

enum class FeatureRuntimeStatus { current, error, blocked };

struct FeatureRuntimeState final {
    document::FeatureId id;
    FeatureRuntimeStatus status;
    std::string message;
};

class CadSession final {
  public:
    CadSession();
    [[nodiscard]] document::DocumentId documentId() const noexcept { return documentId_; }
    [[nodiscard]] document::IdentifiedDocumentSnapshot identifiedSnapshot() const;
    // Replace this session with an unsaved copy in a new namespace; clear transient history.
    void duplicateDocument();

    [[nodiscard]] TopoDS_Shape currentShape() const;
    // Retained geometry is for explicitly labelled display only, never current evaluation.
    [[nodiscard]] TopoDS_Shape displayShape() const { return currentShape_; }
    [[nodiscard]] std::span<const features::EvaluatedBody> displayBodies() const noexcept {
        return currentBodies_;
    }
    [[nodiscard]] SceneStatus sceneStatus() const noexcept {
        return {{generation_, document_.revision()}, geometryStamp_};
    }
    [[nodiscard]] std::optional<TopoDS_Shape> featureShape(document::FeatureId featureId) const;
    [[nodiscard]] std::span<const features::EvaluatedBody> currentBodies() const;
    [[nodiscard]] std::vector<document::FeatureDescriptor> features() const;
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
    [[nodiscard]] document::SketchParameters selectedSketchParameters() const;
    [[nodiscard]] document::SketchParameters
    sketchParameters(document::FeatureId featureId) const;
    [[nodiscard]] CommandResult
    editSketchParameters(document::FeatureId featureId,
                         const document::SketchParameters& parameters,
                         ErrorDisposition disposition = ErrorDisposition::reject);
    [[nodiscard]] std::vector<features::EvaluatedSketchElement> selectedSketchElements() const;
    [[nodiscard]] CommandResult createBoxFeature();
    [[nodiscard]] CommandResult createCylinderFeature(
        document::CylinderResultMode resultMode = document::CylinderResultMode::separate);
    [[nodiscard]] CommandResult createSketchExtrusion(document::FeatureId supportFeature,
                                                      const document::SketchPlane& plane,
                                                      const document::SketchRectangle& rectangle,
                                                      double distance = 10.0);
    [[nodiscard]] CommandResult createSketchFeature(document::FeatureId supportFeature,
                                                    const document::SketchParameters& sketch);
    [[nodiscard]] CommandResult createSketchExtrusion(document::FeatureId supportFeature,
                                                      const document::SketchParameters& sketch,
                                                      double distance = 10.0);
    [[nodiscard]] CommandResult
    createSketchProfileExtrusion(document::FeatureId sketchFeature,
                                 std::span<const document::SketchProfileId> profileIds,
                                 double distance = 10.0);
    [[nodiscard]] bool canAddCylinder() const noexcept;
    [[nodiscard]] bool canTransformFeature(document::FeatureId featureId) const noexcept;
    [[nodiscard]] std::optional<document::FeatureId>
    transformBodyForFeature(document::FeatureId featureId) const noexcept;
    [[nodiscard]] document::FeatureDescriptor selectedFeatureDescriptor() const;
    [[nodiscard]] kernel::BoxDimensions selectedBoxDimensions() const;
    [[nodiscard]] document::BoxParameters selectedBoxParameters() const;
    [[nodiscard]] CommandResult
    editSelectedBoxDimensions(const kernel::BoxDimensions& dimensions,
                              ErrorDisposition disposition = ErrorDisposition::reject);
    [[nodiscard]] CommandResult
    editSelectedBoxParameters(const document::BoxParameters& parameters,
                              ErrorDisposition disposition = ErrorDisposition::reject);
    [[nodiscard]] CommandResult
    editSelectedCylinderParameters(const document::CylinderParameters& parameters,
                                   ErrorDisposition disposition = ErrorDisposition::reject);
    [[nodiscard]] CommandResult
    editCylinderParameters(document::FeatureId featureId,
                           const document::CylinderParameters& parameters,
                           ErrorDisposition disposition = ErrorDisposition::reject);
    [[nodiscard]] CommandResult
    editSelectedExtrusionParameters(const document::ExtrusionParameters& parameters,
                                    ErrorDisposition disposition = ErrorDisposition::reject);
    [[nodiscard]] CommandResult
    editSelectedFeaturePlacement(const kernel::RigidTransform& placement,
                                 ErrorDisposition disposition = ErrorDisposition::reject);
    [[nodiscard]] document::CylinderParameters selectedCylinderParameters() const;
    [[nodiscard]] document::CylinderParameters
    cylinderParameters(document::FeatureId featureId) const;
    [[nodiscard]] document::ExtrusionParameters selectedExtrusionParameters() const;
    [[nodiscard]] kernel::RigidTransform selectedFeaturePlacement() const;
    [[nodiscard]] std::span<const FeatureRuntimeState> featureStates() const noexcept;
    [[nodiscard]] bool canUndo() const noexcept;
    [[nodiscard]] bool canRedo() const noexcept;
    [[nodiscard]] CommandResult undo();
    [[nodiscard]] CommandResult redo();
    [[nodiscard]] bool isModified() const;
    void newDocument();
    void saveDocument(const std::filesystem::path& path);
    void loadDocument(const std::filesystem::path& path);
    // Provenance of the last successful import; editing/saving does not resolve its issues.
    [[nodiscard]] const std::optional<document::MigrationReport>& lastImportReport() const noexcept {
        return importReport_;
    }
    [[nodiscard]] const std::filesystem::path& currentFile() const noexcept { return currentFile_; }

  private:
    friend class CadSessionPersistenceTestAccess;
    using DocumentWriter = std::function<void(const std::filesystem::path&, std::string_view)>;
    void saveDocumentUsing(const std::filesystem::path& path, const DocumentWriter& writer);
    [[nodiscard]] CommandResult finishCandidate(document::DocumentTransaction& transaction,
                                                std::optional<document::FeatureId> selection,
                                                CommandKind kind, ErrorDisposition disposition,
                                                bool clearSketchSelection);
    [[nodiscard]] CommandResult restoreHistory(bool forward);
    [[nodiscard]] bool recompute();
    void markFeaturesCurrent();
    void markFeatureError(document::FeatureId featureId, const std::string& message);
    void normalizeSelection() noexcept;
    [[nodiscard]] std::string serializedDocument() const;

    document::ModelDocument document_;
    document::DocumentId documentId_{document::generateDocumentId()};
    features::FeatureGraph featureGraph_;
    TopoDS_Shape currentShape_;
    std::vector<features::EvaluatedBody> currentBodies_;
    std::vector<features::EvaluatedFeature> currentFeatures_;
    std::optional<document::FeatureId> selectedFeature_{document::ModelDocument::initialBoxId()};
    std::optional<document::SketchElementId> selectedSketchElement_;
    std::vector<document::SketchProfileId> selectedSketchProfiles_;
    std::vector<FeatureRuntimeState> featureStates_;
    std::optional<document::DocumentSnapshot> cleanDocumentState_;
    std::filesystem::path currentFile_;
    std::optional<document::MigrationReport> importReport_;
    std::uint64_t generation_{0};
    std::optional<DocumentStamp> geometryStamp_;
};

} // namespace voelund::application
