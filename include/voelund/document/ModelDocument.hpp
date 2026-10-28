// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <TopoDS_Shape.hxx>
#include <array>
#include <voelund/document/BodyAttributes.hpp>
#include <voelund/document/DocumentId.hpp>
#include <voelund/document/DocumentParameters.hpp>
#include <voelund/document/FeatureId.hpp>
#include <voelund/document/SketchGeometry.hpp>
#include <voelund/document/SketchPattern.hpp>
#include <voelund/document/SketchRegionId.hpp>
#include <voelund/document/SubshapeReference.hpp>
#include <voelund/kernel/ShapeFactory.hpp>
#include <voelund/kernel/SolidPrimitives.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace voelund::document {

class DocumentSerializer;
class DocumentTransaction;
class DocumentSnapshot;

enum class FeatureKind {
    box,
    cylinder,
    sketch,
    extrusion,
    fillet,
    chamfer,
    hole,
    combine,
    datumPlane,
    datumAxis,
    shell,
    primitive, // M27: sphere, cone, torus, wedge
    draft,     // M25
    mirror,    // M09
    pattern,   // M10
    patternCopy,
    move,       // M11
    stepImport, // M17
    importPart
};
enum class CylinderResultMode { join, cut, intersect, separate };

struct BoxParameters final {
    kernel::BoxDimensions dimensions;
    kernel::RigidTransform placement;

    [[nodiscard]] bool operator==(const BoxParameters&) const = default;
};

struct CylinderParameters final {
    double radius;
    double height;
    kernel::RigidTransform placement;
    CylinderResultMode resultMode;

    [[nodiscard]] bool operator==(const CylinderParameters&) const = default;
};

// M27 (schema 25, user decision 2026-10-14): a further primitive solid, joined to, cut from or
// intersected with its input body like a cylinder, or a body of its own. `sizes` follow
// kernel::SolidPrimitiveDefinition (first .. fourth).
struct PrimitiveParameters final {
    kernel::PrimitiveShape shape;
    std::array<double, 4> sizes;
    kernel::RigidTransform placement;
    CylinderResultMode resultMode;

    [[nodiscard]] kernel::SolidPrimitiveDefinition definition() const noexcept {
        return {shape, sizes[0], sizes[1], sizes[2], sizes[3]};
    }
    [[nodiscard]] bool operator==(const PrimitiveParameters&) const = default;
};

struct SketchPlane final {
    double originX;
    double originY;
    double originZ;
    double normalX;
    double normalY;
    double normalZ;
    double xDirectionX;
    double xDirectionY;
    double xDirectionZ;

    [[nodiscard]] bool operator==(const SketchPlane&) const = default;
};

struct SketchSupportBinding final {
    SubshapeRef reference;
    // Sketch frame expressed in the semantic source face's moving frame.
    SketchPlane localPlane;
    bool operator==(const SketchSupportBinding&) const = default;
};

enum class SketchRepairReason { legacySupport = 1, legacyEdges = 2 };
// V01: document base planes with stable identity. A sketch on one of them needs no support
// body and opens a new body whose BodyId is the sketch's FeatureId.
enum class DatumPlane { xy = 1, xz = 2, yz = 3 };
// Canonical frames: XY normal +Z; XZ x along +X, sketch y along +Z (normal -Y); YZ x along
// +Y, sketch y along +Z (normal +X). All pass through the origin.
[[nodiscard]] SketchPlane datumSketchPlane(DatumPlane datum);
// M05 (schema 14, user decision 2026-10-12): an edge of a body, named by its two adjacent
// faces. Their names follow the faces through later Booleans; the edge is where they meet.
struct FilletEdge final {
    SubshapeOrigin first;
    SubshapeOrigin second;
    // Schema 17: unique and nonzero within its feature, kept when other edges are removed;
    // the faces the edge generates are named by it (SubshapeOrigin::treatedEdge).
    std::uint64_t id{0};
    [[nodiscard]] bool operator==(const FilletEdge&) const = default;
};
inline constexpr std::size_t maximumFilletEdges = 256;
// S17 (schema 23, user decision 2026-10-14): a body edge projected onto the sketch plane. Its
// element is construction geometry (a line, circle or arc), follows the edge on every
// evaluation and is fixed in the solver; dimensions and snaps use it like any element.
struct SketchProjection final {
    SketchElementId element;
    BodyId body;
    FilletEdge edge;
    [[nodiscard]] bool operator==(const SketchProjection&) const = default;
};
inline constexpr std::size_t maximumSketchProjections = 256;
struct SketchParameters;
// Each projection names its own construction line, circle or arc of the sketch and a body edge.
void validateSketchProjections(const SketchParameters& parameters);
struct SketchParameters final {
    SketchPlane plane;
    SketchElements elements;
    SketchConstraints constraints;
    std::optional<SketchSupportBinding> supportBinding{};
    std::optional<SketchRepairReason> repairReason{};
    // Set exactly for sketches without a support feature; the plane equals the datum's.
    std::optional<DatumPlane> datum{};
    // S12: associative patterns; their instances are derived from the sources.
    SketchPatterns patterns{};
    // M19 (schema 19): a sketch on a construction plane (a datumPlane feature). Like a
    // base-plane sketch it has no support and opens a body; its plane follows the feature.
    std::optional<FeatureId> constructionPlane{};
    // S17: projected body edges.
    std::vector<SketchProjection> projections{};

