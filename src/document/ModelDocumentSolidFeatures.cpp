#include "ModelDocumentImpl.hpp"

#include <voelund/document/LegacyBodyIdentity.hpp>
#include <voelund/kernel/NumericPolicy.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

// Features added on 2026-10-14 (M19-M27): their parameters, validation and document access.
namespace voelund::document {

void validateSketchProjections(const SketchParameters& parameters) {
    if (parameters.projections.size() > maximumSketchProjections)
        throw std::invalid_argument("A sketch projects at most 256 edges");
    for (std::size_t index = 0; index < parameters.projections.size(); ++index) {
        const auto& projection = parameters.projections[index];
        const auto element =
            std::find_if(parameters.elements.values.begin(), parameters.elements.values.end(),
                         [&](const SketchElement& item) { return item.id == projection.element; });
        if (element == parameters.elements.values.end() || !element->construction ||
            !(std::holds_alternative<SketchLine>(element->geometry) ||
              std::holds_alternative<SketchCircle>(element->geometry) ||
              std::holds_alternative<SketchArc>(element->geometry)))
            throw std::invalid_argument(
                "A projected edge is a construction line, circle or arc of its sketch");
        if (projection.body.value() == 0 || projection.edge.first == projection.edge.second)
            throw std::invalid_argument("A projected edge names a body edge");
        validateSubshapeOrigin(projection.edge.first);
        validateSubshapeOrigin(projection.edge.second);
        for (std::size_t prior = 0; prior < index; ++prior)
            if (parameters.projections[prior].element == projection.element)
                throw std::invalid_argument("Each element projects one edge");
    }
}

void validateCombineParameters(const CombineParameters& parameters) {
    if (parameters.target.value() == 0)
        throw std::invalid_argument("A combination names its target body");
    if (parameters.tools.empty() || parameters.tools.size() > maximumCombineTools)
        throw std::invalid_argument("A combination uses between 1 and 64 tool bodies");
    switch (parameters.operation) {
    case CombineOperation::join:
    case CombineOperation::cut:
    case CombineOperation::intersect:
        break;
    default:
        throw std::invalid_argument("Unknown combine operation");
    }
    for (std::size_t index = 0; index < parameters.tools.size(); ++index) {
        const BodyId tool = parameters.tools[index];
        if (tool.value() == 0 || tool == parameters.target)
            throw std::invalid_argument("A tool body is another body than the target");
        if (std::find(parameters.tools.begin(), parameters.tools.begin() + index, tool) !=
            parameters.tools.begin() + index)
            throw std::invalid_argument("Each tool body is named once");
    }
}

FeatureId ModelDocument::addCombineFeature(std::string name, const CombineParameters& parameters) {
    validateCombineParameters(parameters);
    const FeatureId id = nextFeatureId();
    auto candidate = currentState();
    ++candidate.nextFeatureId;
    candidate.features.push_back(
        FeatureDescriptor{id, FeatureKind::combine, std::move(name), std::nullopt});
    candidate.parameters.emplace_back(parameters);
    static_cast<void>(featureBodyContexts(candidate.features, candidate.parameters));
    commitState(std::move(candidate));
    return id;
}

const CombineParameters& ModelDocument::combineParameters(FeatureId featureId) const {
    const auto* value =
        std::get_if<CombineParameters>(&impl_->view().parameters()[featureIndex(featureId)]);
    if (value == nullptr)
        throw std::out_of_range("Feature is not a combination");
    return *value;
}

bool ModelDocument::setCombineParameters(FeatureId featureId, const CombineParameters& parameters) {
    validateCombineParameters(parameters);
    const std::size_t index = featureIndex(featureId);
    const auto* current = std::get_if<CombineParameters>(&impl_->view().parameters()[index]);
    if (current == nullptr)
        throw std::out_of_range("Feature is not a combination");
    if (*current == parameters)
        return false;
    auto candidate = currentState();
    *std::get_if<CombineParameters>(&candidate.parameters.at(index)) = parameters;
    static_cast<void>(featureBodyContexts(candidate.features, candidate.parameters));
    commitState(std::move(candidate));
    return true;
}

void validateSweepPath(const SweepPath& path) {
    if (path.sketch.value() == 0 || path.elements.empty() ||
        path.elements.size() > maximumSweepPathElements)
        throw std::invalid_argument("A sweep path names a sketch and 1 to 256 elements");
    for (std::size_t index = 0; index < path.elements.size(); ++index)
        if (path.elements[index].value() == 0 ||
            std::find(
                path.elements.begin(), path.elements.begin() + static_cast<std::ptrdiff_t>(index),
                path.elements[index]) != path.elements.begin() + static_cast<std::ptrdiff_t>(index))
            throw std::invalid_argument("Sweep path elements are named once");
}

void validateLoftParameters(const LoftParameters& parameters) {
    if (parameters.sections.empty() || parameters.sections.size() > maximumLoftSections)
        throw std::invalid_argument("A loft has 1 to 15 further sections");
    for (std::size_t index = 0; index < parameters.sections.size(); ++index) {
        const auto sketch = parameters.sections[index].sketch;
        if (sketch.value() == 0)
            throw std::invalid_argument("A loft section names a sketch");
        for (std::size_t other = 0; other < index; ++other)
            if (parameters.sections[other].sketch == sketch)
                throw std::invalid_argument("Each loft section is another sketch");
    }
}

void validateSweepExtrusion(const ExtrusionParameters& parameters) {
    if (parameters.revolve && parameters.revolve->pitch && parameters.symmetric)
        throw std::invalid_argument("A helix is not symmetric"); // M26
    if (!parameters.sweep && !parameters.loft)
        return;
    if (parameters.sweep)
        validateSweepPath(*parameters.sweep);
    if (parameters.loft) {
        validateLoftParameters(*parameters.loft);
        if (parameters.sweep || parameters.regions.size() != 1 || !parameters.profiles.empty())
            throw std::invalid_argument("A loft starts from exactly one region");
    }
    if (parameters.revolve || parameters.symmetric || parameters.reversed ||
        parameters.offset != 0.0 || parameters.offsetParameter || parameters.upToFace ||
        parameters.faceProfile || parameters.distanceParameter)
        throw std::invalid_argument("A sweep or loft follows its path or sections only");
}

void validateRevolveParameters(const RevolveParameters& parameters) {
    const auto& axis = parameters.axis;
    if (axis.sketchLine.has_value() + axis.baseAxis.has_value() + axis.datumAxis.has_value() != 1)
        throw std::invalid_argument("A revolve has one axis");
    if ((axis.sketchLine && axis.sketchLine->value() == 0) ||
        (axis.datumAxis && axis.datumAxis->value() == 0) ||
        (axis.baseAxis &&
         (static_cast<int>(*axis.baseAxis) < 1 || static_cast<int>(*axis.baseAxis) > 3)))
        throw std::invalid_argument("Unknown revolve axis");
    const double angle = parameters.angle.value();
    if (parameters.pitch) {
        if (!kernel::modelLength(*parameters.pitch) || !(angle > 1e-9) ||
            angle > 2000.0 * std::numbers::pi)
            throw std::invalid_argument("A helix has a positive pitch and up to 1000 turns");
        return;
    }
    if (!(angle > 1e-9) || angle > 2.0 * std::numbers::pi + 1e-12)
        throw std::invalid_argument("A revolve turns by more than 0 and at most 360 degrees");
}

void validateDatumPlaneParameters(const DatumPlaneParameters& parameters) {
    if (parameters.basePlane.has_value() == parameters.face.has_value())
        throw std::invalid_argument("A construction plane has one reference");
    if (parameters.basePlane && (static_cast<int>(*parameters.basePlane) < 1 ||
                                 static_cast<int>(*parameters.basePlane) > 3))
        throw std::invalid_argument("Unknown base plane");
    if (parameters.face) {
        if (parameters.face->body.value() == 0)
            throw std::invalid_argument("A construction plane names the body of its face");
        validateSubshapeOrigin(parameters.face->origin);
    }
    if (!kernel::modelCoordinate(parameters.offset))
        throw std::invalid_argument(kernel::modelRangeDiagnostic);
    switch (parameters.rotationAxis) {
    case DatumRotationAxis::none:
        if (parameters.angle.value() != 0.0)
            throw std::invalid_argument("An angle needs a rotation axis");
        break;
    case DatumRotationAxis::x:
    case DatumRotationAxis::y:
        if (std::abs(parameters.angle.value()) > std::numbers::pi)
            throw std::invalid_argument("The plane turns by at most 180 degrees");
        break;
    default:
        throw std::invalid_argument("Unknown rotation axis");
    }
}

void validateDatumAxisParameters(const DatumAxisParameters& parameters) {
    if (parameters.baseAxis.has_value() == parameters.edge.has_value() ||
        parameters.body.has_value() != parameters.edge.has_value())
        throw std::invalid_argument("A construction axis has one reference");
    if (parameters.baseAxis &&
        (static_cast<int>(*parameters.baseAxis) < 1 || static_cast<int>(*parameters.baseAxis) > 3))
        throw std::invalid_argument("Unknown base axis");
    if (parameters.edge) {
        if (parameters.body->value() == 0 || parameters.edge->first == parameters.edge->second)
            throw std::invalid_argument("A construction axis names a body edge");
        validateSubshapeOrigin(parameters.edge->first);
        validateSubshapeOrigin(parameters.edge->second);
    }
}

FeatureId ModelDocument::addDatumPlaneFeature(std::string name,
                                              const DatumPlaneParameters& parameters) {
    validateDatumPlaneParameters(parameters);
    const FeatureId id = nextFeatureId();
    auto candidate = currentState();
    ++candidate.nextFeatureId;
    candidate.features.push_back(
        FeatureDescriptor{id, FeatureKind::datumPlane, std::move(name), std::nullopt});
    candidate.parameters.emplace_back(parameters);
    static_cast<void>(featureBodyContexts(candidate.features, candidate.parameters));
    commitState(std::move(candidate));
    return id;
}

FeatureId ModelDocument::addDatumAxisFeature(std::string name,
                                             const DatumAxisParameters& parameters) {
    validateDatumAxisParameters(parameters);
    const FeatureId id = nextFeatureId();
    auto candidate = currentState();
    ++candidate.nextFeatureId;
    candidate.features.push_back(
        FeatureDescriptor{id, FeatureKind::datumAxis, std::move(name), std::nullopt});
    candidate.parameters.emplace_back(parameters);
    static_cast<void>(featureBodyContexts(candidate.features, candidate.parameters));
    commitState(std::move(candidate));
    return id;
}

const DatumPlaneParameters& ModelDocument::datumPlaneParameters(FeatureId featureId) const {
    const auto* value =
        std::get_if<DatumPlaneParameters>(&impl_->view().parameters()[featureIndex(featureId)]);
    if (value == nullptr)
        throw std::out_of_range("Feature is not a construction plane");
    return *value;
}

bool ModelDocument::setDatumPlaneParameters(FeatureId featureId,
                                            const DatumPlaneParameters& parameters) {
    validateDatumPlaneParameters(parameters);
    const std::size_t index = featureIndex(featureId);
    const auto* current = std::get_if<DatumPlaneParameters>(&impl_->view().parameters()[index]);
    if (current == nullptr)
        throw std::out_of_range("Feature is not a construction plane");
    if (*current == parameters)
        return false;
    auto candidate = currentState();
    *std::get_if<DatumPlaneParameters>(&candidate.parameters.at(index)) = parameters;
    static_cast<void>(featureBodyContexts(candidate.features, candidate.parameters));
    commitState(std::move(candidate));
    return true;
}

const DatumAxisParameters& ModelDocument::datumAxisParameters(FeatureId featureId) const {
    const auto* value =
        std::get_if<DatumAxisParameters>(&impl_->view().parameters()[featureIndex(featureId)]);
    if (value == nullptr)
        throw std::out_of_range("Feature is not a construction axis");
    return *value;
}

bool ModelDocument::setDatumAxisParameters(FeatureId featureId,
                                           const DatumAxisParameters& parameters) {
    validateDatumAxisParameters(parameters);
    const std::size_t index = featureIndex(featureId);
    const auto* current = std::get_if<DatumAxisParameters>(&impl_->view().parameters()[index]);
    if (current == nullptr)
        throw std::out_of_range("Feature is not a construction axis");
    if (*current == parameters)
        return false;
    auto candidate = currentState();
    *std::get_if<DatumAxisParameters>(&candidate.parameters.at(index)) = parameters;
    static_cast<void>(featureBodyContexts(candidate.features, candidate.parameters));
    commitState(std::move(candidate));
    return true;
}

void validateShellParameters(const ShellParameters& parameters) {
    if (!kernel::modelLength(parameters.thickness))
        throw std::invalid_argument(kernel::modelRangeDiagnostic);
    if (parameters.body.value() == 0)
        throw std::invalid_argument("A shell names its body");
    if (parameters.openFaces.empty() || parameters.openFaces.size() > maximumShellFaces)
        throw std::invalid_argument("A shell opens between 1 and 256 faces");
    for (std::size_t index = 0; index < parameters.openFaces.size(); ++index) {
        validateSubshapeOrigin(parameters.openFaces[index]);
        if (std::find(parameters.openFaces.begin(), parameters.openFaces.begin() + index,
                      parameters.openFaces[index]) != parameters.openFaces.begin() + index)
            throw std::invalid_argument("Each open face is named once");
    }
}

FeatureId ModelDocument::addShellFeature(std::string name, const ShellParameters& parameters) {
    validateShellParameters(parameters);
    const FeatureId id = nextFeatureId();
    auto candidate = currentState();
    ++candidate.nextFeatureId;
    candidate.features.push_back(
        FeatureDescriptor{id, FeatureKind::shell, std::move(name), std::nullopt});
    candidate.parameters.emplace_back(parameters);
    static_cast<void>(featureBodyContexts(candidate.features, candidate.parameters));
    commitState(std::move(candidate));
    return id;
}

const ShellParameters& ModelDocument::shellParameters(FeatureId featureId) const {
    const auto* value =
        std::get_if<ShellParameters>(&impl_->view().parameters()[featureIndex(featureId)]);
    if (value == nullptr)
        throw std::out_of_range("Feature is not a shell");
    return *value;
}

bool ModelDocument::setShellParameters(FeatureId featureId, const ShellParameters& parameters) {
    validateShellParameters(parameters);
    const std::size_t index = featureIndex(featureId);
    const auto* current = std::get_if<ShellParameters>(&impl_->view().parameters()[index]);
    if (current == nullptr)
        throw std::out_of_range("Feature is not a shell");
    if (*current == parameters)
        return false;
    auto candidate = currentState();
    *std::get_if<ShellParameters>(&candidate.parameters.at(index)) = parameters;
    static_cast<void>(featureBodyContexts(candidate.features, candidate.parameters));
    commitState(std::move(candidate));
    return true;
}

} // namespace voelund::document
