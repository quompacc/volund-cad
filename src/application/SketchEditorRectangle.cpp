#include <voelund/application/SketchEditorSession.hpp>

#include <array>

namespace voelund::application {

document::SketchElementId
SketchEditorSession::confirmRectangle(const document::SketchRectangle& rectangle) {
    const std::array points{
        rectangle.first,
        document::SketchPoint{rectangle.second.x, rectangle.first.y},
        rectangle.second,
        document::SketchPoint{rectangle.first.x, rectangle.second.y},
    };
    std::array<document::SketchElementId, 4> ids{
        document::SketchElementId{elements_.nextId++},
        document::SketchElementId{elements_.nextId++},
        document::SketchElementId{elements_.nextId++},
        document::SketchElementId{elements_.nextId++},
    };

    for (std::size_t index = 0; index < ids.size(); ++index) {
        const auto lineConstraint = index % 2 == 0 ? document::SketchLineConstraint::horizontal
                                                   : document::SketchLineConstraint::vertical;
        elements_.values.push_back(
            {ids[index], document::SketchLine{points[index], points[(index + 1) % points.size()],
                                              lineConstraint}});
        constraints_.values.push_back({document::SketchConstraintId{constraints_.nextId++},
                                       index % 2 == 0 ? document::SketchConstraintKind::horizontal
                                                      : document::SketchConstraintKind::vertical,
                                       {ids[index], document::SketchPointPosition::start},
                                       std::nullopt,
                                       0.0});
        constraints_.values.push_back(
            {document::SketchConstraintId{constraints_.nextId++},
             document::SketchConstraintKind::coincident,
             {ids[index], document::SketchPointPosition::end},
             document::SketchGeometryReference{ids[(index + 1) % ids.size()],
                                               document::SketchPointPosition::start},
             0.0});
    }

    clearPreview();
    selectedElement_ = ids.front();
    selectedReferences_.clear();
    static_cast<void>(solveConstraints());
    return ids.front();
}

} // namespace voelund::application
