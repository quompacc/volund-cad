#pragma once

#include <voelund/document/FeatureId.hpp>
#include <voelund/document/SketchGeometry.hpp>
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

enum class FeatureKind { box, cylinder, sketch, extrusion };
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

struct SketchParameters final {
    SketchPlane plane;
    SketchElements elements;
    SketchConstraints constraints;

    SketchParameters() = default;
    SketchParameters(SketchPlane sketchPlane, SketchElements sketchElements,
                     SketchConstraints sketchConstraints = {})
        : plane(sketchPlane), elements(std::move(sketchElements)),
          constraints(std::move(sketchConstraints)) {}

    [[nodiscard]] bool operator==(const SketchParameters&) const = default;
};

void validateSketchParameters(const SketchParameters& parameters);
void validateCylinderParameters(const CylinderParameters& parameters);

struct ExtrusionParameters final {
    double distance;
    std::vector<SketchProfileId> profiles;

    [[nodiscard]] bool operator==(const ExtrusionParameters&) const = default;
};

struct FeatureDescriptor final {
    FeatureId id;
    FeatureKind kind;
    std::string name;
    std::optional<FeatureId> inputFeature;
    [[nodiscard]] bool operator==(const FeatureDescriptor&) const = default;
};

using FeatureParameters =
    std::variant<BoxParameters, CylinderParameters, SketchParameters, ExtrusionParameters>;

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

    [[nodiscard]] static constexpr FeatureId initialBoxId() noexcept { return FeatureId{1}; }
    [[nodiscard]] static constexpr FeatureId initialCylinderId() noexcept { return FeatureId{2}; }
    [[nodiscard]] std::span<const FeatureDescriptor> features() const noexcept;
    [[nodiscard]] bool contains(FeatureId featureId) const noexcept;
    [[nodiscard]] FeatureId addBoxFeature(std::string name,
                                          const kernel::BoxDimensions& dimensions);
    [[nodiscard]] FeatureId addCylinderFeature(std::string name, FeatureId inputFeature,
                                               const CylinderParameters& parameters);
    [[nodiscard]] FeatureId addSketchFeature(std::string name, FeatureId supportFeature,
                                             const SketchParameters& parameters);
    [[nodiscard]] FeatureId addExtrusionFeature(std::string name, FeatureId sketchFeature,
                                                const ExtrusionParameters& parameters);
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
    [[nodiscard]] const kernel::RigidTransform& featurePlacement(FeatureId featureId) const;
    [[nodiscard]] bool setFeaturePlacement(FeatureId featureId,
                                           const kernel::RigidTransform& placement);
    [[nodiscard]] bool canUndo() const noexcept;
    [[nodiscard]] bool canRedo() const noexcept;
    [[nodiscard]] bool undo();
    [[nodiscard]] bool redo();
    [[nodiscard]] std::uint64_t revision() const noexcept;

  private:
    friend class DocumentSerializer;
    friend class DocumentSnapshot;
    friend class DocumentTransaction;

    explicit ModelDocument(bool createStarterFeatures);

    struct State;
    struct Impl;

    [[nodiscard]] std::size_t featureIndex(FeatureId featureId) const;
    [[nodiscard]] State currentState() const;
    void swapState(State& state) noexcept;
    void commitState(State state);
    void checkRevisionAvailable() const;
    [[nodiscard]] FeatureId nextFeatureId() const;

    std::unique_ptr<Impl> impl_;
};

} // namespace voelund::document
