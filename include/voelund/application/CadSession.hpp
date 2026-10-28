// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once
#include <voelund/application/CommandTransaction.hpp>
#include <voelund/application/ExtrusionSelectionState.hpp>
#include <voelund/application/FeatureRuntimeState.hpp>
#include <voelund/application/FieldBindingRequest.hpp>
#include <voelund/application/SceneStatus.hpp>
#include <voelund/application/SketchReferenceInspection.hpp>
#include <voelund/application/SketchSupportProjection.hpp>
#include <voelund/application/StepExchangeResult.hpp>

#include <voelund/document/DocumentTransaction.hpp>
#include <voelund/document/IdentifiedDocumentSnapshot.hpp>
#include <voelund/document/LegacyDocumentImporter.hpp>
#include <voelund/document/ModelDocument.hpp>
#include <voelund/features/FeatureGraph.hpp>
#include <voelund/features/SketchEvaluation.hpp>
#include <voelund/features/SketchRegionIdentity.hpp>
#include <voelund/features/SketchSupportBinding.hpp>
#include <voelund/kernel/StepExchange.hpp>

#include <array>
#include <filesystem>
#include <functional>
#include <iosfwd>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace voelund::application {
// U06: features grouped by the body they work on, both in document order. Features that
// belong to no body (invalid documents) stay loose.
struct OutlineBody final {
    document::BodyId id;
    std::vector<document::FeatureId> features;
    // M01: the user's name (empty: the shell numbers it), the stored visibility; N06: colour.
    std::string name{};
    bool hidden{false};
    std::optional<std::uint32_t> color{};
    std::optional<document::BodyMaterial> material{}; // N07
    bool consumed{false}; // M21: combined into another body (its tools are not kept)
};
struct ModelOutline final {
    std::vector<OutlineBody> bodies;
    std::vector<document::FeatureId> loose;
};

// T02: operation, direction and target body of a new extrusion (default: the earlier rule).
struct ExtrusionMode final {
    std::optional<document::ExtrusionOperation> operation{};
    bool reversed{false};
    std::optional<document::BodyId> target{};
    double offset{0.0};                                    // T03
    std::optional<document::BodyFaceReference> upToFace{}; // T03
    bool symmetric{false};                                 // M02: distance = total length
    std::optional<document::RevolveParameters> revolve{};  // M20: "Drehung N"
    std::optional<document::SweepPath> sweep{};            // M23: "Folgen N"
    std::optional<document::LoftParameters> loft{};        // M24: "Ausformung N"
    // PAR02: distance and start offset bound to length parameters; their values follow them.
    std::optional<document::ParameterId> distanceParameter{};
    std::optional<document::ParameterId> offsetParameter{};
    double taperAngle{0.0}; // W16: degrees, positive wider towards the end
    bool operator==(const ExtrusionMode&) const = default;
};

// PAR01: deleting a parameter that is still bound is refused unless its uses are first
// switched to their current fixed values (docs/parameter-table.md, rule 6).
enum class ParameterDeletion { refuseWhenUsed, unbindUses };

// M10: what a new pattern is given; the axis is a base axis, an existing construction axis or
// an edge of `edgeSupport`'s current result (exactly one).
struct PatternInput final {
    document::PatternKind kind{document::PatternKind::linear};
    std::optional<document::BaseAxis> baseAxis{};
    std::optional<document::FeatureId> datumAxis{};
    std::optional<document::FeatureId> edgeSupport{};
    std::optional<document::FilletEdge> edge{};
    bool reversed{false};
    std::uint32_t count{2};
    double spacing{10.0};
    double angle{0.0};
    document::PatternMode mode{document::PatternMode::newBodies};
    bool operator==(const PatternInput&) const = default;
};