    SketchParameters() = default;
    SketchParameters(SketchPlane sketchPlane, SketchElements sketchElements,
                     SketchConstraints sketchConstraints = {})
        : plane(sketchPlane), elements(std::move(sketchElements)),
          constraints(std::move(sketchConstraints)) {}

    [[nodiscard]] bool operator==(const SketchParameters&) const = default;
};

void validateSketchParameters(const SketchParameters& parameters);
void validateCylinderParameters(const CylinderParameters& parameters);
void validatePrimitiveParameters(const PrimitiveParameters& parameters); // M27

// T03: a planar face of one of this document's bodies, named by its semantic origin.
struct BodyFaceReference final {
    BodyId body;
    SubshapeOrigin origin;
    [[nodiscard]] bool operator==(const BodyFaceReference&) const = default;
};

// T02: what an extrusion's prism does to a body. M02 (schema 13): intersect keeps only what
// the prism and the target body have in common.
enum class ExtrusionOperation { newBody = 1, join = 2, cut = 3, intersect = 4 };

enum class BaseAxis { x = 1, y = 2, z = 3 };
// M20 (schema 20, user decision 2026-10-14): the regions turn about an axis instead of being
// swept along the sketch normal: a line of the same sketch, a base axis or a construction
// axis (exactly one). `angle` in (0, 2 pi]; a full turn has no start and end faces. Reversed
// turns the other way; symmetric splits the angle around the sketch plane.
struct RevolveAxis final {
    std::optional<SketchElementId> sketchLine{};
    std::optional<BaseAxis> baseAxis{};
    std::optional<FeatureId> datumAxis{};
    [[nodiscard]] bool operator==(const RevolveAxis&) const = default;
};
struct RevolveParameters final {
    RevolveAxis axis;
    kernel::Radians angle;
    // M26 (schema 28): a helix ("Wendel") advances this far along the axis per turn; `angle`
    // is then the whole turn (up to 1000 turns), reversed means left-handed, never symmetric.
    std::optional<double> pitch{};
    [[nodiscard]] bool operator==(const RevolveParameters&) const = default;
};
void validateRevolveParameters(const RevolveParameters& parameters);
// M23 (schema 26, user decision 2026-10-14): a sweep ("Folgen") along connected elements of an
// earlier sketch, which form one chain (open or closed).
struct SweepPath final {
    FeatureId sketch;
    std::vector<SketchElementId> elements;
    [[nodiscard]] bool operator==(const SweepPath&) const = default;
};
inline constexpr std::size_t maximumSweepPathElements = 256;
void validateSweepPath(const SweepPath& path);
// M24 (schema 27, user decision 2026-10-14): a loft ("Ausformung") from the extrusion's one
// region through one region of each further sketch, in order; ruled or smooth.
struct LoftSection final {
    FeatureId sketch;
    SketchRegionId region;
    [[nodiscard]] bool operator==(const LoftSection&) const = default;
};
struct LoftParameters final {
    std::vector<LoftSection> sections;
    bool ruled{false};
    [[nodiscard]] bool operator==(const LoftParameters&) const = default;
};
inline constexpr std::size_t maximumLoftSections = 15;
void validateLoftParameters(const LoftParameters& parameters);
struct ExtrusionParameters;
// The sweep (M23) or loft (M24) part of an extrusion: valid, alone, and nothing else a prism
// uses.
void validateSweepExtrusion(const ExtrusionParameters& parameters);
// W16: a taper only leans a plain prism: below 89 degrees, no revolve, sweep, loft, face
// profile or symmetric extent.
void validateTaperExtrusion(const ExtrusionParameters& parameters);

