#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>

#include <cmath>
#include <iostream>

namespace {

constexpr voelund::document::SketchPlane plane{0.0, 0.0, 40.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0};

bool addCircle(voelund::application::SketchEditorSession& editor,
               voelund::document::SketchPoint center, voelund::document::SketchPoint rim,
               std::uint64_t expectedId) {
    return editor.setPreview(center, rim) &&
           editor.confirmPreview() == voelund::document::SketchElementId{expectedId};
}

bool circlesBecomeConcentricAndDiameterRemainsEditable() {
    using namespace voelund;
    application::CadSession session;
    session.newDocument();
    application::SketchEditorSession editor(session, session.addBoxFeature(), plane);
    editor.setTool(application::SketchDrawingTool::circle);
    if (!addCircle(editor, {0.0, 0.0}, {2.0, 0.0}, 1) ||
        !addCircle(editor, {8.0, 3.0}, {11.0, 3.0}, 2) ||
        !editor.selectNearestReference(document::SketchElementId{1}, {0.0, 0.0}, false) ||
        !editor.selectNearestReference(document::SketchElementId{2}, {8.0, 3.0}, true) ||
        !editor.canAddSelectedRelation(document::SketchConstraintKind::concentric) ||
        !editor.addSelectedRelation(document::SketchConstraintKind::concentric))
        return false;
    const auto first = std::get<document::SketchCircle>(editor.elements()[0].geometry);
    const auto second = std::get<document::SketchCircle>(editor.elements()[1].geometry);
    if (std::hypot(first.center.x - second.center.x, first.center.y - second.center.y) > 1.0e-7 ||
        !editor.selectNearestReference(document::SketchElementId{2}, second.center, false) ||
        !editor.canAddSelectedRelation(document::SketchConstraintKind::diameter) ||
        !editor.addSelectedRelation(document::SketchConstraintKind::diameter))
        return false;
    const auto diameterId = editor.constraints().back().id;
    if (editor.constraints().back().kind != document::SketchConstraintKind::diameter ||
        !editor.setConstraintValue(diameterId, 12.0))
        return false;
    const auto solved = std::get<document::SketchCircle>(editor.elements()[1].geometry);
    return std::abs(solved.radius - 6.0) <= 1.0e-7 &&
           editor.constraintValue(diameterId) == std::optional<double>{12.0};
}

} // namespace

int main() {
    if (!circlesBecomeConcentricAndDiameterRemainsEditable()) {
        std::cerr << "Concentric or diameter editor relation failed\n";
        return 1;
    }
    return 0;
}
