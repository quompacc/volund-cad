#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>

namespace voelund::application {
bool SketchEditorSession::addProjectedEdgeReference(
    document::SketchGeometryReference point, const document::ProjectedEdgeReference& reference) {
    if (!active_ || !supportBinding_ || !supportStamp_ ||
        *supportStamp_ != session_.sceneStatus().document || !canAllocateIds(0, 2))
        return false;
    try {
        document::validateProjectedEdgeReference(reference);
    } catch (const std::invalid_argument&) {
        return false;
    }
    const auto previousElements = elements_;
    const auto previousConstraints = constraints_;
    for (const auto kind : {document::SketchConstraintKind::coordinateX,
                            document::SketchConstraintKind::coordinateY}) {
        document::SketchConstraint constraint{document::SketchConstraintId{constraints_.nextId++},
                                              kind, point, std::nullopt};
        constraint.projectedEdge = reference;
        constraints_.values.push_back(std::move(constraint));
    }
    if (solveConstraints())
        return true;
    elements_ = previousElements;
    constraints_ = previousConstraints;
    static_cast<void>(solveConstraints());
    return false;
}
} // namespace voelund::application
