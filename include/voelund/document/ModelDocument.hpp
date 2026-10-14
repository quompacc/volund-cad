#pragma once

#include <voelund/document/BodyAttributes.hpp>
#include <voelund/document/DocumentId.hpp>
#include <voelund/document/DocumentParameters.hpp>
#include <voelund/document/FeatureId.hpp>
#include <voelund/document/SketchGeometry.hpp>
#include <voelund/document/SketchPattern.hpp>
#include <voelund/document/SketchRegionId.hpp>
#include <voelund/document/SubshapeReference.hpp>
#include <voelund/kernel/ShapeFactory.hpp>

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

enum class FeatureKind { box, cylinder, sketch, extrusion, fillet, chamfer, hole, combine };
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

    SketchParameters() = default;
    SketchParameters(SketchPlane sketchPlane, SketchElements sketchElements,
                     SketchConstraints sketchConstraints = {})
        : plane(sketchPlane), elements(std::move(sketchElements)),
          constraints(std::move(sketchConstraints)) {}

    [[nodiscard]] bool operator==(const SketchParameters&) const = default;
};

void validateSketchParameters(const SketchParameters& parameters);
void validateCylinderParameters(const CylinderParameters& parameters);

// T03: a planar face of one of this document's bodies, named by its semantic origin.
struct BodyFaceReference final {
    BodyId body;
    SubshapeOrigin origin;
    [[nodiscard]] bool operator==(const BodyFaceReference&) const = default;
};

// T02: what an extrusion's prism does to a body. M02 (schema 13): intersect keeps only what
// the prism and the target body have in common.
enum class ExtrusionOperation { newBody = 1, join = 2, cut = 3, intersect = 4 };

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
    // Join, cut or intersect of a base-plane sketch names the body it works on (its creating feature's
    // ID). A face sketch always works on its support's body and names none.
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

    [[nodiscard]] bool operator==(const ExtrusionParameters&) const = default;
};
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
struct ChamferParameters final {
    BodyId body;
    std::vector<FilletEdge> edges;
    double distance;
    [[nodiscard]] bool operator==(const ChamferParameters&) const = default;
};
void validateChamferParameters(const ChamferParameters& parameters);
// M08 (schema 16, user decision 2026-10-13): a cylindrical hole into a planar body face. It
// sits at (x, y) in the face's frame (the frame a sketch on that face gets) and runs along
// the inward normal, `depth` deep with a flat bottom, or through the whole body without one.
struct HoleParameters final {
    BodyFaceReference face;
    double x;
    double y;
    double diameter;
    std::optional<double> depth{};
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

// The operation in effect, with the earlier rule for an unset one.
[[nodiscard]] ExtrusionOperation effectiveOperation(const ExtrusionParameters& parameters,
                                                    bool baseSketch) noexcept;

struct FeatureDescriptor final {
    FeatureId id;
    FeatureKind kind;
    std::string name;
    std::optional<FeatureId> inputFeature;
    [[nodiscard]] bool operator==(const FeatureDescriptor&) const = default;
};

using FeatureParameters =
    std::variant<BoxParameters, CylinderParameters, SketchParameters, ExtrusionParameters,
                 FilletParameters, ChamferParameters, HoleParameters, CombineParameters>;

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
    // V01: a sketch on a base plane; parameters.datum must be set.
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
    [[nodiscard]] FeatureId addHoleFeature(std::string name, const HoleParameters& parameters);
    [[nodiscard]] FeatureId addCombineFeature(std::string name,
                                              const CombineParameters& parameters);
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
    [[nodiscard]] const kernel::RigidTransform& featurePlacement(FeatureId featureId) const;
    [[nodiscard]] bool setFeaturePlacement(FeatureId featureId,
                                           const kernel::RigidTransform& placement);
    [[nodiscard]] const ParameterTable& parameterTable() const noexcept;
    // Replaces the whole table; bindings are validated against it. Values are not mirrored
    // here: the application re-solves bound sketches in the same transaction.
    [[nodiscard]] bool setParameterTable(const ParameterTable& table);
    // M01: name and visibility of bodies (BodyAttributes.hpp). Renaming is one undo step;
    // hiding is stored but makes no undo step, and undo/redo keep the current visibility.
    [[nodiscard]] const BodyAttributeTable& bodyAttributes() const noexcept;
    [[nodiscard]] bool renameBody(BodyId body, std::string name);
    [[nodiscard]] bool setBodyHidden(BodyId body, bool hidden);
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
