// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <stdexcept>
#include <voelund/document/IdCapacity.hpp>
#include <voelund/document/SubshapeReference.hpp>

namespace voelund::document {
void validateSubshapeOrigin(const SubshapeOrigin& origin) {
    const int representations = (origin.role ? 1 : 0) + (origin.extrusionBoundary.empty() ? 0 : 1) +
                                (origin.faceExtrusion ? 1 : 0) + (origin.treatedEdge ? 1 : 0) +
                                (origin.importedFace ? 1 : 0);
    if (!hasIdCapacity(origin.feature.value(), 1) || representations != 1 ||
        (origin.role && origin.extrusionFace != ExtrusionFace::end) ||
        (origin.extrusionFace < ExtrusionFace::end || origin.extrusionFace > ExtrusionFace::side) ||
        (origin.extrusionFace == ExtrusionFace::side) != origin.sweptEdge.has_value())
        throw std::invalid_argument("Invalid support origin");
    if (origin.sweptEdge)
        validateEdgeOrigin(*origin.sweptEdge);
    if (origin.treatedEdge &&
        (*origin.treatedEdge == 0 || origin.extrusionFace != ExtrusionFace::end))
        throw std::invalid_argument("Invalid fillet or chamfer face origin");
    if (origin.importedFace &&
        (*origin.importedFace == 0 || origin.extrusionFace != ExtrusionFace::end))
        throw std::invalid_argument("Invalid imported face origin");
    if (origin.role) {
        const auto role = static_cast<int>(*origin.role);
        if (role < 0 || role > static_cast<int>(kernel::PrimitiveRole::torusSurface))
            throw std::invalid_argument("Unknown support face role");
    }
    if (origin.instances.size() > maximumSubshapeInstances)
        throw std::invalid_argument("A face is copied at most 16 times");
    for (const auto& instance : origin.instances)
        if (!hasIdCapacity(instance.feature.value(), 1) || instance.index == 0)
            throw std::invalid_argument("Invalid face copy");
    std::uint64_t previous = 0;
    for (const auto id : origin.extrusionBoundary) {
        if (!hasIdCapacity(id.value(), 1) || id.value() <= previous)
            throw std::invalid_argument("Noncanonical extrusion boundary");
        previous = id.value();
    }
}
} // namespace voelund::document