struct ExtrusionParameters final {
    double distance;
    // Pre-R09 selection by minimum loop element ID. Kept only until Open migrates it to
    // regions; a remaining entry means the selection could not be mapped and needs repair.
    std::vector<SketchProfileId> profiles;
    // Explicit R09 region selection. Both lists empty selects every closed profile.
    std::vector<SketchRegionId> regions{};
    // PAR01: the distance mirrors this length parameter while bound.
    std::optional<ParameterId> distanceParameter{};
    // T02 (schema 8). Unset keeps the earlier rule: the prism of a base-plane sketch opens
    // the sketch's body, the prism of a face sketch joins its support's body.
    std::optional<ExtrusionOperation> operation{};
    // Extrude against the sketch normal, e.g. a cut into the face the sketch lies on.
    bool reversed{false};
    // M02 (schema 13, user decision 2026-10-11): `distance` is the total length, centred on
    // the sketch plane (8 gives -4..+4). Not combined with reversed, offset or an end face.
    bool symmetric{false};
    // Join, cut or intersect of a base-plane sketch names the body it works on (its creating
    // feature's ID). A face sketch always works on its support's body and names none.
    std::optional<BodyId> target{};
    // T03 (schema 9): the prism starts this far along its direction from the sketch plane
    // (mm, may be negative); bindable like the distance.
    double offset{0.0};
    std::optional<ParameterId> offsetParameter{};
    // T03: the prism ends on this face, parallel to the sketch, instead of after `distance`
    // (which stays stored as the last value).
    std::optional<BodyFaceReference> upToFace{};
    // T04 (schema 10): push/pull. The profile is this planar body face instead of a sketch;
    // the extrusion then has no input feature, sweeps along the face's outward normal
    // (into the body when reversed) and joins or cuts the face's body.
    std::optional<BodyFaceReference> faceProfile{};
    // M20: a revolve ("Drehen", a groove when it cuts). It keeps regions, operation,
    // target, reversed and symmetric; distance stays stored but unused; no offset, end face or
    // face profile.
    std::optional<RevolveParameters> revolve{};
    // M23: a sweep along a path instead of a prism; keeps regions, operation and target;
    // distance stays stored but unused; no direction, offset, end face, revolve or face profile.
    std::optional<SweepPath> sweep{};
    // M24: a loft through further sections instead of a prism; its own regions name exactly one
    // region, the first section; like a sweep otherwise.
    std::optional<LoftParameters> loft{};
    // T05 (schema 36, user decision 2026-10-17): a push/pull that joins leaves no seam where it
    // continues a face of its body. New push/pulls set it; older ones read without it and stay
    // as they were.
    bool mergeSeams{false};
    // W16 (schema 40, user request 2026-10-25 as FreeCAD's "Schrägungswinkel"): degrees the
    // sides lean, positive wider towards the end, negative narrower; 0 is a straight prism.
    double taperAngle{0.0};

