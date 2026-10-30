// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "DocumentSerializationDetail.hpp"
#include "ModelDocumentImpl.hpp"
#include "NativeBodyAttributes.hpp"
#include <voelund/document/IdCapacity.hpp>
#include <voelund/document/LegacyBodyIdentity.hpp>

#include <voelund/kernel/StepExchange.hpp>

#include <TopAbs_ShapeEnum.hxx>

#include <stdexcept>
#include <string>
#include <utility>

// M17: solids read from a STEP file, embedded as an import feature with one part feature per
// further solid.
namespace voelund::document {
namespace {

constexpr std::size_t maximumImportTextBytes = 4096;

// A body name: no NUL, at most maximumBodyNameBytes, never cut inside a UTF-8 character.
std::string bodyName(std::string name) {
    std::erase(name, '\0');
    if (name.size() > maximumBodyNameBytes) {
        std::size_t end = maximumBodyNameBytes;
        while (end > 0 && (static_cast<unsigned char>(name[end]) & 0xC0) == 0x80)
            --end;
        name.resize(end);
    }
    return name;
}

} // namespace

void validateImportParameters(const ImportParameters& parameters) {
    if (parameters.solids.empty() || parameters.solids.size() > maximumImportedSolids)
        throw std::invalid_argument("An import holds 1 to 10000 solids");
    for (const auto& solid : parameters.solids)
        if (solid.solid.IsNull() || solid.solid.ShapeType() != TopAbs_SOLID || !solid.brep ||
            solid.brep->empty() || solid.name.find('\0') != std::string::npos)
            throw std::invalid_argument("An import holds solids only");
    std::size_t bytes = 0;
    for (const auto& solid : parameters.solids) {
        if (solid.name.size() > maximumImportTextBytes)
            throw std::invalid_argument("Imported solid name too long");
        bytes += solid.brep->size();
    }
    if (bytes > detail::maximumImportedBytes)
        throw std::invalid_argument("The imported solids are too large to store in a document");
    if (parameters.fileName.find('\0') != std::string::npos ||
        parameters.sourceUnit.find('\0') != std::string::npos ||
        parameters.fileName.size() > maximumImportTextBytes ||
        parameters.sourceUnit.size() > maximumImportTextBytes)
        throw std::invalid_argument("Invalid import source");
}

ImportedSolid importedSolid(const TopoDS_Shape& solid, std::string name) {
    if (solid.IsNull() || solid.ShapeType() != TopAbs_SOLID)
        throw std::invalid_argument("An import holds solids only");
    return importedSolid(kernel::writeShapeBytes(solid), std::move(name));
}

ImportedSolid importedSolid(std::string brep, std::string name) {
    auto solid = kernel::readShapeBytes(brep);
    if (solid.IsNull())
        throw std::invalid_argument("The stored solid of an import is not valid");
    return {std::move(name), std::make_shared<const std::string>(std::move(brep)),
            std::move(solid)};
}

std::optional<FeatureId> companionOwner(const FeatureParameters& parameters) noexcept {
    if (const auto* copy = std::get_if<PatternCopyParameters>(&parameters))
        return copy->pattern;
    if (const auto* part = std::get_if<ImportPartParameters>(&parameters))
        return part->import;
    if (const auto* half = std::get_if<SplitPartParameters>(&parameters)) // N12a
        return half->split;
    return std::nullopt;
}

FeatureId ModelDocument::addImportFeature(std::string name, ImportParameters parameters,
                                          std::vector<std::optional<std::uint32_t>> colors) {
    colors.resize(parameters.solids.size());
    validateImportParameters(parameters);
    const std::size_t parts = parameters.solids.size() - 1;
    if (!hasIdCapacity(nextFeatureId().value(), parts + 1))
        throw std::invalid_argument("No feature IDs left for the import");
    const FeatureId id = nextFeatureId();
    auto candidate = currentState();
    candidate.nextFeatureId += parts + 1;
    std::vector<std::string> names;
    for (std::size_t index = 0; index < parameters.solids.size(); ++index) {
        const auto& own = parameters.solids[index].name;
        names.push_back(bodyName(own.empty() ? name + "." + std::to_string(index + 1) : own));
    }
    candidate.features.push_back(
        FeatureDescriptor{id, FeatureKind::stepImport, std::move(name), std::nullopt});
    candidate.parameters.emplace_back(std::move(parameters));
    setBodyAttributes(candidate.bodyAttributes, id.value(),
                      BodyAttributes{names[0], false, colors[0], {}});
    for (std::size_t index = 1; index <= parts; ++index) {
        const FeatureId part{id.value() + index};
        candidate.features.push_back(
            FeatureDescriptor{part, FeatureKind::importPart, names[index], std::nullopt});
        candidate.parameters.emplace_back(
            ImportPartParameters{id, static_cast<std::uint32_t>(index)});
        setBodyAttributes(candidate.bodyAttributes, part.value(),
                          BodyAttributes{names[index], false, colors[index], {}});
    }
    static_cast<void>(featureBodyContexts(candidate.features, candidate.parameters));
    validateBodyAttributes(candidate.bodyAttributes, candidate.features, candidate.parameters);
    commitState(std::move(candidate));
    return id;
}

const ImportParameters& ModelDocument::importParameters(FeatureId featureId) const {
    const auto* value =
        std::get_if<ImportParameters>(&impl_->view().parameters()[featureIndex(featureId)]);
    if (value == nullptr)
        throw std::out_of_range("Feature is not an import");
    return *value;
}

} // namespace voelund::document
