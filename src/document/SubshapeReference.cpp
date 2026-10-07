#include <stdexcept>
#include <voelund/document/IdCapacity.hpp>
#include <voelund/document/SubshapeReference.hpp>

namespace voelund::document {
void validateSubshapeOrigin(const SubshapeOrigin& origin) {
    const int representations = (origin.role ? 1 : 0) + (origin.extrusionBoundary.empty() ? 0 : 1) +
                                (origin.faceExtrusionCap ? 1 : 0);
    if (!hasIdCapacity(origin.feature.value(), 1) || representations != 1)
        throw std::invalid_argument("Invalid support origin");
    if (origin.role) {
        const auto role = static_cast<int>(*origin.role);
        if (role < 0 || role > static_cast<int>(kernel::PrimitiveRole::cylinderSide))
            throw std::invalid_argument("Unknown support face role");
    }
    std::uint64_t previous = 0;
    for (const auto id : origin.extrusionBoundary) {
        if (!hasIdCapacity(id.value(), 1) || id.value() <= previous)
            throw std::invalid_argument("Noncanonical extrusion boundary");
        previous = id.value();
    }
}
} // namespace voelund::document