    [[nodiscard]] bool operator==(const ExtrusionParameters&) const = default;
};
// M05: one constant radius on edges of one body; one feature, one undo step.
struct FilletParameters final {
    BodyId body;
    std::vector<FilletEdge> edges;
    double radius;
    [[nodiscard]] bool operator==(const FilletParameters&) const = default;
};
void validateFilletParameters(const FilletParameters& parameters);
// M07 (schema 15, user decision 2026-10-12): a symmetric chamfer, `distance` on both faces of
// each edge; edges named as for the fillet.
// N10 (schema 46): equal distances, two distances (`distance` on the first face of each edge,
// `second` on the other) or a distance and an angle (`second` in degrees, between 0 and 90,
// from the first face); `flipped` measures from each edge's second face instead.
enum class ChamferMode { equal = 0, twoDistances = 1, distanceAngle = 2 };
struct ChamferShape final {
    ChamferMode mode{ChamferMode::equal};
    double second{0.0};
    bool flipped{false};
    [[nodiscard]] bool operator==(const ChamferShape&) const = default;
};
struct ChamferParameters final {
    BodyId body;
    std::vector<FilletEdge> edges;
    double distance;
    ChamferShape shape{}; // N10
    [[nodiscard]] bool operator==(const ChamferParameters&) const = default;
};
void validateChamferParameters(const ChamferParameters& parameters);
// M08 (schema 16, user decision 2026-10-13): a cylindrical hole into a planar body face. It
// sits at (x, y) in the face's frame (the frame a sketch on that face gets) and runs along
// the inward normal, `depth` deep with a flat bottom, or through the whole body without one.
// N09 (schema 43): a counterbore (a wider cylinder of `headDepth` at the entrance) or a
// countersink (a cone from `headDiameter` with `countersinkAngle` degrees); a blind hole can
// end in a 118° drill point (depth to the shoulder). `thread` ("M6", HoleThreads.hpp) marks a
// tapped hole: `diameter` is then its core drill, the thread is an attribute only.
enum class HoleKind { simple = 0, counterbore = 1, countersink = 2 };
inline constexpr double drillPointAngle = 118.0; // degrees
struct HoleVariant final {
    HoleKind kind{HoleKind::simple};
    double headDiameter{0.0};
    double headDepth{0.0};
    double countersinkAngle{90.0};
    bool drillPoint{false};
    std::string thread{};
    [[nodiscard]] bool operator==(const HoleVariant&) const = default;
    // A hole whose shape is M08's plain cylinder (its faces keep their names).
    [[nodiscard]] bool plain() const noexcept { return kind == HoleKind::simple && !drillPoint; }
};
// N09b (schema 44): holes at points of the feature's input sketch (a sketch on `face`), one per
// point, all alike; x and y are then unused. Without points one hole at (x, y).
inline constexpr std::size_t maximumHolePoints = 100;
// N09c (schema 45): the hole's centre at `distance` from a straight edge of its body, measured
// in the plane of its face (with a second such edge it is fixed); x and y are then unused.
struct HoleEdgeOffset final {
    FilletEdge edge;
    double distance;
    [[nodiscard]] bool operator==(const HoleEdgeOffset&) const = default;
};
struct HoleParameters final {
    BodyFaceReference face;
    double x;
    double y;
    double diameter;
    std::optional<double> depth{};
    HoleVariant variant{}; // N09
    std::vector<SketchElementId> points{};
    std::vector<HoleEdgeOffset> offsets{}; // N09c: none or two
    [[nodiscard]] bool operator==(const HoleParameters&) const = default;
};
void validateHoleParameters(const HoleParameters& parameters);
// M21 (schema 18, user decision 2026-10-14): a Boolean of whole bodies. The target keeps its
// identity and takes the result; the tools are consumed unless kept, and stay editable through
// their own features.
enum class CombineOperation { join = 1, cut = 2, intersect = 3 };
inline constexpr std::size_t maximumCombineTools = 64;
struct CombineParameters final {
    BodyId target;
    std::vector<BodyId> tools;
    CombineOperation operation{CombineOperation::join};
    bool keepTools{false};
    [[nodiscard]] bool operator==(const CombineParameters&) const = default;
};
void validateCombineParameters(const CombineParameters& parameters);
// M19 (schema 19, user decision 2026-10-14): a construction plane `offset` along the normal
// of its reference, a base plane or a planar body face. A rotation first turns the reference
// frame by `angle` about its x or y axis. It belongs to no body.
enum class DatumRotationAxis { none = 0, x = 1, y = 2 };
struct DatumPlaneParameters final {
    std::optional<DatumPlane> basePlane{};
    std::optional<BodyFaceReference> face{};
    double offset{0.0};
    DatumRotationAxis rotationAxis{DatumRotationAxis::none};
    kernel::Radians angle{};
    [[nodiscard]] bool operator==(const DatumPlaneParameters&) const = default;
};
void validateDatumPlaneParameters(const DatumPlaneParameters& parameters);
// M19: a construction axis, a base axis or the line of a body edge: a straight edge itself, a
// circular edge's axis through its centre. It belongs to no body.
struct DatumAxisParameters final {
    std::optional<BaseAxis> baseAxis{};
    std::optional<BodyId> body{};
    std::optional<FilletEdge> edge{};
    [[nodiscard]] bool operator==(const DatumAxisParameters&) const = default;
};
void validateDatumAxisParameters(const DatumAxisParameters& parameters);
// M22 (schema 21, user decision 2026-10-14): hollows a body to walls `thickness` thick and
// opens the named faces (at least one). Inward keeps the outer size, outward grows the body.
inline constexpr std::size_t maximumShellFaces = 256;
struct ShellParameters final {
    BodyId body;
    std::vector<SubshapeOrigin> openFaces;
    double thickness;
    bool outward{false};
    [[nodiscard]] bool operator==(const ShellParameters&) const = default;
};
void validateShellParameters(const ShellParameters& parameters);
// M25 (schema 29, user decision 2026-10-14): a draft ("Formschräge") tilts the named faces of a
// body by `angle` about their line on the plane of the neutral face, pulled along its normal.
inline constexpr std::size_t maximumDraftFaces = 256;
struct DraftParameters final {
    BodyId body;
    std::vector<SubshapeOrigin> faces;
    SubshapeOrigin neutral;
    kernel::Radians angle;
    [[nodiscard]] bool operator==(const DraftParameters&) const = default;
};
void validateDraftParameters(const DraftParameters& parameters);
// M09 (schema 31, user decision 2026-10-15): mirrors a body about a plane: a base plane, an
// earlier construction plane or a planar body face (exactly one). A new body keeps the
// original and makes the mirror image a body of its own; join fuses the image with the
// original. Mirrored faces carry the copy step (SubshapeOrigin::instances).
enum class MirrorMode { newBody = 1, join = 2 };
struct MirrorPlane final {
    std::optional<DatumPlane> basePlane{};
    std::optional<FeatureId> constructionPlane{};
    std::optional<BodyFaceReference> face{};
    [[nodiscard]] bool operator==(const MirrorPlane&) const = default;
};
struct MirrorParameters final {
    BodyId body;
    MirrorPlane plane;
    MirrorMode mode{MirrorMode::newBody};
    // N11b (schema 48): when set, the mirror repeats what these earlier holes and extrusions did
    // to `body`, mirrored, instead of the whole body; it is always joined.
    std::vector<FeatureId> features{};
    [[nodiscard]] bool operator==(const MirrorParameters&) const = default;
};
void validateMirrorParameters(const MirrorParameters& parameters);
// N11b: the steps a pattern or mirror repeats: 1 to 20 distinct ones, the result joined.
void validateRepeatedSteps(std::span<const FeatureId> steps, bool joined);
// M10 (schema 32, user decision 2026-10-15): copies of a body along a direction (linear) or
// about an axis (circular). The direction or axis is a base axis or a body edge (as for a
// construction axis) or an earlier construction axis (exactly one). `count` includes the
// original. Linear copies stand `spacing` apart; circular ones share `angle`: a full turn
// evenly without a doubled end, a part with both ends included. As new bodies every copy is
// a patternCopy feature of its own body, kept in step with the pattern by the document; joined
// they fuse with the original. Copied faces carry {pattern, index}.
enum class PatternKind { linear = 1, circular = 2, path = 3 };
enum class PatternMode { newBodies = 1, join = 2 };
inline constexpr std::uint32_t maximumPatternCount = 100;
inline constexpr std::size_t maximumPatternFeatures = 20; // N11b
// N11a (schema 47): the second direction of a grid ("Rastermuster"), along an axis as above;
// `count` rows `spacing` apart, including the first.
struct PatternDirection final {
    DatumAxisParameters axis{};
    std::optional<FeatureId> datumAxis{};
    bool reversed{false};
    std::uint32_t count{2};
    double spacing{10.0};
    [[nodiscard]] bool operator==(const PatternDirection&) const = default;
};
struct PatternParameters final {
    BodyId body;
    PatternKind kind{PatternKind::linear};
    DatumAxisParameters axis{};
    std::optional<FeatureId> datumAxis{};
    bool reversed{false};
    std::uint32_t count{2};
    double spacing{10.0};
    kernel::Radians angle{0.0};
    PatternMode mode{PatternMode::newBodies};
    // N11a: a linear pattern's second direction; instance i + count * j stands i steps along
    // the first and j along the second. Skipped instances (ascending, never 0) are left out.
    std::optional<PatternDirection> second{};
    std::vector<std::uint32_t> skipped{};
    // N11b (schema 48): when set, the pattern repeats what these earlier holes and extrusions
    // (join or cut) did to `body` instead of copying the whole body; it is always joined.
    std::vector<FeatureId> features{};
    // N11c (schema 49): a pattern along a curve runs along the elements of an earlier sketch
    // instead of an axis; copy k stands k * spacing along it from its start (round a closed
    // one), turned with its direction when `followPath`.
    std::optional<SweepPath> path{};
    bool followPath{false};
    [[nodiscard]] bool operator==(const PatternParameters&) const = default;
};
void validatePatternParameters(const PatternParameters& parameters);
// All instances of the pattern, the original and the skipped ones included.
[[nodiscard]] std::uint32_t patternInstances(const PatternParameters& parameters) noexcept;
[[nodiscard]] bool patternSkips(const PatternParameters& parameters, std::uint32_t index);
// M11 (schema 33, user decision 2026-10-15): moves and turns a body where it stands in the
// history ("Bewegen"): turned about `pivot` like a primitive's placement (about X, then Y, then
// Z), then shifted. The pivot is the middle of the body when the move was made.
struct MoveParameters final {
    BodyId body;
    kernel::RigidTransform transform;
    std::array<double, 3> pivot;
    [[nodiscard]] bool operator==(const MoveParameters&) const = default;
};
void validateMoveParameters(const MoveParameters& parameters);
// One copy of a new-body pattern: its own body, `index` steps from the original.
struct PatternCopyParameters final {
    FeatureId pattern;
    std::uint32_t index;
    [[nodiscard]] bool operator==(const PatternCopyParameters&) const = default;
};
// M17 (schema 35, user decision 2026-10-16): the solids of a STEP file, embedded in the
// document in mm; the file is not needed again. The import feature is the body of the first
// solid; every further solid is an importPart feature of its own body that moves, is deleted and
// is shown with the import. Faces are named by their index in the stored solid.
struct ImportedSolid final {
    std::string name;                        // the STEP product's name, UTF-8; may be empty
    std::shared_ptr<const std::string> brep; // OCCT binary BRep as stored
    TopoDS_Shape solid;                      // read from `brep`
    [[nodiscard]] bool operator==(const ImportedSolid& other) const {
        return name == other.name &&
               (brep == other.brep || (brep && other.brep && *brep == *other.brep));
    }
};
// The stored form of a solid, and the solid as read back from it, so an import evaluates as it
// will after saving. Throws std::invalid_argument for data that is no valid solid.
[[nodiscard]] ImportedSolid importedSolid(const TopoDS_Shape& solid, std::string name);
[[nodiscard]] ImportedSolid importedSolid(std::string brep, std::string name);
inline constexpr std::size_t maximumImportedSolids = 1000;
struct ImportParameters final {
    std::string fileName{};   // where it came from, for information only
    std::string sourceUnit{}; // the file's length unit as it names it
    std::vector<ImportedSolid> solids;
    [[nodiscard]] bool operator==(const ImportParameters&) const = default;
};
void validateImportParameters(const ImportParameters& parameters);
// One further solid of an import: solid `index` (from 1) of it, a body of its own.
struct ImportPartParameters final {
    FeatureId import;
    std::uint32_t index;
    [[nodiscard]] bool operator==(const ImportPartParameters&) const = default;
};

