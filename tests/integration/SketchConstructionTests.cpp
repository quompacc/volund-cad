#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>

#include <cmath>
#include <iostream>

namespace {

constexpr voelund::document::SketchPlane plane{0.0, 0.0, 40.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0};

bool add(voelund::application::SketchEditorSession& editor, voelund::document::SketchPoint first,
         voelund::document::SketchPoint second, std::uint64_t expectedId) {
    return editor.setPreview(first, second) &&
           editor.confirmPreview() == voelund::document::SketchElementId{expectedId};
}

bool constructionGeometryIsExcludedFromProfiles() {
    using namespace voelund;
    application::CadSession session;
    session.newDocument();
    application::SketchEditorSession editor(session, session.addBoxFeature(), plane);
    if (!add(editor, {0.0, 0.0}, {4.0, 0.0}, 1) || !add(editor, {4.0, 0.0}, {4.0, 3.0}, 2) ||
        !add(editor, {4.0, 3.0}, {0.0, 3.0}, 3) || !add(editor, {0.0, 3.0}, {0.0, 0.0}, 4) ||
        editor.closedProfileCount() != 1 || !editor.selectElement(document::SketchElementId{1}) ||
        !editor.toggleSelectedConstruction() || !editor.isSelectedConstruction() ||
        editor.closedProfileCount() != 0 || !editor.evaluatedElements()[0].construction)
        return false;
    return editor.toggleSelectedConstruction() && !editor.isSelectedConstruction() &&
           editor.closedProfileCount() == 1;
}

bool directionalPointDimensionsAreSignedAndEditable() {
    using namespace voelund;
    application::CadSession session;
    session.newDocument();
    application::SketchEditorSession editor(session, session.addBoxFeature(), plane);
    if (!add(editor, {0.0, 0.0}, {1.0, 0.0}, 1) || !add(editor, {3.0, 4.0}, {4.0, 4.0}, 2) ||
        !editor.selectNearestReference(document::SketchElementId{1}, {0.0, 0.0}, false) ||
        !editor.selectNearestReference(document::SketchElementId{2}, {3.0, 4.0}, true) ||
        !editor.addSelectedRelation(document::SketchConstraintKind::pointDistanceX))
        return false;
    const auto horizontalId = editor.constraints().back().id;
    if (!editor.setConstraintValue(horizontalId, -6.0))
        return false;
    const auto first = std::get<document::SketchLine>(editor.elements()[0].geometry).start;
    const auto second = std::get<document::SketchLine>(editor.elements()[1].geometry).start;
    if (!editor.selectNearestReference(document::SketchElementId{1}, first, false) ||
        !editor.selectNearestReference(document::SketchElementId{2}, second, true) ||
        !editor.addSelectedRelation(document::SketchConstraintKind::pointDistanceY))
        return false;
    const auto verticalId = editor.constraints().back().id;
    if (!editor.setConstraintValue(verticalId, 0.0))
        return false;
    const auto solvedFirst = std::get<document::SketchLine>(editor.elements()[0].geometry).start;
    const auto solvedSecond = std::get<document::SketchLine>(editor.elements()[1].geometry).start;
    return std::abs(solvedSecond.x - solvedFirst.x + 6.0) <= 1.0e-7 &&
           std::abs(solvedSecond.y - solvedFirst.y) <= 1.0e-7;
}

} // namespace

int main() {
    if (!constructionGeometryIsExcludedFromProfiles()) {
        std::cerr << "Construction geometry profile exclusion failed\n";
        return 1;
    }
    if (!directionalPointDimensionsAreSignedAndEditable()) {
        std::cerr << "Directional point dimension editing failed\n";
        return 1;
    }
    return 0;
}
