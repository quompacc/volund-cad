// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "ModelDocumentImpl.hpp"
#include <voelund/document/IdCapacity.hpp>
#include <voelund/document/LegacyBodyIdentity.hpp>
#include <voelund/document/ModelDocument.hpp>
#include <voelund/document/SketchPlaneValidation.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

namespace voelund::document {
namespace {

void validateBox(const kernel::BoxDimensions& dimensions) {
    if (!kernel::modelLength(dimensions.width) || !kernel::modelLength(dimensions.depth) ||
        !kernel::modelLength(dimensions.height)) {
        throw std::invalid_argument(kernel::modelRangeDiagnostic);
    }
}

void validateCylinder(const CylinderParameters& parameters) {
    if (!kernel::modelLength(parameters.radius) || !kernel::modelLength(parameters.height)) {
        throw std::invalid_argument(kernel::modelRangeDiagnostic);
    }
    switch (parameters.resultMode) {
    case CylinderResultMode::separate:
    case CylinderResultMode::join:
    case CylinderResultMode::cut:
    case CylinderResultMode::intersect:
        break;
    default:
        throw std::invalid_argument("Unknown cylinder connection mode");
    }
}

void validatePlacement(const kernel::RigidTransform& placement) {
    if (!kernel::modelCoordinate(placement.translationX) ||
        !kernel::modelCoordinate(placement.translationY) ||
        !kernel::modelCoordinate(placement.translationZ))
        throw std::invalid_argument(kernel::modelRangeDiagnostic);
    for (double value :
         {placement.translationX, placement.translationY, placement.translationZ,
          placement.rotationX.value(), placement.rotationY.value(), placement.rotationZ.value()})
        if (!std::isfinite(value))
            throw std::invalid_argument("Placement must contain finite values");
}

// Box, cylinder and the M27 primitives carry a rigid placement; nothing else does.
template <typename Parameters> auto* placementOf(Parameters& parameters) {
    using Pointer = std::conditional_t<std::is_const_v<Parameters>, const kernel::RigidTransform*,
                                       kernel::RigidTransform*>;
    return std::visit(
        [](auto& value) -> Pointer {
            if constexpr (requires { value.placement.rotationX; })
                return &value.placement;
            else
                return nullptr;
        },
        parameters);
}

void validateExtrusion(const ExtrusionParameters& parameters) {
    if (!kernel::modelLength(parameters.distance))
        throw std::invalid_argument(kernel::modelRangeDiagnostic);
    for (std::size_t index = 0; index < parameters.profiles.size(); ++index) {
        if (parameters.profiles[index].value() == 0)
            throw std::invalid_argument("Extrusion profile IDs must be non-zero");
        for (std::size_t prior = 0; prior < index; ++prior) {
            if (parameters.profiles[prior] == parameters.profiles[index])
                throw std::invalid_argument("Extrusion profile IDs must be unique");
        }
    }
    if (!parameters.profiles.empty() && !parameters.regions.empty())
        throw std::invalid_argument("Extrusion mixes legacy profiles and regions");
    for (std::size_t index = 0; index < parameters.regions.size(); ++index) {
        for (std::size_t prior = 0; prior < index; ++prior) {
            if (parameters.regions[prior] == parameters.regions[index])
                throw std::invalid_argument("Extrusion region IDs must be unique");
        }
    }
    if (parameters.operation && (*parameters.operation < ExtrusionOperation::newBody ||
                                 *parameters.operation > ExtrusionOperation::intersect))
        throw std::invalid_argument("Unknown extrusion operation");
    if (parameters.target && parameters.operation != ExtrusionOperation::join &&
        parameters.operation != ExtrusionOperation::cut &&
        parameters.operation != ExtrusionOperation::intersect)
        throw std::invalid_argument("Only a join, cut or intersect names a target body");
    if (parameters.symmetric &&
        (parameters.reversed || parameters.offset != 0.0 || parameters.offsetParameter ||
         parameters.upToFace || parameters.faceProfile))
        throw std::invalid_argument(
            "A symmetric extrusion has no direction, start offset or end face");
    validateSweepExtrusion(parameters); // M23
    validateTaperExtrusion(parameters); // W16
    if (parameters.revolve) {
        validateRevolveParameters(*parameters.revolve);
        if (parameters.offset != 0.0 || parameters.offsetParameter || parameters.upToFace ||
            parameters.faceProfile || parameters.distanceParameter)
            throw std::invalid_argument("A revolve has no offset, end face or face profile");
    }
    if (!kernel::modelCoordinate(parameters.offset))
        throw std::invalid_argument(kernel::modelRangeDiagnostic);
    if (parameters.upToFace)
        validateSubshapeOrigin(parameters.upToFace->origin);
    if (parameters.faceProfile) {
        validateSubshapeOrigin(parameters.faceProfile->origin);
        if (!parameters.profiles.empty() || !parameters.regions.empty() || parameters.target ||
            (parameters.operation != ExtrusionOperation::join &&
             parameters.operation != ExtrusionOperation::cut))
            throw std::invalid_argument(
                "A face extrusion joins or cuts its face's body and selects no sketch regions");
    }
}

} // namespace

ExtrusionOperation effectiveOperation(const ExtrusionParameters& parameters,
                                      bool baseSketch) noexcept {
    return parameters.operation.value_or(baseSketch ? ExtrusionOperation::newBody
                                                    : ExtrusionOperation::join);
}

void validateCylinderParameters(const CylinderParameters& parameters) {
    validateCylinder(parameters);
    validatePlacement(parameters.placement);
}

void validatePrimitiveParameters(const PrimitiveParameters& parameters) { // M27
    if (const char* problem = kernel::primitiveSizeProblem(parameters.definition()))
        throw std::invalid_argument(problem);
    validateCylinder({1.0, 1.0, parameters.placement, parameters.resultMode});
    validatePlacement(parameters.placement);
}

SketchPlane datumSketchPlane(DatumPlane datum) {
    switch (datum) {
    case DatumPlane::xy:
        return {0, 0, 0, 0, 0, 1, 1, 0, 0};
    case DatumPlane::xz:
        return {0, 0, 0, 0, -1, 0, 1, 0, 0};
    case DatumPlane::yz:
        return {0, 0, 0, 1, 0, 0, 0, 1, 0};
    }
    throw std::invalid_argument("Unknown datum plane");
}

void validateSketchParameters(const SketchParameters& parameters) {
    if (parameters.datum && (parameters.supportBinding || parameters.repairReason ||
                             !(parameters.plane == datumSketchPlane(*parameters.datum))))
        throw std::invalid_argument("A datum sketch lies exactly on its unbound base plane");
    if (parameters.constructionPlane &&
        (parameters.datum || parameters.supportBinding || parameters.repairReason ||
         parameters.constructionPlane->value() == 0))
        throw std::invalid_argument("A sketch on a construction plane has no other support");
    validateCanonicalSketchPlane(parameters.plane);
    validateSketchElements(parameters.elements);
    validateSketchConstraints(parameters.elements, parameters.constraints);
    validateSketchPatterns(parameters.elements, parameters.constraints, parameters.patterns);
    validateSketchProjections(parameters);
    if (parameters.supportBinding) {
        validateSubshapeOrigin(parameters.supportBinding->reference.origin);
        validateCanonicalSketchPlane(parameters.supportBinding->localPlane);
    }
}

ModelDocument::ModelDocument() : ModelDocument(true) {}

ModelDocument ModelDocument::empty() { return ModelDocument(false); }

ModelDocument::ModelDocument(bool createStarterFeatures)
    : ModelDocument(initialSnapshot(createStarterFeatures), generateDocumentId()) {}
ModelDocument::ModelDocument(const DocumentSnapshot& snapshot, DocumentId id)
    : impl_(std::make_unique<Impl>(IdentifiedDocumentSnapshot::restore(id, snapshot))) {}
DocumentSnapshot ModelDocument::initialSnapshot(bool createStarterFeatures) {
    if (!createStarterFeatures)
        return DocumentSnapshot({}, {}, 1);
    return DocumentSnapshot(
        {{initialBoxId(), FeatureKind::box, "Box", std::nullopt},
         {initialCylinderId(), FeatureKind::cylinder, "Cylinder", initialBoxId()}},
        {BoxParameters{{40, 30, 20}, {0, 0, 0, {}, {}, {}}},
         CylinderParameters{8, 25, {0, 0, 0, {}, {}, {}}, CylinderResultMode::join}},
        3);
}

std::span<const FeatureDescriptor> ModelDocument::features() const noexcept {
    return impl_->view().features();
}

bool ModelDocument::contains(FeatureId featureId) const noexcept {
    for (const FeatureDescriptor& feature : impl_->view().features()) {
        if (feature.id == featureId) {
            return true;
        }
    }
    return false;
}

FeatureId ModelDocument::addBoxFeature(std::string name, const kernel::BoxDimensions& dimensions) {
    validateBox(dimensions);
    const FeatureId id = nextFeatureId();
    auto candidate = currentState();
    ++candidate.nextFeatureId;
    candidate.features.push_back(
        FeatureDescriptor{id, FeatureKind::box, std::move(name), std::nullopt});
    candidate.parameters.emplace_back(
        BoxParameters{dimensions, kernel::RigidTransform{0.0, 0.0, 0.0, {}, {}, {}}});
    commitState(std::move(candidate));
    return id;
}

FeatureId ModelDocument::addCylinderFeature(std::string name, std::optional<FeatureId> inputFeature,
                                            const CylinderParameters& parameters) {
    if (inputFeature && !contains(*inputFeature)) {
        throw std::invalid_argument("Cylinder input feature does not exist");
    }
    if (!inputFeature && parameters.resultMode != CylinderResultMode::separate)
        throw std::invalid_argument("A cylinder without input is a new body");
    validateCylinderParameters(parameters);
    const FeatureId id = nextFeatureId();
    auto candidate = currentState();
    ++candidate.nextFeatureId;
    candidate.features.push_back(
        FeatureDescriptor{id, FeatureKind::cylinder, std::move(name), inputFeature});
    candidate.parameters.emplace_back(parameters);
    commitState(std::move(candidate));
    return id;
}

FeatureId ModelDocument::addDatumSketchFeature(std::string name,
                                               const SketchParameters& parameters) {
    if (!parameters.datum && !parameters.constructionPlane)
        throw std::invalid_argument("A sketch without support requires a base plane");
    validateSketchParameters(parameters);
    if (parameters.constructionPlane) {
        const auto features = impl_->view().features();
        const auto plane =
            std::find_if(features.begin(), features.end(), [&](const FeatureDescriptor& feature) {
                return feature.id == *parameters.constructionPlane;
            });
        if (plane == features.end() || plane->kind != FeatureKind::datumPlane)
            throw std::invalid_argument("The construction plane does not exist");
    }
    const FeatureId id = nextFeatureId();
    auto candidate = currentState();
    ++candidate.nextFeatureId;
    candidate.features.push_back(
        FeatureDescriptor{id, FeatureKind::sketch, std::move(name), std::nullopt});
    candidate.parameters.emplace_back(parameters);
    commitState(std::move(candidate));
    return id;
}

FeatureId ModelDocument::addSketchFeature(std::string name, FeatureId supportFeature,
                                          const SketchParameters& parameters) {
    if (!contains(supportFeature)) {
        throw std::invalid_argument("Sketch support feature does not exist");
    }
    if (parameters.datum || parameters.constructionPlane)
        throw std::invalid_argument("A supported sketch cannot also lie on a base plane");
    validateSketchParameters(parameters);
    const FeatureId id = nextFeatureId();
    auto candidate = currentState();
    ++candidate.nextFeatureId;
    candidate.features.push_back(
        FeatureDescriptor{id, FeatureKind::sketch, std::move(name), supportFeature});
    candidate.parameters.emplace_back(parameters);
    commitState(std::move(candidate));
    return id;
}

FeatureId ModelDocument::addFaceExtrusionFeature(std::string name,
                                                 const ExtrusionParameters& parameters) {
    if (!parameters.faceProfile)
        throw std::invalid_argument("Face extrusion requires a profile face");
    validateExtrusion(parameters);
    const FeatureId id = nextFeatureId();
    auto candidate = currentState();
    ++candidate.nextFeatureId;
    candidate.features.push_back(
        FeatureDescriptor{id, FeatureKind::extrusion, std::move(name), std::nullopt});
    candidate.parameters.emplace_back(parameters);
    static_cast<void>(featureBodyContexts(candidate.features, candidate.parameters));
    commitState(std::move(candidate));
    return id;
}

FeatureId ModelDocument::addFilletFeature(std::string name, const FilletParameters& parameters) {
    validateFilletParameters(parameters);
    const FeatureId id = nextFeatureId();
    auto candidate = currentState();
    ++candidate.nextFeatureId;
    candidate.features.push_back(
        FeatureDescriptor{id, FeatureKind::fillet, std::move(name), std::nullopt});
    candidate.parameters.emplace_back(parameters);
    static_cast<void>(featureBodyContexts(candidate.features, candidate.parameters));
    commitState(std::move(candidate));
    return id;
}

FeatureId ModelDocument::addChamferFeature(std::string name, const ChamferParameters& parameters) {
    validateChamferParameters(parameters);
    const FeatureId id = nextFeatureId();
    auto candidate = currentState();
    ++candidate.nextFeatureId;
    candidate.features.push_back(
        FeatureDescriptor{id, FeatureKind::chamfer, std::move(name), std::nullopt});
    candidate.parameters.emplace_back(parameters);
    static_cast<void>(featureBodyContexts(candidate.features, candidate.parameters));
    commitState(std::move(candidate));
    return id;
}

FeatureId ModelDocument::addHoleFeature(std::string name, const HoleParameters& parameters) {
    validateHoleParameters(parameters);
    const FeatureId id = nextFeatureId();
    auto candidate = currentState();
    ++candidate.nextFeatureId;
    candidate.features.push_back(
        FeatureDescriptor{id, FeatureKind::hole, std::move(name), std::nullopt});
    candidate.parameters.emplace_back(parameters);
    static_cast<void>(featureBodyContexts(candidate.features, candidate.parameters));
    commitState(std::move(candidate));
    return id;
}

FeatureId ModelDocument::addExtrusionFeature(std::string name, FeatureId sketchFeature,
                                             const ExtrusionParameters& parameters) {
    if (!contains(sketchFeature))
        throw std::invalid_argument("Extrusion requires an existing sketch");
    validateExtrusion(parameters);
    if (parameters.faceProfile)
        throw std::invalid_argument("A sketch extrusion has no profile face");
    if (impl_->view().features()[featureIndex(sketchFeature)].kind != FeatureKind::sketch) {
        throw std::invalid_argument("Extrusion input feature must be a sketch");
    }
    const FeatureId id = nextFeatureId();
    auto candidate = currentState();
    ++candidate.nextFeatureId;
    candidate.features.push_back(
        FeatureDescriptor{id, FeatureKind::extrusion, std::move(name), sketchFeature});
    candidate.parameters.emplace_back(parameters);
    static_cast<void>(featureBodyContexts(candidate.features, candidate.parameters));
    commitState(std::move(candidate));
    return id;
}

const kernel::BoxDimensions& ModelDocument::boxDimensions(FeatureId featureId) const {
    return boxParameters(featureId).dimensions;
}

const BoxParameters& ModelDocument::boxParameters(FeatureId featureId) const {
    const FeatureParameters& parameters = impl_->view().parameters()[featureIndex(featureId)];
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
    validatePlacement(parameters.placement);
    const std::size_t index = featureIndex(featureId);
    auto* current = std::get_if<BoxParameters>(&impl_->view().parameters()[index]);
    if (current == nullptr) {
        throw std::out_of_range("Feature is not a box");
    }
    if (*current == parameters) {
        return false;
    }
    auto candidate = currentState();
    *std::get_if<BoxParameters>(&candidate.parameters.at(index)) = parameters;
    commitState(std::move(candidate));
    return true;
}

const CylinderParameters& ModelDocument::cylinderParameters(FeatureId featureId) const {
    const FeatureParameters& parameters = impl_->view().parameters()[featureIndex(featureId)];
    const auto* cylinder = std::get_if<CylinderParameters>(&parameters);
    if (cylinder == nullptr) {
        throw std::out_of_range("Feature is not a cylinder");
    }
    return *cylinder;
}

bool ModelDocument::setCylinderParameters(FeatureId featureId,
                                          const CylinderParameters& parameters) {
    validateCylinderParameters(parameters);
    const std::size_t index = featureIndex(featureId);
    auto* current = std::get_if<CylinderParameters>(&impl_->view().parameters()[index]);
    if (current == nullptr) {
        throw std::out_of_range("Feature is not a cylinder");
    }
    if (*current == parameters) {
        return false;
    }
    auto candidate = currentState();
    *std::get_if<CylinderParameters>(&candidate.parameters.at(index)) = parameters;
    commitState(std::move(candidate));
    return true;
}

const SketchParameters& ModelDocument::sketchParameters(FeatureId featureId) const {
    const auto* value =
        std::get_if<SketchParameters>(&impl_->view().parameters()[featureIndex(featureId)]);
    if (value == nullptr) {
        throw std::out_of_range("Feature is not a sketch");
    }
    return *value;
}

bool ModelDocument::setSketchParameters(FeatureId featureId, const SketchParameters& parameters) {
    validateSketchParameters(parameters);
    const std::size_t index = featureIndex(featureId);
    auto* current = std::get_if<SketchParameters>(&impl_->view().parameters()[index]);
    if (current == nullptr) {
        throw std::out_of_range("Feature is not a sketch");
    }
    if (*current == parameters) {
        return false;
    }
    auto candidate = currentState();
    *std::get_if<SketchParameters>(&candidate.parameters.at(index)) = parameters;
    commitState(std::move(candidate));
    return true;
}

SketchElementId ModelDocument::addSketchElement(FeatureId featureId, SketchGeometry geometry) {
    SketchParameters changed = sketchParameters(featureId);
    if (!hasIdCapacity(changed.elements.nextId, 1))
        throw std::overflow_error("Sketch element IDs exhausted");
    const SketchElementId id{changed.elements.nextId++};
    changed.elements.values.push_back(SketchElement{id, std::move(geometry)});
    static_cast<void>(setSketchParameters(featureId, changed));
    return id;
}

const ExtrusionParameters& ModelDocument::extrusionParameters(FeatureId featureId) const {
    const auto* value =
        std::get_if<ExtrusionParameters>(&impl_->view().parameters()[featureIndex(featureId)]);
    if (value == nullptr) {
        throw std::out_of_range("Feature is not an extrusion");
    }
    return *value;
}

bool ModelDocument::setExtrusionParameters(FeatureId featureId,
                                           const ExtrusionParameters& parameters) {
    validateExtrusion(parameters);
    const std::size_t index = featureIndex(featureId);
    auto* current = std::get_if<ExtrusionParameters>(&impl_->view().parameters()[index]);
    if (current == nullptr) {
        throw std::out_of_range("Feature is not an extrusion");
    }
    if (*current == parameters) {
        return false;
    }
    auto candidate = currentState();
    *std::get_if<ExtrusionParameters>(&candidate.parameters.at(index)) = parameters;
    static_cast<void>(featureBodyContexts(candidate.features, candidate.parameters));
    commitState(std::move(candidate));
    return true;
}

const FilletParameters& ModelDocument::filletParameters(FeatureId featureId) const {
    const auto* value =
        std::get_if<FilletParameters>(&impl_->view().parameters()[featureIndex(featureId)]);
    if (value == nullptr)
        throw std::out_of_range("Feature is not a fillet");
    return *value;
}

bool ModelDocument::setFilletParameters(FeatureId featureId, const FilletParameters& parameters) {
    validateFilletParameters(parameters);
    const std::size_t index = featureIndex(featureId);
    auto* current = std::get_if<FilletParameters>(&impl_->view().parameters()[index]);
    if (current == nullptr)
        throw std::out_of_range("Feature is not a fillet");
    if (*current == parameters)
        return false;
    auto candidate = currentState();
    *std::get_if<FilletParameters>(&candidate.parameters.at(index)) = parameters;
    static_cast<void>(featureBodyContexts(candidate.features, candidate.parameters));
    commitState(std::move(candidate));
    return true;
}

const ChamferParameters& ModelDocument::chamferParameters(FeatureId featureId) const {
    const auto* value =
        std::get_if<ChamferParameters>(&impl_->view().parameters()[featureIndex(featureId)]);
    if (value == nullptr)
        throw std::out_of_range("Feature is not a chamfer");
    return *value;
}

bool ModelDocument::setChamferParameters(FeatureId featureId, const ChamferParameters& parameters) {
    validateChamferParameters(parameters);
    const std::size_t index = featureIndex(featureId);
    auto* current = std::get_if<ChamferParameters>(&impl_->view().parameters()[index]);
    if (current == nullptr)
        throw std::out_of_range("Feature is not a chamfer");
    if (*current == parameters)
        return false;
    auto candidate = currentState();
    *std::get_if<ChamferParameters>(&candidate.parameters.at(index)) = parameters;
    static_cast<void>(featureBodyContexts(candidate.features, candidate.parameters));
    commitState(std::move(candidate));
    return true;
}

const HoleParameters& ModelDocument::holeParameters(FeatureId featureId) const {
    const auto* value =
        std::get_if<HoleParameters>(&impl_->view().parameters()[featureIndex(featureId)]);
    if (value == nullptr)
        throw std::out_of_range("Feature is not a hole");
    return *value;
}

bool ModelDocument::setHoleParameters(FeatureId featureId, const HoleParameters& parameters) {
    validateHoleParameters(parameters);
    const std::size_t index = featureIndex(featureId);
    auto* current = std::get_if<HoleParameters>(&impl_->view().parameters()[index]);
    if (current == nullptr)
        throw std::out_of_range("Feature is not a hole");
    if (*current == parameters)
        return false;
    auto candidate = currentState();
    *std::get_if<HoleParameters>(&candidate.parameters.at(index)) = parameters;
    static_cast<void>(featureBodyContexts(candidate.features, candidate.parameters));
    commitState(std::move(candidate));
    return true;
}

const kernel::RigidTransform& ModelDocument::featurePlacement(FeatureId featureId) const {
    const auto* placement = placementOf(impl_->view().parameters()[featureIndex(featureId)]);
    if (placement == nullptr)
        throw std::out_of_range("Feature has no rigid placement");
    return *placement;
}

bool ModelDocument::setFeaturePlacement(FeatureId featureId,
                                        const kernel::RigidTransform& placement) {
    validatePlacement(placement);
    const std::size_t index = featureIndex(featureId);
    FeatureParameters changed = impl_->view().parameters()[index];
    auto* target = placementOf(changed);
    if (target == nullptr)
        throw std::out_of_range("Feature has no rigid placement");
    *target = placement;
    if (changed == impl_->view().parameters()[index]) {
        return false;
    }
    auto candidate = currentState();
    candidate.parameters.at(index) = std::move(changed);
    commitState(std::move(candidate));
    return true;
}

std::size_t ModelDocument::featureIndex(FeatureId featureId) const {
    for (std::size_t index = 0; index < impl_->view().features().size(); ++index) {
        if (impl_->view().features()[index].id == featureId) {
            return index;
        }
    }
    throw std::out_of_range("Unknown feature ID");
}

} // namespace voelund::document

namespace voelund::document {
ModelDocument::~ModelDocument() = default;
ModelDocument::ModelDocument(const ModelDocument& other)
    : impl_(std::make_unique<Impl>(*other.impl_)) {}
ModelDocument& ModelDocument::operator=(const ModelDocument& other) {
    if (this != &other) {
        auto copy = std::make_unique<Impl>(*other.impl_);
        impl_.swap(copy);
    }
    return *this;
}
ModelDocument::ModelDocument(ModelDocument&& other) noexcept = default;
ModelDocument& ModelDocument::operator=(ModelDocument&& other) noexcept = default;
std::uint64_t ModelDocument::revision() const noexcept { return impl_->native.revision(); }
} // namespace voelund::document

namespace voelund::document {
DocumentId ModelDocument::documentId() const noexcept { return impl_->native.view().id(); }
} // namespace voelund::document