// The operation in effect, with the earlier rule for an unset one.
[[nodiscard]] ExtrusionOperation effectiveOperation(const ExtrusionParameters& parameters,
                                                    bool baseSketch) noexcept;

struct FeatureDescriptor final {
    FeatureId id;
    FeatureKind kind;
    std::string name;
    std::optional<FeatureId> inputFeature;
    // M14 (schema 34, user decision 2026-10-16): skipped in evaluation; later features work on
    // the body without it. Not a visibility switch and not a deletion.
    bool suppressed{false};
    [[nodiscard]] bool operator==(const FeatureDescriptor&) const = default;
};

using FeatureParameters =
    std::variant<BoxParameters, CylinderParameters, SketchParameters, ExtrusionParameters,
                 FilletParameters, ChamferParameters, HoleParameters, CombineParameters,
                 DatumPlaneParameters, DatumAxisParameters, ShellParameters, PrimitiveParameters,
                 DraftParameters, MirrorParameters, PatternParameters, PatternCopyParameters,
                 MoveParameters, ImportParameters, ImportPartParameters>;
// M10/M17: the feature a companion belongs to (a pattern copy's pattern, an import part's
// import); it moves, is deleted and is shown with that feature.
[[nodiscard]] std::optional<FeatureId> companionOwner(const FeatureParameters& parameters) noexcept;
[[nodiscard]] constexpr bool isCompanion(FeatureKind kind) noexcept {
    return kind == FeatureKind::patternCopy || kind == FeatureKind::importPart;
}

