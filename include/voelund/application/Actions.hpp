#pragma once
#include <voelund/application/CadSession.hpp>
#include <voelund/application/CommandResult.hpp>
#include <voelund/application/SceneStatus.hpp>

#include <voelund/document/DocumentId.hpp>
#include <voelund/document/DocumentParameters.hpp>
#include <voelund/document/ModelDocument.hpp>
#include <voelund/document/SketchRegionId.hpp>

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
};
struct SetParameterValueAction final {
    document::ParameterId parameter;
    Quantity value;
};
struct RenameParameterAction final {
    document::ParameterId parameter;
    std::string name;
};
// nullopt unbinds and keeps the current value as a fixed one.
struct BindSketchDimensionAction final {
    document::FeatureId sketch;
    document::SketchConstraintId constraint;
    std::optional<document::ParameterId> parameter;
};
struct BindExtrusionDistanceAction final {
    document::FeatureId extrusion;
    std::optional<document::ParameterId> parameter;
};
// A sketch on a base plane (sketch.datum set); it opens a new body.
struct CreateDatumSketchAction final {
    document::SketchParameters sketch;
};
struct EditSketchAction final {
    document::FeatureId sketch;
    document::SketchParameters parameters;
};
struct CreateProfileExtrusionAction final {
    document::FeatureId sketch;
    std::vector<document::SketchRegionId> regions;
    Quantity distance;
};
struct SetExtrusionDistanceAction final {
    document::FeatureId extrusion;
    Quantity distance;
};
// AI02b: solids and placements. A box opens a new body.
struct CreateBoxAction final {
    Quantity width;
    Quantity depth;
    Quantity height;
};
struct SetBoxDimensionsAction final {
    document::FeatureId box;
    Quantity width;
    Quantity depth;
    Quantity height;
};
// The input needs a current result; `separate` opens a new body beside it.
struct CreateCylinderAction final {
    document::FeatureId input;
    document::CylinderResultMode mode;
};
struct SetCylinderSizeAction final {
    document::FeatureId cylinder;
    Quantity radius;
    Quantity height;
};
// Moves the feature that places this feature's result (CadSession::placementFeature).
struct SetPlacementAction final {
    document::FeatureId feature;
    Quantity x;
    Quantity y;
    Quantity z;
    Quantity rotationX;
    Quantity rotationY;
    Quantity rotationZ;
};

using Action = std::variant<CreateParameterAction, SetParameterValueAction, RenameParameterAction,
                            BindSketchDimensionAction, BindExtrusionDistanceAction,
                            CreateDatumSketchAction, EditSketchAction,
                            CreateProfileExtrusionAction, SetExtrusionDistanceAction,
                            CreateBoxAction, SetBoxDimensionsAction, CreateCylinderAction,
                            SetCylinderSizeAction, SetPlacementAction>;
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
    setPlacement
};
[[nodiscard]] ActionKind actionKind(const Action& action) noexcept;

struct ActionRequest final {
    std::string requestId;
    document::DocumentId document;
    DocumentStamp expected;
    Action action;
};

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
    duplicateRequest, // AI02c
    requestConflict   // AI02c
};

struct ActionResult final {
    CommandResult command;
    ActionRejection rejection{ActionRejection::none};
    DocumentStamp stamp; // The session stamp after the request.
    std::vector<document::FeatureId> createdFeatures;
    std::vector<document::FeatureId> changedFeatures;
    std::vector<document::ParameterId> createdParameters;
    std::vector<document::ParameterId> changedParameters;
};

// Validates and, if accepted, runs the request as exactly one CadSession command. A
// rejection leaves document, stamp and undo history unchanged.
[[nodiscard]] ActionResult executeAction(CadSession& session, const ActionRequest& request);

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
};
[[nodiscard]] ActionContext describeActionContext(const CadSession& session);

} // namespace voelund::application
