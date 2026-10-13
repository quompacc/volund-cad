#include <voelund/document/ModelDocument.hpp>
#include <voelund/kernel/NumericPolicy.hpp>

#include <stdexcept>

// M05/M07/M08: validation of the fillet, chamfer and hole parameters.
namespace voelund::document {
namespace {
// M05/M07: the body, 1-256 distinct edges between two different faces, a model length.
void validateEdgeTreatment(BodyId body, const std::vector<FilletEdge>& edges, double value) {
    if (!kernel::modelLength(value))
        throw std::invalid_argument(kernel::modelRangeDiagnostic);
    if (body.value() == 0)
        throw std::invalid_argument("A fillet or chamfer names its body");
    if (edges.empty() || edges.size() > maximumFilletEdges)
        throw std::invalid_argument("A fillet or chamfer treats between 1 and 256 edges");
    for (std::size_t index = 0; index < edges.size(); ++index) {
        const FilletEdge& edge = edges[index];
        validateSubshapeOrigin(edge.first);
        validateSubshapeOrigin(edge.second);
        if (edge.first == edge.second)
            throw std::invalid_argument("A fillet edge lies between two different faces");
        if (edge.id == 0)
            throw std::invalid_argument("A fillet or chamfer edge has an id");
        for (std::size_t prior = 0; prior < index; ++prior) {
            const FilletEdge& other = edges[prior];
            if ((other.first == edge.first && other.second == edge.second) ||
                (other.first == edge.second && other.second == edge.first) || other.id == edge.id)
                throw std::invalid_argument("Fillet and chamfer edges must be unique");
        }
    }
}
} // namespace

void validateFilletParameters(const FilletParameters& parameters) {
    validateEdgeTreatment(parameters.body, parameters.edges, parameters.radius);
}

void validateChamferParameters(const ChamferParameters& parameters) {
    validateEdgeTreatment(parameters.body, parameters.edges, parameters.distance);
}

void validateHoleParameters(const HoleParameters& parameters) {
    if (!kernel::modelLength(parameters.diameter) ||
        (parameters.depth && !kernel::modelLength(*parameters.depth)) ||
        !kernel::modelCoordinate(parameters.x) || !kernel::modelCoordinate(parameters.y))
        throw std::invalid_argument(kernel::modelRangeDiagnostic);
    if (parameters.face.body.value() == 0)
        throw std::invalid_argument("A hole names the body of its face");
    validateSubshapeOrigin(parameters.face.origin);
}

} // namespace voelund::document