// M16: what a feature refers to. Structural references must stand earlier and exist (its input,
// the bodies it works on, sketches, construction features, a copy's pattern); geometric ones name
// faces and edges by the features that made them, which may later fail without them.
struct FeatureReferences final {
    std::vector<FeatureId> structural;
    std::vector<FeatureId> geometric;
};
[[nodiscard]] FeatureReferences featureReferences(const FeatureDescriptor& feature,
                                                  const FeatureParameters& parameters);

// PAR01: every binding names an existing parameter of a matching unit, only dimensional
// constraints bind, and the table itself is valid. Throws std::invalid_argument.
void validateParameterBindings(std::span<const FeatureParameters> parameters,
                               const ParameterTable& table);

class ModelDocument final {
  public:
    ModelDocument();
    ~ModelDocument();
    ModelDocument(const ModelDocument& other);
    ModelDocument& operator=(const ModelDocument& other);
    // A moved-from document may be destroyed or assigned a new value.
    ModelDocument(ModelDocument&& other) noexcept;
    ModelDocument& operator=(ModelDocument&& other) noexcept;
    [[nodiscard]] static ModelDocument empty();
    [[nodiscard]] DocumentId documentId() const noexcept;

    [[nodiscard]] static constexpr FeatureId initialBoxId() noexcept { return FeatureId{1}; }
    [[nodiscard]] static constexpr FeatureId initialCylinderId() noexcept { return FeatureId{2}; }
    [[nodiscard]] std::span<const FeatureDescriptor> features() const noexcept;
    [[nodiscard]] bool contains(FeatureId featureId) const noexcept;
    [[nodiscard]] FeatureId addBoxFeature(std::string name,
                                          const kernel::BoxDimensions& dimensions);
    // M01: without input the cylinder is a new body at the origin (separate mode only).
    [[nodiscard]] FeatureId addCylinderFeature(std::string name,
                                               std::optional<FeatureId> inputFeature,
                                               const CylinderParameters& parameters);
    // V01: a sketch on a base plane; parameters.datum must be set. M19: or on a construction
    // plane, parameters.constructionPlane naming an earlier datumPlane feature.
    [[nodiscard]] FeatureId addDatumSketchFeature(std::string name,
                                                  const SketchParameters& parameters);
    [[nodiscard]] FeatureId addSketchFeature(std::string name, FeatureId supportFeature,
                                             const SketchParameters& parameters);
    [[nodiscard]] FeatureId addExtrusionFeature(std::string name, FeatureId sketchFeature,
                                                const ExtrusionParameters& parameters);
    // T04: push/pull of a body face; parameters.faceProfile must be set.
    [[nodiscard]] FeatureId addFaceExtrusionFeature(std::string name,
                                                    const ExtrusionParameters& parameters);
    // M05: rounds edges of an existing body; no input feature, the body names its producer.
    [[nodiscard]] FeatureId addFilletFeature(std::string name, const FilletParameters& parameters);
    [[nodiscard]] FeatureId addChamferFeature(std::string name,
                                              const ChamferParameters& parameters);
    // N09b: a hole with points takes `sketch` as its input (the sketch the points belong to).
    [[nodiscard]] FeatureId addHoleFeature(std::string name, const HoleParameters& parameters,
                                           std::optional<FeatureId> sketch = std::nullopt);
    [[nodiscard]] FeatureId addCombineFeature(std::string name,
                                              const CombineParameters& parameters);
    [[nodiscard]] FeatureId addDatumPlaneFeature(std::string name,
                                                 const DatumPlaneParameters& parameters);
    [[nodiscard]] FeatureId addDatumAxisFeature(std::string name,
                                                const DatumAxisParameters& parameters);
    [[nodiscard]] FeatureId addShellFeature(std::string name, const ShellParameters& parameters);
    // M27: as addCylinderFeature (ModelDocumentPrimitives.cpp).
    [[nodiscard]] FeatureId addPrimitiveFeature(std::string name,
                                                std::optional<FeatureId> inputFeature,
                                                const PrimitiveParameters& parameters);
    [[nodiscard]] const PrimitiveParameters& primitiveParameters(FeatureId featureId) const;
    // M25 (ModelDocumentDraft.cpp).
    [[nodiscard]] FeatureId addDraftFeature(std::string name, const DraftParameters& parameters);
    [[nodiscard]] const DraftParameters& draftParameters(FeatureId featureId) const;
    [[nodiscard]] bool setDraftParameters(FeatureId featureId, const DraftParameters& parameters);
    // M09 (ModelDocumentMirror.cpp).
    [[nodiscard]] FeatureId addMirrorFeature(std::string name, const MirrorParameters& parameters);
    [[nodiscard]] const MirrorParameters& mirrorParameters(FeatureId featureId) const;
    [[nodiscard]] bool setMirrorParameters(FeatureId featureId, const MirrorParameters& parameters);
    // M10 (ModelDocumentPattern.cpp): adding or changing a pattern adds or removes its copy
    // features in the same step.
    [[nodiscard]] FeatureId addPatternFeature(std::string name,
                                              const PatternParameters& parameters);
    [[nodiscard]] const PatternParameters& patternParameters(FeatureId featureId) const;
    [[nodiscard]] bool setPatternParameters(FeatureId featureId,
                                            const PatternParameters& parameters);
    // M11 (ModelDocumentMove.cpp).
    [[nodiscard]] FeatureId addMoveFeature(std::string name, const MoveParameters& parameters);
    [[nodiscard]] const MoveParameters& moveParameters(FeatureId featureId) const;
    [[nodiscard]] bool setMoveParameters(FeatureId featureId, const MoveParameters& parameters);
    // M17 (ModelDocumentImport.cpp): the import and its parts in one step; every body is named
    // after its solid, else "<name>.<n>".
    // N06: `colors` are the solids' colours from the file, in the order of the solids.
    [[nodiscard]] FeatureId addImportFeature(std::string name, ImportParameters parameters,
                                             std::vector<std::optional<std::uint32_t>> colors = {});
    [[nodiscard]] const ImportParameters& importParameters(FeatureId featureId) const;
    [[nodiscard]] bool setPrimitiveParameters(FeatureId featureId,
                                              const PrimitiveParameters& parameters);
    [[nodiscard]] const kernel::BoxDimensions& boxDimensions(FeatureId featureId) const;
    [[nodiscard]] const BoxParameters& boxParameters(FeatureId featureId) const;
    [[nodiscard]] bool setBoxDimensions(FeatureId featureId,
                                        const kernel::BoxDimensions& dimensions);
    [[nodiscard]] bool setBoxParameters(FeatureId featureId, const BoxParameters& parameters);
    [[nodiscard]] const CylinderParameters& cylinderParameters(FeatureId featureId) const;
    [[nodiscard]] bool setCylinderParameters(FeatureId featureId,
                                             const CylinderParameters& parameters);
    [[nodiscard]] const SketchParameters& sketchParameters(FeatureId featureId) const;
    [[nodiscard]] bool setSketchParameters(FeatureId featureId, const SketchParameters& parameters);
    [[nodiscard]] SketchElementId addSketchElement(FeatureId featureId, SketchGeometry geometry);
    [[nodiscard]] const ExtrusionParameters& extrusionParameters(FeatureId featureId) const;
    [[nodiscard]] bool setExtrusionParameters(FeatureId featureId,
                                              const ExtrusionParameters& parameters);
    [[nodiscard]] const FilletParameters& filletParameters(FeatureId featureId) const;
    [[nodiscard]] bool setFilletParameters(FeatureId featureId, const FilletParameters& parameters);
    [[nodiscard]] const ChamferParameters& chamferParameters(FeatureId featureId) const;
    [[nodiscard]] bool setChamferParameters(FeatureId featureId,
                                            const ChamferParameters& parameters);
    [[nodiscard]] const HoleParameters& holeParameters(FeatureId featureId) const;
    [[nodiscard]] bool setHoleParameters(FeatureId featureId, const HoleParameters& parameters);
    [[nodiscard]] const CombineParameters& combineParameters(FeatureId featureId) const;
    [[nodiscard]] bool setCombineParameters(FeatureId featureId,
                                            const CombineParameters& parameters);
    [[nodiscard]] const DatumPlaneParameters& datumPlaneParameters(FeatureId featureId) const;
    [[nodiscard]] bool setDatumPlaneParameters(FeatureId featureId,
                                               const DatumPlaneParameters& parameters);
    [[nodiscard]] const DatumAxisParameters& datumAxisParameters(FeatureId featureId) const;
    [[nodiscard]] bool setDatumAxisParameters(FeatureId featureId,
                                              const DatumAxisParameters& parameters);
    [[nodiscard]] const ShellParameters& shellParameters(FeatureId featureId) const;
    [[nodiscard]] bool setShellParameters(FeatureId featureId, const ShellParameters& parameters);
    [[nodiscard]] const kernel::RigidTransform& featurePlacement(FeatureId featureId) const;
    [[nodiscard]] bool setFeaturePlacement(FeatureId featureId,
                                           const kernel::RigidTransform& placement);
    // M14: suppresses or reactivates a feature; one undo step.
    [[nodiscard]] bool setFeatureSuppressed(FeatureId featureId, bool suppressed);
    // M14 (user decision 2026-10-16): the rollback marker after `featureId`, or at the end
    // (nullopt). Stored, but no undo step and no model change; undo and redo keep it. New
    // features are inserted at the marker, which then stands after them.
    [[nodiscard]] bool setRollback(std::optional<FeatureId> featureId);
    [[nodiscard]] std::optional<FeatureId> rollbackAfter() const noexcept;
    // M16 (ModelDocumentReorder.cpp): moves a feature (a pattern with its copies) after `after`,
    // or to the front; deletes features (a pattern's copies go with it). Body inputs follow
    // explicitly; a feature that would stand before or without what it needs throws
    // std::invalid_argument. One undo step each.
    [[nodiscard]] bool moveFeature(FeatureId featureId, std::optional<FeatureId> after);
    [[nodiscard]] bool deleteFeatures(std::span<const FeatureId> featureIds);
    [[nodiscard]] const ParameterTable& parameterTable() const noexcept;
    // Replaces the whole table; bindings are validated against it. Values are not mirrored
    // here: the application re-solves bound sketches in the same transaction.
    [[nodiscard]] bool setParameterTable(const ParameterTable& table);
    // M01: name and visibility of bodies (BodyAttributes.hpp). Renaming is one undo step;
    // hiding is stored but makes no undo step, and undo/redo keep the current visibility.
    [[nodiscard]] const BodyAttributeTable& bodyAttributes() const noexcept;
    [[nodiscard]] bool renameBody(BodyId body, std::string name);
    [[nodiscard]] bool setBodyHidden(BodyId body, bool hidden);
    // N06: none restores the theme's colour; one undo step.
    [[nodiscard]] bool setBodyColor(BodyId body, std::optional<std::uint32_t> color);
    // N07: none takes the material away; one undo step.
    [[nodiscard]] bool setBodyMaterial(BodyId body, std::optional<BodyMaterial> material);
    [[nodiscard]] bool canUndo() const noexcept;
    [[nodiscard]] bool canRedo() const noexcept;
    [[nodiscard]] bool undo();
    [[nodiscard]] bool redo();
    [[nodiscard]] std::uint64_t revision() const noexcept;

  private:
    friend class DocumentSerializer;
    friend class DocumentSnapshot;
    friend struct ModelDocumentBackendTestAccess;
    friend class DocumentTransaction;

    explicit ModelDocument(bool createStarterFeatures);
    ModelDocument(const DocumentSnapshot& snapshot, DocumentId id);

    struct State;
    struct Impl;

    [[nodiscard]] std::size_t featureIndex(FeatureId featureId) const;
    [[nodiscard]] State currentState() const;
    static DocumentSnapshot initialSnapshot(bool createStarterFeatures);
    void commitState(State state);
    void checkRevisionAvailable() const;
    [[nodiscard]] FeatureId nextFeatureId() const;

    std::unique_ptr<Impl> impl_;
};

} // namespace voelund::document
