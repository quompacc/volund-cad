#pragma once
#include <voelund/application/CadSession.hpp>
#include <voelund/application/CommandResult.hpp>
#include <voelund/application/SceneStatus.hpp>

#include <voelund/document/DocumentId.hpp>
#include <voelund/document/DocumentParameters.hpp>
#include <voelund/document/ModelDocument.hpp>
#include <voelund/document/SketchRegionId.hpp>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

// AI02 (docs/implementation-plan/AI02-plan.md): one typed action layer for GUI, assistant
// and MCP. Every action names its targets explicitly; the GUI selection may prepare a
// request but never decides its target. In-process only, no wire schema.
namespace voelund::application {

// The CAD convention: lengths in millimetres, sketch angles in radians. Document
// parameters keep the unit the user enters, i.e. angle parameters are in degrees.
enum class QuantityUnit { millimetre, degree, radian };
struct Quantity final {
    double value;
    QuantityUnit unit;
    bool operator==(const Quantity&) const = default;
};

struct CreateParameterAction final {
    std::string name;
    document::ParameterUnit unit;
    Quantity value;
    std::string comment{};
    bool operator==(const CreateParameterAction&) const = default;
};
struct SetParameterValueAction final {
    document::ParameterId parameter;
    Quantity value;
    bool operator==(const SetParameterValueAction&) const = default;
};
struct RenameParameterAction final {
    document::ParameterId parameter;
    std::string name;
    bool operator==(const RenameParameterAction&) const = default;
};
// nullopt unbinds and keeps the current value as a fixed one.
struct BindSketchDimensionAction final {
    document::FeatureId sketch;
    document::SketchConstraintId constraint;
    std::optional<document::ParameterId> parameter;
    bool operator==(const BindSketchDimensionAction&) const = default;
};
struct BindExtrusionDistanceAction final {
    document::FeatureId extrusion;
    std::optional<document::ParameterId> parameter;
    bool offset{false}; // T03: binds the start offset instead of the distance
    bool operator==(const BindExtrusionDistanceAction&) const = default;
};
// A sketch on a base plane (sketch.datum set); it opens a new body.
struct CreateDatumSketchAction final {
    document::SketchParameters sketch;
    bool operator==(const CreateDatumSketchAction&) const = default;
};
struct EditSketchAction final {
    document::FeatureId sketch;
    document::SketchParameters parameters;
    bool operator==(const EditSketchAction&) const = default;
};
struct CreateProfileExtrusionAction final {
    document::FeatureId sketch;
    std::vector<document::SketchRegionId> regions;
    Quantity distance;
    ExtrusionMode mode{}; // T02
    bool operator==(const CreateProfileExtrusionAction&) const = default;
};
struct SetExtrusionDistanceAction final {
    document::FeatureId extrusion;
    Quantity distance;
    bool operator==(const SetExtrusionDistanceAction&) const = default;
};
// AI02b: solids and placements. A box opens a new body.
struct CreateBoxAction final {
    Quantity width;
    Quantity depth;
    Quantity height;
    bool operator==(const CreateBoxAction&) const = default;
};
struct SetBoxDimensionsAction final {
    document::FeatureId box;
    Quantity width;
    Quantity depth;
    Quantity height;
    bool operator==(const SetBoxDimensionsAction&) const = default;
};
// The input needs a current result; `separate` opens a new body beside it.
struct CreateCylinderAction final {
    // M01: the body's current feature to join, cut or intersect; none = a new body.
    std::optional<document::FeatureId> input;
    document::CylinderResultMode mode;
    bool operator==(const CreateCylinderAction&) const = default;
};
struct SetCylinderSizeAction final {
    document::FeatureId cylinder;
    Quantity radius;
    Quantity height;
    bool operator==(const SetCylinderSizeAction&) const = default;
};
// Moves the feature that places this feature's result (CadSession::placementFeature).
// AI02d: the shell's edits. A sketch on a body face carries its support binding; the full
// parameter edits mirror the property panel (lengths in mm, rotations in rad).
struct CreateSketchAction final {
    document::FeatureId support;
    document::SketchParameters sketch;
    bool operator==(const CreateSketchAction&) const = default;
};
struct EditBoxParametersAction final {
    document::FeatureId box;
    document::BoxParameters parameters;
    bool operator==(const EditBoxParametersAction&) const = default;
};
struct EditCylinderParametersAction final {
    document::FeatureId cylinder;
    document::CylinderParameters parameters;
    bool operator==(const EditCylinderParametersAction&) const = default;
};
// M27: a primitive on `input` (none = a new body, mode separate) and its full parameters.
struct CreatePrimitiveAction final {
    kernel::PrimitiveShape shape;
    std::optional<document::FeatureId> input;
    document::CylinderResultMode mode;
    bool operator==(const CreatePrimitiveAction&) const = default;
};
struct EditPrimitiveParametersAction final {
    document::FeatureId primitive;
    document::PrimitiveParameters parameters;
    bool operator==(const EditPrimitiveParametersAction&) const = default;
};
struct EditExtrusionParametersAction final {
    document::FeatureId extrusion;
    document::ExtrusionParameters parameters;
    bool operator==(const EditExtrusionParametersAction&) const = default;
};
// AI02c: a base-plane sketch and its extrusion in one transaction and one undo step. Empty
// `regions` fills nested contours alternately (a contour inside makes a hole, AI04a).
struct CreateSketchExtrusionAction final {
    document::SketchParameters sketch;
    std::vector<document::SketchRegionId> regions;
    Quantity distance;
    ExtrusionMode mode{}; // T02
    bool operator==(const CreateSketchExtrusionAction&) const = default;
};
// T04: push/pull. A planar face of `support`'s current result is swept along its outward
// normal: a positive distance pulls (joins), a negative one pushes into the body (cuts).
struct PushPullFaceAction final {
    document::FeatureId support;
    document::SubshapeOrigin face;
    Quantity distance;
    bool operator==(const PushPullFaceAction&) const = default;
};
// M05: rounds edges of `support`'s current body with one radius; each edge is named by its
// two adjacent faces (user decision 2026-10-12).
struct CreateFilletAction final {
    document::FeatureId support;
    std::vector<document::FilletEdge> edges;
    Quantity radius;
    bool operator==(const CreateFilletAction&) const = default;
};
struct EditFilletParametersAction final {
    document::FeatureId fillet;
    document::FilletParameters parameters;
    bool operator==(const EditFilletParametersAction&) const = default;
};
// M07: a symmetric chamfer of edges of `support`'s current body (user decision 2026-10-12).
struct CreateChamferAction final {
    document::FeatureId support;
    std::vector<document::FilletEdge> edges;
    Quantity distance;
    bool operator==(const CreateChamferAction&) const = default;
};
struct EditChamferParametersAction final {
    document::FeatureId chamfer;
    document::ChamferParameters parameters;
    bool operator==(const EditChamferParametersAction&) const = default;
};
// M08: a hole into a planar face of `support`'s current result at (x, y) of the face's
// frame; no depth means through all (user decision 2026-10-13).
struct CreateHoleAction final {
    document::FeatureId support;
    document::SubshapeOrigin face;
    Quantity x;
    Quantity y;
    Quantity diameter;
    std::optional<Quantity> depth;
    bool operator==(const CreateHoleAction&) const = default;
};
struct EditHoleParametersAction final {
    document::FeatureId hole;
    document::HoleParameters parameters;
    bool operator==(const EditHoleParametersAction&) const = default;
};
// M21: a Boolean of whole bodies, named by their current results (target and tools).
struct CreateCombineAction final {
    document::FeatureId target;
    std::vector<document::FeatureId> tools;
    document::CombineOperation operation{document::CombineOperation::join};
    bool keepTools{false};
    bool operator==(const CreateCombineAction&) const = default;
};
struct EditCombineParametersAction final {
    document::FeatureId combine;
    document::CombineParameters parameters;
    bool operator==(const EditCombineParametersAction&) const = default;
};
// M19: a construction plane on a base plane, or on a planar face of `support`'s current
// result; `offset` along its normal after turning by `angle` about the frame's x or y axis.
struct CreateDatumPlaneAction final {
    std::optional<document::DatumPlane> basePlane;
    std::optional<document::FeatureId> support;
    std::optional<document::SubshapeOrigin> face;
    Quantity offset;
    document::DatumRotationAxis rotationAxis{document::DatumRotationAxis::none};
    Quantity angle{0.0, QuantityUnit::radian};
    bool operator==(const CreateDatumPlaneAction&) const = default;
};
struct EditDatumPlaneParametersAction final {
    document::FeatureId plane;
    document::DatumPlaneParameters parameters;
    bool operator==(const EditDatumPlaneParametersAction&) const = default;
};
// M19: a construction axis on a base axis or on an edge of `support`'s current result.
struct CreateDatumAxisAction final {
    std::optional<document::BaseAxis> baseAxis;
    std::optional<document::FeatureId> support;
    std::optional<document::FilletEdge> edge;
    bool operator==(const CreateDatumAxisAction&) const = default;
};
struct EditDatumAxisParametersAction final {
    document::FeatureId axis;
    document::DatumAxisParameters parameters;
    bool operator==(const EditDatumAxisParametersAction&) const = default;
};
// M22: walls of `support`'s current body with the named faces opened.
struct CreateShellAction final {
    document::FeatureId support;
    std::vector<document::SubshapeOrigin> faces;
    Quantity thickness;
    bool outward{false};
    bool operator==(const CreateShellAction&) const = default;
};
struct EditShellParametersAction final {
    document::FeatureId shell;
    document::ShellParameters parameters;
    bool operator==(const EditShellParametersAction&) const = default;
};
// M25: a draft of faces of `support`'s body about the neutral face (angle in rad).
struct CreateDraftAction final {
    document::FeatureId support;
    std::vector<document::SubshapeOrigin> faces;
    document::SubshapeOrigin neutral;
    Quantity angle;
    bool operator==(const CreateDraftAction&) const = default;
};
struct EditDraftParametersAction final {
    document::FeatureId draft;
    document::DraftParameters parameters;
    bool operator==(const EditDraftParametersAction&) const = default;
};
// M09: the mirror image of `support`'s current body about a base plane, an existing
// construction plane or a planar face of `faceSupport`'s current result (exactly one).
struct CreateMirrorAction final {
    document::FeatureId support;
    std::optional<document::DatumPlane> basePlane;
    std::optional<document::FeatureId> constructionPlane;
    std::optional<document::FeatureId> faceSupport;
    std::optional<document::SubshapeOrigin> face;
    document::MirrorMode mode{document::MirrorMode::newBody};
    bool operator==(const CreateMirrorAction&) const = default;
};
struct EditMirrorParametersAction final {
    document::FeatureId mirror;
    document::MirrorParameters parameters;
    bool operator==(const EditMirrorParametersAction&) const = default;
};
// M10: copies of `support`'s current body along or about an axis.
struct CreatePatternAction final {
    document::FeatureId support;
    PatternInput input;
    bool operator==(const CreatePatternAction&) const = default;
};
struct EditPatternParametersAction final {
    document::FeatureId pattern;
    document::PatternParameters parameters;
    bool operator==(const EditPatternParametersAction&) const = default;
};
// M11: moves the body of `feature`, turned about X, Y or Z through its middle when
// `rotationAxis` is set (angle in rad).
struct MoveBodyAction final {
    document::FeatureId feature;
    Quantity x;
    Quantity y;
    Quantity z;
    std::optional<int> rotationAxis;
    Quantity angle{0.0, QuantityUnit::radian};
    bool operator==(const MoveBodyAction&) const = default;
};
struct EditMoveParametersAction final {
    document::FeatureId move;
    document::MoveParameters parameters;
    bool operator==(const EditMoveParametersAction&) const = default;
};
struct SetPlacementAction final {
    document::FeatureId feature;
    Quantity x;
    Quantity y;
    Quantity z;
    Quantity rotationX;
    Quantity rotationY;
    Quantity rotationZ;
    bool operator==(const SetPlacementAction&) const = default;
};

using Action = std::variant<CreateParameterAction, SetParameterValueAction, RenameParameterAction,
                            BindSketchDimensionAction, BindExtrusionDistanceAction,
                            CreateDatumSketchAction, EditSketchAction,
                            CreateProfileExtrusionAction, SetExtrusionDistanceAction,
                            CreateBoxAction, SetBoxDimensionsAction, CreateCylinderAction,
                            SetCylinderSizeAction, SetPlacementAction,
                            CreateSketchExtrusionAction, CreateSketchAction,
                            EditBoxParametersAction, EditCylinderParametersAction,
                            EditExtrusionParametersAction, PushPullFaceAction,
                            CreateFilletAction, EditFilletParametersAction,
                            CreateChamferAction, EditChamferParametersAction,
                            CreateHoleAction, EditHoleParametersAction,
                            CreateCombineAction, EditCombineParametersAction,
                            CreateDatumPlaneAction, EditDatumPlaneParametersAction,
                            CreateDatumAxisAction, EditDatumAxisParametersAction,
                            CreateShellAction, EditShellParametersAction,
                            CreatePrimitiveAction, EditPrimitiveParametersAction,
                            CreateDraftAction, EditDraftParametersAction,
                            CreateMirrorAction, EditMirrorParametersAction,
                            CreatePatternAction, EditPatternParametersAction,
                            MoveBodyAction, EditMoveParametersAction>;
// Parallel to the alternatives of Action.
enum class ActionKind {
    createParameter,
    setParameterValue,
    renameParameter,
    bindSketchDimension,
    bindExtrusionDistance,
    createDatumSketch,
    editSketch,
    createProfileExtrusion,
    setExtrusionDistance,
    createBox,
    setBoxDimensions,
    createCylinder,
    setCylinderSize,
    setPlacement,
    createSketchExtrusion,
    createSketch,
    editBoxParameters,
    editCylinderParameters,
    editExtrusionParameters,
    pushPullFace,
    createFillet,
    editFilletParameters,
    createChamfer,
    editChamferParameters,
    createHole,
    editHoleParameters,
    createCombine,
    editCombineParameters,
    createDatumPlane,
    editDatumPlaneParameters,
    createDatumAxis,
    editDatumAxisParameters,
    createShell,
    editShellParameters,
    createPrimitive, // M27
    editPrimitiveParameters,
    createDraft, // M25
    editDraftParameters,
    createMirror, // M09
    editMirrorParameters,
    createPattern, // M10
    editPatternParameters,
    moveBody, // M11
    editMoveParameters
};
[[nodiscard]] ActionKind actionKind(const Action& action) noexcept;

struct ActionRequest final {
    std::string requestId;
    document::DocumentId document;
    DocumentStamp expected;
    Action action;
    // How an evaluation error is handled; nullopt keeps the command's default (parameter
    // edits and bindings keep editable errors, everything else rejects).
    std::optional<ErrorDisposition> disposition{};
    bool operator==(const ActionRequest&) const = default;
};
// AI02d: a request of a local caller (the shell) on the current stamp or on the stamp a
// tool stored when it started. Local requests are not repeated, so they share one ID.
[[nodiscard]] ActionRequest prepareRequest(const CadSession& session, Action action,
                                           std::optional<DocumentStamp> stamp = std::nullopt,
                                           std::optional<ErrorDisposition> disposition = std::nullopt);

// Checked before any transaction, in this order (AI01). `none` leaves the decision to the
// command itself; see ActionResult::command.
enum class ActionRejection {
    none,
    foreignDocument,
    staleStamp,
    unknownTarget,
    unsupportedTarget, // The target exists but does not offer this action.
    invalidUnit,
    nonFinite,
    unresolvedReference,
    missingRequestId, // ActionDispatcher only.
    requestConflict   // The request ID was already used for other content.
};

struct ActionResult final {
    CommandResult command;
    ActionRejection rejection{ActionRejection::none};
    DocumentStamp stamp; // The session stamp after the request.
    std::vector<document::FeatureId> createdFeatures;
    std::vector<document::FeatureId> changedFeatures;
    std::vector<document::ParameterId> createdParameters;
    std::vector<document::ParameterId> changedParameters;
    // ActionDispatcher: the stored result of an earlier, identical request; nothing ran again.
    bool repeated{false};
};

// Validates and, if accepted, runs the request as exactly one CadSession command. A
// rejection leaves document, stamp and undo history unchanged.
[[nodiscard]] ActionResult executeAction(CadSession& session, const ActionRequest& request);

// AI02c: runs the request on an isolated copy of the session. The active document, its
// stamp and history stay unchanged; `result.stamp` is the unchanged session stamp. A later
// execute of the same request revalidates against the then current stamp.
struct ActionPreview final {
    ActionResult result;
    std::vector<features::EvaluatedBody> bodies; // Valid bodies of the candidate.
    // M06 (user report 2026-10-12): the candidate itself, read-only, e.g. to name an edge
    // picked on the previewed body.
    std::shared_ptr<const CadSession> candidate{};
};
[[nodiscard]] ActionPreview previewAction(const CadSession& session, const ActionRequest& request);

// AI02c: executeAction with protection against repetition. A request ID seen before with
// identical content returns the stored result (`repeated`); with other content it is
// rejected (`requestConflict`). The table holds the last `capacity` requests of the
// current document generation.
class ActionDispatcher final {
  public:
    static constexpr std::size_t capacity = 256;
    explicit ActionDispatcher(CadSession& session) : session_(session) {}
    [[nodiscard]] ActionResult execute(const ActionRequest& request);
    [[nodiscard]] ActionPreview preview(const ActionRequest& request) const {
        return previewAction(session_, request);
    }