// M16: what deleting a feature takes along: `dependents` need it, `cascade` adds its copies and
// them. Without dependents, or when they may keep errors (onlyThisPossible), it can go alone.
struct DeletionPlan final {
    std::vector<document::FeatureId> dependents;
    std::vector<document::FeatureId> cascade;
    bool onlyThisPossible{true};
};

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
    // Profiles are R09 regions: an element picks the one region its outer boundary uses.
    [[nodiscard]] std::span<const document::SketchRegionId> selectedSketchProfiles() const noexcept;
    [[nodiscard]] bool selectSketchProfileByElement(document::SketchElementId elementId,
                                                    bool additive);
    // Picking the area (user report 2026-10-05): the unambiguous region of `sketch` whose face
    // contains the point (model coordinates, on the sketch plane). Selects the sketch.
    [[nodiscard]] bool selectSketchProfileAt(document::FeatureId sketch,
                                             const std::array<double, 3>& point, bool additive);
    // M04: selects `sketch` with exactly these regions (the open extrusion panel's); view
    // state only.
    [[nodiscard]] bool selectSketchRegions(document::FeatureId sketch,
                                           std::span<const document::SketchRegionId> regions);
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
    // T01: faces a sketch can name on `support`'s current result (none when stale), and a sketch
    // bound to one in the face's frame as a pick would bind it; invalid_argument otherwise.
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
    // M01: a cylinder as a new, independent body at the origin (no input, separate).
    [[nodiscard]] CommandResult createNewBodyCylinder();
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
    // AI02c: a base-plane sketch and its extrusion in one undo step; regions resolve against
    // the new sketch, none fills nested contours alternately (outer, not holes, islands).
    [[nodiscard]] CommandResult
    createDatumSketchExtrusion(const document::SketchParameters& sketch,
                               std::span<const document::SketchRegionId> regions, double distance,
                               const ExtrusionMode& mode = {});
    // T04: push/pull of a planar face of `support`'s current result as one extrusion feature
    // (positive distance joins outward, negative cuts inward).
    [[nodiscard]] CommandResult pushPullFace(document::FeatureId support,
                                             const features::SupportOrigin& face, double distance);
    // M05: rounds edges of `support`'s current body ("Verrundung N"), one undo step.
    [[nodiscard]] CommandResult createFillet(document::FeatureId support,
                                             std::span<const document::FilletEdge> edges,
                                             double radius);
    [[nodiscard]] CommandResult
    editFilletParameters(document::FeatureId featureId,
                         const document::FilletParameters& parameters,
                         ErrorDisposition disposition = ErrorDisposition::reject);
    [[nodiscard]] document::FilletParameters filletParameters(document::FeatureId featureId) const;
    // M07: a symmetric chamfer ("Fase N"), one undo step, on the same named edges.
    [[nodiscard]] CommandResult createChamfer(document::FeatureId support,
                                              std::span<const document::FilletEdge> edges,
                                              double distance);
    [[nodiscard]] CommandResult
    editChamferParameters(document::FeatureId featureId,
                          const document::ChamferParameters& parameters,
                          ErrorDisposition disposition = ErrorDisposition::reject);
    [[nodiscard]] document::ChamferParameters
    chamferParameters(document::FeatureId featureId) const;
    // M08: a hole into a planar face of `support`'s current result ("Bohrung N"), at (x, y)
    // of the face's frame; no depth means through all. One undo step.
    [[nodiscard]] CommandResult createHole(document::FeatureId support,
                                           const document::SubshapeOrigin& face, double x, double y,
                                           double diameter, std::optional<double> depth);
    // N09: as above with the hole's size, depth and variant; its face is `face`.
    [[nodiscard]] CommandResult createHole(document::FeatureId support,
                                           const document::SubshapeOrigin& face,
                                           const document::HoleParameters& hole);
    // N09b: like `hole` at the points of a sketch on a body face (none given: all of them).
    [[nodiscard]] CommandResult createSketchHoles(document::FeatureId sketch,
                                                  const document::HoleParameters& hole);
    [[nodiscard]] CommandResult
    editHoleParameters(document::FeatureId featureId, const document::HoleParameters& parameters,
                       ErrorDisposition disposition = ErrorDisposition::reject);
    [[nodiscard]] document::HoleParameters holeParameters(document::FeatureId featureId) const;
    // M21: a Boolean of whole bodies ("Kombinieren N"), each body named by its current
    // result; one undo step. Unless kept, the tools are no separate bodies afterwards.
    [[nodiscard]] CommandResult createCombine(document::FeatureId target,
                                              std::span<const document::FeatureId> tools,
                                              document::CombineOperation operation, bool keepTools);
    [[nodiscard]] CommandResult
    editCombineParameters(document::FeatureId featureId,
                          const document::CombineParameters& parameters,
                          ErrorDisposition disposition = ErrorDisposition::reject);
    [[nodiscard]] document::CombineParameters
    combineParameters(document::FeatureId featureId) const;
    // M19: a construction plane on a base plane or a planar face, and a construction axis on a
    // base axis or an edge, of `support`'s current result. One undo step each.
    [[nodiscard]] CommandResult createDatumPlane(std::optional<document::DatumPlane> basePlane,
                                                 std::optional<document::FeatureId> support,
                                                 std::optional<document::SubshapeOrigin> face,
                                                 double offset,
                                                 document::DatumRotationAxis rotationAxis,
                                                 double angle);
    [[nodiscard]] CommandResult
    editDatumPlaneParameters(document::FeatureId featureId,
                             const document::DatumPlaneParameters& parameters,
                             ErrorDisposition disposition = ErrorDisposition::reject);
    [[nodiscard]] CommandResult createDatumAxis(std::optional<document::BaseAxis> baseAxis,
                                                std::optional<document::FeatureId> support,
                                                std::optional<document::FilletEdge> edge);
    [[nodiscard]] CommandResult
    editDatumAxisParameters(document::FeatureId featureId,
                            const document::DatumAxisParameters& parameters,
                            ErrorDisposition disposition = ErrorDisposition::reject);
    // M22: walls of `support`'s body ("Wandstärke N"), named faces opened; one undo step.
    [[nodiscard]] CommandResult createShell(document::FeatureId support,
                                            std::span<const document::SubshapeOrigin> faces,
                                            double thickness, bool outward);
    [[nodiscard]] CommandResult
    editShellParameters(document::FeatureId featureId, const document::ShellParameters& parameters,
                        ErrorDisposition disposition = ErrorDisposition::reject);
    [[nodiscard]] document::ShellParameters shellParameters(document::FeatureId featureId) const;
    // M25: tilts faces by `angle` (radians) about the neutral face's plane; one undo step.
    [[nodiscard]] CommandResult createDraft(document::FeatureId support,
                                            std::span<const document::SubshapeOrigin> faces,
                                            const document::SubshapeOrigin& neutral, double angle);
    [[nodiscard]] CommandResult
    editDraftParameters(document::FeatureId featureId, const document::DraftParameters& parameters,
                        ErrorDisposition disposition = ErrorDisposition::reject);
    [[nodiscard]] document::DraftParameters draftParameters(document::FeatureId featureId) const;
    // M09 (CadSessionMirror.cpp): mirrors `support`'s body about a base or construction plane or
    // a planar face of `faceSupport`'s result; a new body or joined. One undo step.
    [[nodiscard]] CommandResult createMirror(document::FeatureId support,
                                             std::optional<document::DatumPlane> basePlane,
                                             std::optional<document::FeatureId> constructionPlane,
                                             std::optional<document::FeatureId> faceSupport,
                                             std::optional<document::SubshapeOrigin> face,
                                             document::MirrorMode mode);
    [[nodiscard]] CommandResult
    editMirrorParameters(document::FeatureId featureId,
                         const document::MirrorParameters& parameters,
                         ErrorDisposition disposition = ErrorDisposition::reject);
    [[nodiscard]] document::MirrorParameters mirrorParameters(document::FeatureId featureId) const;
    // M10 (CadSessionPattern.cpp): copies of `support`'s body ("Muster N"); one undo step.
    [[nodiscard]] CommandResult createPattern(document::FeatureId support,
                                              const PatternInput& input);
    [[nodiscard]] CommandResult
    editPatternParameters(document::FeatureId featureId,
                          const document::PatternParameters& parameters,
                          ErrorDisposition disposition = ErrorDisposition::reject);
    [[nodiscard]] document::PatternParameters
    patternParameters(document::FeatureId featureId) const;
    // M11 (CadSessionMove.cpp): moves `feature`'s body, turns it by `angle` (radians) about axis
    // 0..2 through its middle: a last primitive or move changes, else "Bewegen N". One undo step.
    [[nodiscard]] CommandResult moveBody(document::FeatureId feature,
                                         const std::array<double, 3>& translation,
                                         std::optional<int> rotationAxis, double angle,
                                         ErrorDisposition disposition = ErrorDisposition::reject);
    // N02b (CadSessionGroupEdits.cpp): bodies moved (not turned) or steps suppressed together.
    [[nodiscard]] CommandResult moveBodies(std::span<const document::FeatureId> features,
                                           const std::array<double, 3>& translation,
                                           ErrorDisposition disposition = ErrorDisposition::reject);
    [[nodiscard]] CommandResult
    setFeaturesSuppressed(std::span<const document::FeatureId> features, bool suppressed,
                          ErrorDisposition disposition = ErrorDisposition::keepEditableErrors);
    [[nodiscard]] CommandResult
    editMoveParameters(document::FeatureId featureId, const document::MoveParameters& parameters,
                       ErrorDisposition disposition = ErrorDisposition::reject);
    [[nodiscard]] document::MoveParameters moveParameters(document::FeatureId featureId) const;
    // M17 (CadSessionStep.cpp): a STEP file as one import feature at the end of the history (or
    // at the rollback marker), one undo step; a file that gives no solid changes nothing.
    [[nodiscard]] StepImportResult importStep(const std::filesystem::path& path);
    // M18 (user decision 2026-10-16): a STEP file opened as a new document holding its import.
    // The file is read first; when it gives no solid the open document stays.
    [[nodiscard]] StepImportResult openStep(const std::filesystem::path& path);
    // N02 (CadSessionClipboard.cpp): bodies' current solids for the clipboard (invalid_argument
    // without one); a paste adds them as one fixed copy "Kopie N", one undo step.
    [[nodiscard]] std::vector<document::ImportedSolid>
    copyBodies(std::span<const document::BodyId> bodies) const;
    [[nodiscard]] CommandResult pasteBodies(std::vector<document::ImportedSolid> solids);
    static constexpr const char* pasteSource = "Zwischenablage"; // the import's file name
    // M17: writes `bodies` (none given: every visible body) as STEP in mm, each named as the
    // browser names it. Refused while the current revision has errors.
    [[nodiscard]] StepExportResult exportStep(const std::filesystem::path& path,
                                              std::span<const document::BodyId> bodies = {});
    // M14 (user decision 2026-10-16): suppresses or reactivates a feature, one undo step. Later
    // features that need what it made show their error; by default that is kept.
    [[nodiscard]] CommandResult
    setFeatureSuppressed(document::FeatureId featureId, bool suppressed,
                         ErrorDisposition disposition = ErrorDisposition::keepEditableErrors);
    // M14: the rollback marker after `featureId` (nullopt: at the end). No undo step and no
    // unsaved change; the model shows the state at the marker.
    [[nodiscard]] bool setRollback(std::optional<document::FeatureId> featureId);
    [[nodiscard]] std::optional<document::FeatureId> rollbackAfter() const noexcept;
    // W08: while a tool edits an earlier step (rolled back to it), commits and previews check
    // the history up to `marker` for errors, as without the rollback; nullopt ends it.
    void setEditCheck(std::optional<std::optional<document::FeatureId>> marker);
    // M16 (CadSessionHistoryEdit.cpp): moves a feature after `after` (or to the front); refused
    // when it would stand before what it needs or when a feature would fail. One undo step.
    [[nodiscard]] CommandResult moveFeature(document::FeatureId featureId,
                                            std::optional<document::FeatureId> after);
    [[nodiscard]] DeletionPlan deletionPlan(document::FeatureId featureId) const;
    // Deletes the features (a pattern's copies too); one undo step. Kept dependents fail and
    // stay editable when `disposition` keeps errors.
    [[nodiscard]] CommandResult
    deleteFeatures(std::span<const document::FeatureId> featureIds,
                   ErrorDisposition disposition = ErrorDisposition::reject);
    // M27 (CadSessionPrimitives.cpp): sphere, cone, torus or wedge ("Kugel N" ...) as a cylinder:
    // on `input` (join, cut, intersect, separate) or a new body at the origin; one undo step.
    [[nodiscard]] CommandResult createPrimitive(kernel::PrimitiveShape shape,
                                                std::optional<document::FeatureId> input,
                                                document::CylinderResultMode mode);
    [[nodiscard]] CommandResult
    editPrimitiveParameters(document::FeatureId featureId,
                            const document::PrimitiveParameters& parameters,
                            ErrorDisposition disposition = ErrorDisposition::reject);
    [[nodiscard]] std::optional<document::PrimitiveParameters> selectedPrimitiveParameters() const;
    // M27: the shape name in features ("Kugel", "Kegel", "Torus", "Keil").
    [[nodiscard]] static const char* primitiveShapeName(kernel::PrimitiveShape shape) noexcept;
    // S17: a sketch as the current evaluation solved it (projected edges up to date).
    [[nodiscard]] std::optional<document::SketchParameters>
    evaluatedSketch(document::FeatureId sketch) const;
    // The current frame of a construction plane or axis; none when it failed.
    [[nodiscard]] std::optional<document::SketchPlane>
    datumFrame(document::FeatureId feature) const;
    // The body a feature's current result belongs to; none for sketches and failed features.
    [[nodiscard]] std::optional<document::BodyId> resultBody(document::FeatureId feature) const;
    // M06: the named edge of `support`'s current result at a picked edge (between two named
    // faces), and back the result's edges of named edges, e.g. to highlight them.
    [[nodiscard]] std::optional<document::FilletEdge> filletEdge(document::FeatureId support,
                                                                 const TopoDS_Shape& edge) const;
    [[nodiscard]] std::vector<TopoDS_Shape>
    filletEdgeShapes(document::FeatureId support,
                     std::span<const document::FilletEdge> edges) const;
    // M01: a cylinder is always possible: on the selected body or as a new body.
    [[nodiscard]] bool canAddCylinder() const noexcept;
    // M01: the current producer of the selected feature's body (the input a join, cut or
    // intersect names); none without a selected body with a solid.
    [[nodiscard]] std::optional<document::FeatureId> selectedBodyProducer() const;
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
    // U10: for a blocked feature, the failed feature among its (indirect) inputs.
    [[nodiscard]] std::optional<document::FeatureId> errorCause(document::FeatureId feature) const;
    // PAR01 parameters, each command one undo step. A value or expression (N01) is mirrored into
    // every binding (bound sketches re-solved); an unsolvable result stays an editable error.
    [[nodiscard]] const document::ParameterTable& parameterTable() const noexcept;
    [[nodiscard]] std::size_t parameterUseCount(document::ParameterId id) const;
    [[nodiscard]] CommandResult createParameter(std::string name, document::ParameterUnit unit,
                                                double value, std::string comment = {},
                                                std::string expression = {});
    [[nodiscard]] CommandResult
    setParameterValue(document::ParameterId id, double value,
                      ErrorDisposition disposition = ErrorDisposition::keepEditableErrors);
    [[nodiscard]] CommandResult
    setParameterExpression(document::ParameterId id, std::string expression,
                           ErrorDisposition disposition = ErrorDisposition::keepEditableErrors);
    [[nodiscard]] CommandResult renameParameter(document::ParameterId id, std::string name);
    // M01: rename (one undo step; empty: numbered default), show or hide (stored, no undo step).
    // N06: the colour 0xRRGGBB, none for the theme's (one undo step).
    [[nodiscard]] CommandResult renameBody(document::BodyId body, std::string name);
    [[nodiscard]] bool setBodyHidden(document::BodyId body, bool hidden);
    [[nodiscard]] CommandResult setBodyColor(document::BodyId body, std::optional<std::uint32_t>);
    [[nodiscard]] CommandResult setBodyMaterial(document::BodyId body,
                                                std::optional<document::BodyMaterial> material);
    [[nodiscard]] CommandResult setParameterComment(document::ParameterId id, std::string comment);
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
    // N01b: binds (nullopt: unbinds) a field of document::featureField or an extrusion's
    // "distance"/"offset". Pending ones join the next feature command's undo step if it has them.
    [[nodiscard]] CommandResult bindFeatureField(document::FeatureId feature, std::string field,
                                                 std::optional<document::ParameterId> parameter);
    std::vector<FieldBindingRequest>& pendingFieldBindings() noexcept { return pendingFields_; }
    [[nodiscard]] bool canUndo() const noexcept;
    [[nodiscard]] bool canRedo() const noexcept;
    [[nodiscard]] CommandResult undo();
    [[nodiscard]] CommandResult redo();
    [[nodiscard]] bool isModified() const;
    void newDocument();
    void saveDocument(const std::filesystem::path& path);
    void loadDocument(const std::filesystem::path& path);
    // L05: the document as native bytes for a recovery snapshot; changes nothing.
    [[nodiscard]] std::string recoverySnapshot() const { return serializedDocument(); }
    // L05: features of the last saved state; nullopt for a document never saved.
    [[nodiscard]] std::optional<std::size_t> savedFeatureCount() const;
    // L05: a recovery snapshot as an unsaved document without a file (only Save As writes it,
    // the original stays); a snapshot of another document than `expected` is rejected.
    void restoreDocument(std::string_view bytes, document::DocumentId expected);
    // L05b: an open sketch draft for its own recovery file: this document with the draft as the
    // edited sketch or as a new sketch on its plane or support. The session stays unchanged.
    struct SketchDraftSnapshot final {
        std::string bytes;
        document::FeatureId feature;
    };
    [[nodiscard]] SketchDraftSnapshot
    sketchDraftSnapshot(std::optional<document::FeatureId> edited,
                        std::optional<document::FeatureId> support,
                        const document::SketchParameters& draft) const;
    struct SketchDraft final {
        document::SketchParameters sketch;
        std::optional<document::FeatureId> support;
    };
    [[nodiscard]] static SketchDraft readSketchDraft(std::string_view bytes,
                                                     document::FeatureId feature);
    // Provenance of the last successful import; editing/saving does not resolve its issues.
    [[nodiscard]] const std::optional<document::MigrationReport>&
    lastImportReport() const noexcept {
        return importReport_;
    }
    [[nodiscard]] const std::filesystem::path& currentFile() const noexcept { return currentFile_; }

  private:
    [[nodiscard]] std::size_t pasteCount() const; // N02: pastes so far, for "Kopie N"
    // N02b: one body of a group move applied to `candidate`; true when it changed.
    bool addGroupMove(document::ModelDocument& candidate, document::FeatureId source,
                      const std::array<double, 3>& translation, std::size_t& sequence);
    [[nodiscard]] StepImportResult addImport(const std::filesystem::path& path,
                                             kernel::StepImport read);
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
    [[nodiscard]] bool applyParameterTable(document::ModelDocument& candidate,
                                           document::ParameterTable table) const;
    [[nodiscard]] bool mirrorParameter(document::ModelDocument& candidate,
                                       document::ParameterId id) const;
    [[nodiscard]] bool recompute();
    [[nodiscard]] std::shared_ptr<const features::FeatureGraphEvaluation>
    checkedEvaluation(const document::DocumentSnapshot& snapshot,
                      std::shared_ptr<const features::FeatureGraphEvaluation> shown);
    void normalizeSelection() noexcept;
    [[nodiscard]] std::string serializedDocument() const;
    void loadFrom(std::istream& input, std::filesystem::path file, bool keepCleanState,
                  std::optional<document::DocumentId> expected = std::nullopt);

    document::ModelDocument document_;
    features::FeatureGraph featureGraph_;
    std::shared_ptr<const features::FeatureGraphEvaluation> evaluation_;
    std::optional<std::optional<document::FeatureId>> editCheck_; // W08
    std::shared_ptr<const features::FeatureGraphEvaluation> editCheckEvaluation_;
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
    std::vector<FieldBindingRequest> pendingFields_; // N01b
};

} // namespace voelund::application
