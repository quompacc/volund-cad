#pragma once
#include <voelund/application/CommandTransaction.hpp>
#include <voelund/application/SceneStatus.hpp>
#include <voelund/application/SketchReferenceInspection.hpp>
#include <voelund/application/SketchSupportProjection.hpp>

#include <voelund/document/DocumentTransaction.hpp>
#include <voelund/document/IdentifiedDocumentSnapshot.hpp>
#include <voelund/document/LegacyDocumentImporter.hpp>
#include <voelund/document/ModelDocument.hpp>
#include <voelund/features/FeatureGraph.hpp>
#include <voelund/features/SketchEvaluation.hpp>
#include <voelund/features/SketchRegionIdentity.hpp>
#include <voelund/features/SketchSupportBinding.hpp>

#include <array>
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

struct ExtrusionRegionState final {
    document::SketchRegionId region;
    features::SketchRegionReferenceStatus status;
};
struct ExtrusionSelectionState final {
    std::vector<document::SketchProfileId> legacyProfiles; // Evaluated as before; needs repair.
    std::vector<ExtrusionRegionState> regions;
    [[nodiscard]] bool needsRepair() const noexcept;
};

// U06: features grouped by the body they work on, both in document order. Features that
// belong to no body (invalid documents) stay loose.
struct OutlineBody final {
    document::BodyId id;
    std::vector<document::FeatureId> features;
};
struct ModelOutline final {
    std::vector<OutlineBody> bodies;
    std::vector<document::FeatureId> loose;
};

// T02: operation, direction and target body of a new extrusion. The default keeps the
// earlier rule (document::effectiveOperation).
struct ExtrusionMode final {
    std::optional<document::ExtrusionOperation> operation{};
    bool reversed{false};
    std::optional<document::BodyId> target{};
    double offset{0.0};                                  // T03
    std::optional<document::BodyFaceReference> upToFace{}; // T03
    bool operator==(const ExtrusionMode&) const = default;
};

// PAR01: deleting a parameter that is still bound is refused unless its uses are first
// switched to their current fixed values (docs/parameter-table.md, rule 6).
enum class ParameterDeletion { refuseWhenUsed, unbindUses };

class CadSession final {
  public:
    CadSession();
    [[nodiscard]] document::DocumentId documentId() const noexcept {
        return document_.documentId();
    }
    [[nodiscard]] document::IdentifiedDocumentSnapshot identifiedSnapshot() const;
    // Replace this session with an unsaved copy in a new namespace; clear transient history.
    void duplicateDocument();