  private:
    struct Entry final {
        ActionRequest request;
        ActionResult result;
    };
    CadSession& session_;
    std::uint64_t generation_{0};
    std::deque<Entry> entries_;
};

// Read-only context for a caller that prepares requests.
struct ActionUnits final {
    QuantityUnit length{QuantityUnit::millimetre};
    QuantityUnit sketchAngle{QuantityUnit::radian};
    QuantityUnit angleParameter{QuantityUnit::degree};
};
struct SketchRegionSummary final {
    document::SketchRegionId id;
    double area;    // mm²
    bool ambiguous; // Shares its signature with another cell; never resolved by ID.
};
struct SketchRegionsSummary final {
    document::FeatureId sketch;
    bool valid; // False when region construction failed; then `regions` is empty.
    std::vector<SketchRegionSummary> regions;
};
// The document itself (createParameter, createDatumSketch, createBox), a feature or a
// parameter.
using ActionTarget = std::variant<std::monostate, document::FeatureId, document::ParameterId>;
struct ActionAvailability final {
    ActionTarget target;
    std::vector<ActionKind> actions;
};
// T01: the faces of a body's latest result that create_sketch_on_face can name.
struct BodyFaces final {
    document::BodyId body;
    document::FeatureId support; // The body's latest producer; the sketch's support.
    std::vector<features::SupportFaceFrame> faces;
};
struct ActionContext final {
    document::DocumentId document;
    DocumentStamp stamp;
    ActionUnits units;
    std::vector<document::FeatureDescriptor> features;
    ModelOutline outline;
    std::vector<FeatureRuntimeState> featureStates;
    document::ParameterTable parameters;
    std::vector<SketchRegionsSummary> sketchRegions;
    std::vector<ActionAvailability> available;
    std::vector<BodyFaces> bodyFaces;
};
[[nodiscard]] ActionContext describeActionContext(const CadSession& session);

} // namespace voelund::application
