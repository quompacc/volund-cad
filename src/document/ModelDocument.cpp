#include <voelund/document/ModelDocument.hpp>

#include <cmath>
#include <stdexcept>
#include <utility>

namespace voelund::document {
namespace {

void validateBox(const kernel::BoxDimensions& dimensions) {
    if (dimensions.width <= 0.0 || dimensions.depth <= 0.0 || dimensions.height <= 0.0) {
        throw std::invalid_argument("Box dimensions must be positive");
    }
}

void validateCylinder(const CylinderParameters& parameters) {
    if (parameters.radius <= 0.0 || parameters.height <= 0.0) {
        throw std::invalid_argument("Cylinder radius and height must be positive");
    }
}

} // namespace

void validateSketchParameters(const SketchParameters& parameters) {
    const double values[] = {
        parameters.plane.originX,     parameters.plane.originY,     parameters.plane.originZ,
        parameters.plane.normalX,     parameters.plane.normalY,     parameters.plane.normalZ,
        parameters.plane.xDirectionX, parameters.plane.xDirectionY, parameters.plane.xDirectionZ};
    for (double value : values) {
        if (!std::isfinite(value)) {
            throw std::invalid_argument("Sketch plane must contain finite values");
        }
    }
    const double normalLength =
        std::hypot(parameters.plane.normalX, parameters.plane.normalY, parameters.plane.normalZ);
    const double xDirectionLength = std::hypot(
        parameters.plane.xDirectionX, parameters.plane.xDirectionY, parameters.plane.xDirectionZ);
    if (normalLength <= 1.0e-9 || xDirectionLength <= 1.0e-9) {
        throw std::invalid_argument("Sketch plane directions must not be zero");
    }
    const double dot = parameters.plane.normalX * parameters.plane.xDirectionX +
                       parameters.plane.normalY * parameters.plane.xDirectionY +
                       parameters.plane.normalZ * parameters.plane.xDirectionZ;
    if (std::abs(dot / (normalLength * xDirectionLength)) > 1.0e-8) {
        throw std::invalid_argument("Sketch plane directions must be perpendicular");
    }
    validateSketchElements(parameters.elements);
    validateSketchConstraints(parameters.elements, parameters.constraints);
}

ModelDocument::ModelDocument() : ModelDocument(true) {}

ModelDocument ModelDocument::empty() { return ModelDocument(false); }

ModelDocument::ModelDocument(bool createStarterFeatures) {
    if (!createStarterFeatures) {
        return;
    }
    features_.push_back(FeatureDescriptor{initialBoxId(), FeatureKind::box, "Box", std::nullopt});
    parameters_.emplace_back(BoxParameters{kernel::BoxDimensions{40.0, 30.0, 20.0},
                                           kernel::RigidTransform{0.0, 0.0, 0.0, 0.0, 0.0, 0.0}});
    features_.push_back(
        FeatureDescriptor{initialCylinderId(), FeatureKind::cylinder, "Cylinder", initialBoxId()});
    parameters_.emplace_back(CylinderParameters{
        8.0, 25.0, kernel::RigidTransform{0.0, 0.0, 0.0, 0.0, 0.0, 0.0}, CylinderResultMode::join});
    nextFeatureId_ = 3;
}

std::span<const FeatureDescriptor> ModelDocument::features() const noexcept { return features_; }

bool ModelDocument::contains(FeatureId featureId) const noexcept {
    for (const FeatureDescriptor& feature : features_) {
        if (feature.id == featureId) {
            return true;
        }
    }
    return false;
}

FeatureId ModelDocument::addBoxFeature(std::string name, const kernel::BoxDimensions& dimensions) {
    validateBox(dimensions);
    recordChange();
    const FeatureId id{nextFeatureId_++};
    features_.push_back(FeatureDescriptor{id, FeatureKind::box, std::move(name), std::nullopt});
    parameters_.emplace_back(
        BoxParameters{dimensions, kernel::RigidTransform{0.0, 0.0, 0.0, 0.0, 0.0, 0.0}});
    return id;
}

FeatureId ModelDocument::addCylinderFeature(std::string name, FeatureId inputFeature,
                                            const CylinderParameters& parameters) {
    if (!contains(inputFeature)) {
        throw std::invalid_argument("Cylinder input feature does not exist");
    }
    validateCylinder(parameters);
    recordChange();
    const FeatureId id{nextFeatureId_++};
    features_.push_back(
        FeatureDescriptor{id, FeatureKind::cylinder, std::move(name), inputFeature});
    parameters_.emplace_back(parameters);
    return id;
}

FeatureId ModelDocument::addSketchFeature(std::string name, FeatureId supportFeature,
                                          const SketchParameters& parameters) {
    if (!contains(supportFeature)) {
        throw std::invalid_argument("Sketch support feature does not exist");
    }
    validateSketchParameters(parameters);
    recordChange();
    const FeatureId id{nextFeatureId_++};
    features_.push_back(
        FeatureDescriptor{id, FeatureKind::sketch, std::move(name), supportFeature});
    parameters_.emplace_back(parameters);
    return id;
}

FeatureId ModelDocument::addExtrusionFeature(std::string name, FeatureId sketchFeature,
                                             const ExtrusionParameters& parameters) {
    if (!contains(sketchFeature) || !std::isfinite(parameters.distance) ||
        parameters.distance <= 0.0) {
        throw std::invalid_argument("Extrusion requires a sketch and a positive distance");
    }
    if (features_.at(featureIndex(sketchFeature)).kind != FeatureKind::sketch) {
        throw std::invalid_argument("Extrusion input feature must be a sketch");
    }
    recordChange();
    const FeatureId id{nextFeatureId_++};
    features_.push_back(
        FeatureDescriptor{id, FeatureKind::extrusion, std::move(name), sketchFeature});
    parameters_.emplace_back(parameters);
    return id;
}

const kernel::BoxDimensions& ModelDocument::boxDimensions(FeatureId featureId) const {
    return boxParameters(featureId).dimensions;
}

const BoxParameters& ModelDocument::boxParameters(FeatureId featureId) const {
    const FeatureParameters& parameters = parameters_.at(featureIndex(featureId));
    const auto* box = std::get_if<BoxParameters>(&parameters);
    if (box == nullptr) {
        throw std::out_of_range("Feature is not a box");
    }
    return *box;
}

bool ModelDocument::setBoxDimensions(FeatureId featureId, const kernel::BoxDimensions& dimensions) {
    BoxParameters parameters = boxParameters(featureId);
    parameters.dimensions = dimensions;
    return setBoxParameters(featureId, parameters);
}

bool ModelDocument::setBoxParameters(FeatureId featureId, const BoxParameters& parameters) {
    validateBox(parameters.dimensions);
    const std::size_t index = featureIndex(featureId);
    auto* current = std::get_if<BoxParameters>(&parameters_.at(index));
    if (current == nullptr) {
        throw std::out_of_range("Feature is not a box");
    }
    if (*current == parameters) {
        return false;
    }
    recordChange();
    *std::get_if<BoxParameters>(&parameters_.at(index)) = parameters;
    return true;
}

const CylinderParameters& ModelDocument::cylinderParameters(FeatureId featureId) const {
    const FeatureParameters& parameters = parameters_.at(featureIndex(featureId));
    const auto* cylinder = std::get_if<CylinderParameters>(&parameters);
    if (cylinder == nullptr) {
        throw std::out_of_range("Feature is not a cylinder");
    }
    return *cylinder;
}

bool ModelDocument::setCylinderParameters(FeatureId featureId,
                                          const CylinderParameters& parameters) {
    validateCylinder(parameters);
    const std::size_t index = featureIndex(featureId);
    auto* current = std::get_if<CylinderParameters>(&parameters_.at(index));
    if (current == nullptr) {
        throw std::out_of_range("Feature is not a cylinder");
    }
    if (*current == parameters) {
        return false;
    }
    recordChange();
    *std::get_if<CylinderParameters>(&parameters_.at(index)) = parameters;
    return true;
}

const SketchParameters& ModelDocument::sketchParameters(FeatureId featureId) const {
    const auto* value = std::get_if<SketchParameters>(&parameters_.at(featureIndex(featureId)));
    if (value == nullptr) {
        throw std::out_of_range("Feature is not a sketch");
    }
    return *value;
}

bool ModelDocument::setSketchParameters(FeatureId featureId, const SketchParameters& parameters) {
    validateSketchParameters(parameters);
    const std::size_t index = featureIndex(featureId);
    auto* current = std::get_if<SketchParameters>(&parameters_.at(index));
    if (current == nullptr) {
        throw std::out_of_range("Feature is not a sketch");
    }
    if (*current == parameters) {
        return false;
    }
    recordChange();
    *std::get_if<SketchParameters>(&parameters_.at(index)) = parameters;
    return true;
}

SketchElementId ModelDocument::addSketchElement(FeatureId featureId, SketchGeometry geometry) {
    SketchParameters changed = sketchParameters(featureId);
    const SketchElementId id{changed.elements.nextId++};
    changed.elements.values.push_back(SketchElement{id, std::move(geometry)});
    static_cast<void>(setSketchParameters(featureId, changed));
    return id;
}

const ExtrusionParameters& ModelDocument::extrusionParameters(FeatureId featureId) const {
    const auto* value = std::get_if<ExtrusionParameters>(&parameters_.at(featureIndex(featureId)));
    if (value == nullptr) {
        throw std::out_of_range("Feature is not an extrusion");
    }
    return *value;
}

bool ModelDocument::setExtrusionParameters(FeatureId featureId,
                                           const ExtrusionParameters& parameters) {
    if (!std::isfinite(parameters.distance) || parameters.distance <= 0.0) {
        throw std::invalid_argument("Extrusion distance must be positive and finite");
    }
    const std::size_t index = featureIndex(featureId);
    auto* current = std::get_if<ExtrusionParameters>(&parameters_.at(index));
    if (current == nullptr) {
        throw std::out_of_range("Feature is not an extrusion");
    }
    if (*current == parameters) {
        return false;
    }
    recordChange();
    *std::get_if<ExtrusionParameters>(&parameters_.at(index)) = parameters;
    return true;
}

const kernel::RigidTransform& ModelDocument::featurePlacement(FeatureId featureId) const {
    const FeatureParameters& parameters = parameters_.at(featureIndex(featureId));
    if (const auto* box = std::get_if<BoxParameters>(&parameters)) {
        return box->placement;
    }
    const auto* cylinder = std::get_if<CylinderParameters>(&parameters);
    if (cylinder == nullptr) {
        throw std::out_of_range("Feature has no rigid placement");
    }
    return cylinder->placement;
}

bool ModelDocument::setFeaturePlacement(FeatureId featureId,
                                        const kernel::RigidTransform& placement) {
    const std::size_t index = featureIndex(featureId);
    FeatureParameters changed = parameters_.at(index);
    if (auto* box = std::get_if<BoxParameters>(&changed)) {
        box->placement = placement;
    } else if (auto* cylinder = std::get_if<CylinderParameters>(&changed)) {
        cylinder->placement = placement;
    } else {
        throw std::out_of_range("Feature has no rigid placement");
    }
    if (changed == parameters_.at(index)) {
        return false;
    }
    recordChange();
    parameters_.at(index) = changed;
    return true;
}

bool ModelDocument::canUndo() const noexcept { return !undoHistory_.empty(); }

bool ModelDocument::canRedo() const noexcept { return !redoHistory_.empty(); }

bool ModelDocument::undo() noexcept {
    if (!canUndo()) {
        return false;
    }
    redoHistory_.push_back(currentState());
    restoreState(undoHistory_.back());
    undoHistory_.pop_back();
    return true;
}

bool ModelDocument::redo() noexcept {
    if (!canRedo()) {
        return false;
    }
    undoHistory_.push_back(currentState());
    restoreState(redoHistory_.back());
    redoHistory_.pop_back();
    return true;
}

std::size_t ModelDocument::featureIndex(FeatureId featureId) const {
    for (std::size_t index = 0; index < features_.size(); ++index) {
        if (features_[index].id == featureId) {
            return index;
        }
    }
    throw std::out_of_range("Unknown feature ID");
}

ModelDocument::State ModelDocument::currentState() const noexcept {
    return State{features_, parameters_, nextFeatureId_};
}

void ModelDocument::restoreState(const State& state) noexcept {
    features_ = state.features;
    parameters_ = state.parameters;
    nextFeatureId_ = state.nextFeatureId;
}

void ModelDocument::recordChange() {
    undoHistory_.push_back(currentState());
    redoHistory_.clear();
}

} // namespace voelund::document