    [[nodiscard]] TopoDS_Shape currentShape() const;
    // Current successful final bodies, including independent branches of an error document.
    [[nodiscard]] std::span<const features::EvaluatedBody> validBodies() const noexcept;
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
    [[nodiscard]] ModelOutline modelOutline() const;
    [[nodiscard]] std::optional<document::FeatureId> selectedFeature() const noexcept;
    [[nodiscard]] bool selectFeature(document::FeatureId featureId) noexcept;
    // Nothing selected (a click into empty space); view state only.
    void clearSelection() noexcept;
    [[nodiscard]] std::optional<document::SketchElementId> selectedSketchElement() const noexcept;
    [[nodiscard]] bool selectSketchElement(document::SketchElementId elementId) noexcept;
    // Sketch "profiles" are R09 regions: picking an element selects the one region whose
    // outer boundary uses it; ambiguous picks select nothing.
    [[nodiscard]] std::span<const document::SketchRegionId> selectedSketchProfiles() const noexcept;
    [[nodiscard]] bool selectSketchProfileByElement(document::SketchElementId elementId,
                                                    bool additive);
    // Picking the area (user report 2026-10-05): the unambiguous region of `sketch` whose face
    // contains the point (model coordinates, on the sketch plane). Selects the sketch.
    [[nodiscard]] bool selectSketchProfileAt(document::FeatureId sketch,
                                             const std::array<double, 3>& point, bool additive);
    [[nodiscard]] std::vector<document::SketchElementId> selectedSketchProfileElements() const;
    [[nodiscard]] bool canExtrudeSelectedSketchProfiles() const noexcept;
    // R09 projection of an extrusion's selection: unmigrated legacy profiles and the
    // current resolution of every region, so the UI can offer repair instead of guessing.
    [[nodiscard]] ExtrusionSelectionState
    inspectExtrusionSelection(document::FeatureId extrusion) const;
    [[nodiscard]] document::SketchParameters selectedSketchParameters() const;
    [[nodiscard]] document::SketchParameters sketchParameters(document::FeatureId featureId) const;
    [[nodiscard]] document::SketchParameters
    sketchParametersForEditing(document::FeatureId featureId) const;
    [[nodiscard]] document::SketchSupportBinding
    bindSelectedSupport(document::FeatureId support, const TopoDS_Shape& face,
                        const document::SketchPlane& plane) const;
    // T01: faces a sketch can name on the current result of `support` (empty when that
    // result is not current), and a sketch bound to one of them in the face's own frame,
    // as a pick of that face would bind it. Throws std::invalid_argument for another face.
    [[nodiscard]] std::vector<features::SupportFaceFrame>
    supportFaces(document::FeatureId support) const;
    [[nodiscard]] document::SketchParameters
    sketchOnFace(document::FeatureId support, const features::SupportOrigin& face) const;
    // T03: a face of `support`'s current result as a stable reference (e.g. an extrusion's
    // end face), from a named origin or from a picked face. Throws std::invalid_argument.
    [[nodiscard]] document::BodyFaceReference
    faceReference(document::FeatureId support, const features::SupportOrigin& face) const;
    [[nodiscard]] document::BodyFaceReference faceReference(document::FeatureId support,
                                                            const TopoDS_Shape& face) const;
    [[nodiscard]] SketchSupportProjection
    projectSketchSupport(document::FeatureId support, const document::SketchSupportBinding& binding,
                         unsigned divisions = 32) const;
    void resolveSketchSupport(document::FeatureId support,
                              document::SketchParameters& sketch) const;
    [[nodiscard]] SketchReferenceInspection
    inspectSketchReferences(document::FeatureId sketch) const;
    // Read-only staged proposals, including downstream references unlocked by earlier proposals.
    [[nodiscard]] std::vector<SketchSupportRepairSuggestion> suggestSketchSupportRepairs() const;
    [[nodiscard]] CommandResult
    repairSketchSupports(std::span<const SketchSupportRepair> repairs,
                         ErrorDisposition disposition = ErrorDisposition::reject);
    [[nodiscard]] CommandResult
    repairSketchSupport(const SketchReferenceInspection& expected,
                        const features::SketchSupportCandidate& chosen,
                        ErrorDisposition disposition = ErrorDisposition::reject);
    [[nodiscard]] CommandResult
    repairSketchEdgeReferences(const SketchReferenceInspection& expected,
                               std::span<const document::SketchConstraintId> slots,
                               const document::ProjectedEdgeReference& chosen,
                               ErrorDisposition disposition = ErrorDisposition::reject);
    [[nodiscard]] CommandResult
    editSketchParameters(document::FeatureId featureId,
                         const document::SketchParameters& parameters,
                         ErrorDisposition disposition = ErrorDisposition::reject);
    [[nodiscard]] std::vector<features::EvaluatedSketchElement> selectedSketchElements() const;
    [[nodiscard]] CommandResult createBoxFeature();
    // AI02: commands with explicit targets; they keep the selection. The legacy variants
    // above and the Selected ones below delegate to them.
    [[nodiscard]] CommandResult createBoxFeature(const kernel::BoxDimensions& dimensions);
    // On the last feature.
    [[nodiscard]] CommandResult createCylinderFeature(
        document::CylinderResultMode resultMode = document::CylinderResultMode::separate);
    [[nodiscard]] CommandResult createCylinderFeature(document::FeatureId inputFeature,
                                                      document::CylinderResultMode resultMode);
    [[nodiscard]] CommandResult createSketchExtrusion(document::FeatureId supportFeature,
                                                      const document::SketchPlane& plane,
                                                      const document::SketchRectangle& rectangle,
                                                      double distance = 10.0);
    [[nodiscard]] CommandResult createSketchFeature(document::FeatureId supportFeature,
                                                    const document::SketchParameters& sketch);
    // V01: a sketch on a base plane (sketch.datum set) needs no body and opens a new one.
    [[nodiscard]] CommandResult createDatumSketchFeature(const document::SketchParameters& sketch);
    [[nodiscard]] CommandResult createSketchExtrusion(document::FeatureId supportFeature,
                                                      const document::SketchParameters& sketch,
                                                      double distance = 10.0);
    [[nodiscard]] CommandResult
    createSketchProfileExtrusion(document::FeatureId sketchFeature,
                                 std::span<const document::SketchRegionId> profileIds,
                                 double distance = 10.0, const ExtrusionMode& mode = {});
    // AI02c: a base-plane sketch and its extrusion as one transaction and one undo step.
    // The regions resolve against the new sketch. Empty fills nested contours alternately:
    // the outer region, not the inside of its holes, again islands inside those.
    [[nodiscard]] CommandResult
    createDatumSketchExtrusion(const document::SketchParameters& sketch,
                               std::span<const document::SketchRegionId> regions,
                               double distance, const ExtrusionMode& mode = {});
    [[nodiscard]] bool canAddCylinder() const noexcept;
    [[nodiscard]] bool canAddCylinder(document::FeatureId inputFeature) const noexcept;
    // The feature whose placement moves this feature's result, if any.
    [[nodiscard]] std::optional<document::FeatureId>
    placementFeature(document::FeatureId featureId) const;
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
    editBoxDimensions(document::FeatureId featureId, const kernel::BoxDimensions& dimensions,
                      ErrorDisposition disposition = ErrorDisposition::reject);
    [[nodiscard]] CommandResult
    editBoxParameters(document::FeatureId featureId, const document::BoxParameters& parameters,
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
    editExtrusionParameters(document::FeatureId featureId,
                            const document::ExtrusionParameters& parameters,
                            ErrorDisposition disposition = ErrorDisposition::reject);
    [[nodiscard]] CommandResult
    editSelectedFeaturePlacement(const kernel::RigidTransform& placement,
                                 ErrorDisposition disposition = ErrorDisposition::reject);
    // Moves the placement feature of featureId.
    [[nodiscard]] CommandResult
    editFeaturePlacement(document::FeatureId featureId, const kernel::RigidTransform& placement,
                         ErrorDisposition disposition = ErrorDisposition::reject);
    [[nodiscard]] document::CylinderParameters selectedCylinderParameters() const;
    [[nodiscard]] document::CylinderParameters
    cylinderParameters(document::FeatureId featureId) const;
    [[nodiscard]] document::ExtrusionParameters selectedExtrusionParameters() const;
    [[nodiscard]] document::ExtrusionParameters
    extrusionParameters(document::FeatureId featureId) const;
    [[nodiscard]] kernel::RigidTransform selectedFeaturePlacement() const;
    [[nodiscard]] std::span<const FeatureRuntimeState> featureStates() const noexcept;
    // PAR01 document parameters. Every command is one transaction and one undo step.
    [[nodiscard]] const document::ParameterTable& parameterTable() const noexcept;
    [[nodiscard]] std::size_t parameterUseCount(document::ParameterId id) const;
    [[nodiscard]] CommandResult createParameter(std::string name, document::ParameterUnit unit,
                                                double value, std::string comment = {});
    // Mirrors the value into every binding, re-solves bound sketches and recomputes; an
    // unsolvable result is kept as an editable error by default.
    [[nodiscard]] CommandResult
    setParameterValue(document::ParameterId id, double value,
                      ErrorDisposition disposition = ErrorDisposition::keepEditableErrors);
    [[nodiscard]] CommandResult renameParameter(document::ParameterId id, std::string name);
    [[nodiscard]] CommandResult setParameterComment(document::ParameterId id,
                                                    std::string comment);
    [[nodiscard]] CommandResult
    deleteParameter(document::ParameterId id,
                    ParameterDeletion mode = ParameterDeletion::refuseWhenUsed);
    // nullopt unbinds and keeps the current value as a fixed one.
    [[nodiscard]] CommandResult
    bindSketchDimension(document::FeatureId sketch, document::SketchConstraintId constraint,
                        std::optional<document::ParameterId> parameter,
                        ErrorDisposition disposition = ErrorDisposition::keepEditableErrors);
    [[nodiscard]] CommandResult
    bindExtrusionDistance(document::FeatureId extrusion,
                          std::optional<document::ParameterId> parameter,
                          ErrorDisposition disposition = ErrorDisposition::keepEditableErrors,
                          bool offset = false); // T03: the start offset instead
    [[nodiscard]] bool canUndo() const noexcept;
    [[nodiscard]] bool canRedo() const noexcept;
    [[nodiscard]] CommandResult undo();
    [[nodiscard]] CommandResult redo();
    [[nodiscard]] bool isModified() const;
    void newDocument();
    void saveDocument(const std::filesystem::path& path);
    void loadDocument(const std::filesystem::path& path);
    // Provenance of the last successful import; editing/saving does not resolve its issues.
    [[nodiscard]] const std::optional<document::MigrationReport>&
    lastImportReport() const noexcept {
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
    [[nodiscard]] CommandResult
    commitParameterEdit(const std::function<bool(document::ModelDocument&)>& edit,
                        ErrorDisposition disposition);
    [[nodiscard]] bool mirrorParameter(document::ModelDocument& candidate,
                                       document::ParameterId id) const;
    [[nodiscard]] bool recompute();
    void normalizeSelection() noexcept;
    [[nodiscard]] std::string serializedDocument() const;

    document::ModelDocument document_;
    features::FeatureGraph featureGraph_;
    std::shared_ptr<const features::FeatureGraphEvaluation> evaluation_;
    TopoDS_Shape currentShape_;
    std::vector<features::EvaluatedBody> currentBodies_;
    std::vector<features::EvaluatedFeature> currentFeatures_;
    std::optional<document::FeatureId> selectedFeature_{document::ModelDocument::initialBoxId()};
    std::optional<document::SketchElementId> selectedSketchElement_;
    std::vector<document::SketchRegionId> selectedSketchProfiles_;
    std::vector<FeatureRuntimeState> featureStates_;
    std::optional<document::DocumentSnapshot> cleanDocumentState_;
    std::filesystem::path currentFile_;
    std::optional<document::MigrationReport> importReport_;
    std::uint64_t generation_{0};
    std::optional<DocumentStamp> geometryStamp_;
};

} // namespace voelund::application
