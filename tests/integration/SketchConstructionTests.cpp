#include "../CommandResultAssertions.hpp"
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
    application::SketchEditorSession editor(
        session, voelund::test::requireCommittedFeature(session.createBoxFeature()), plane);
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
    application::SketchEditorSession editor(
        session, voelund::test::requireCommittedFeature(session.createBoxFeature()), plane);
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

bool midpointAndConstructionBisectorRemainParametric() {
    using namespace voelund;
    application::CadSession session;
    session.newDocument();
    application::SketchEditorSession editor(
        session, voelund::test::requireCommittedFeature(session.createBoxFeature()), plane);
    if (!add(editor, {2.0, 3.0}, {3.0, 3.0}, 1) || !add(editor, {0.0, 0.0}, {10.0, 0.0}, 2) ||
        !editor.selectNearestReference(document::SketchElementId{1}, {2.0, 3.0}, false) ||
        !editor.selectNearestReference(document::SketchElementId{2}, {0.0, 0.0}, true) ||
        !editor.addSelectedRelation(document::SketchConstraintKind::midpoint))
        return false;
    const auto midpoint = std::get<document::SketchLine>(editor.elements()[0].geometry).start;
    const auto midpointTarget = std::get<document::SketchLine>(editor.elements()[1].geometry);
    if (std::abs(midpoint.x - (midpointTarget.start.x + midpointTarget.end.x) * 0.5) > 1.0e-7 ||
        std::abs(midpoint.y - (midpointTarget.start.y + midpointTarget.end.y) * 0.5) > 1.0e-7 ||
        !add(editor, {14.0, -4.0}, {14.0, 4.0}, 3) || !add(editor, {10.0, 0.0}, {20.0, 2.0}, 4) ||
        !editor.selectElement(document::SketchElementId{3}) ||
        !editor.toggleSelectedConstruction() ||
        !editor.selectNearestReference(document::SketchElementId{3}, {14.0, -4.0}, false) ||
        !editor.selectNearestReference(document::SketchElementId{4}, {10.0, 0.0}, true) ||
        !editor.addSelectedRelation(document::SketchConstraintKind::perpendicularBisector))
        return false;
    const auto& axis = std::get<document::SketchLine>(editor.elements()[2].geometry);
    const auto& target = std::get<document::SketchLine>(editor.elements()[3].geometry);
    const double dot = (axis.end.x - axis.start.x) * (target.end.x - target.start.x) +
                       (axis.end.y - axis.start.y) * (target.end.y - target.start.y);
    const double first = std::hypot(axis.start.x - target.start.x, axis.start.y - target.start.y);
    const double second = std::hypot(axis.start.x - target.end.x, axis.start.y - target.end.y);
    return std::abs(dot) <= 1.0e-7 && std::abs(first - second) <= 1.0e-7 &&
           editor.elements()[2].construction;
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
    if (!midpointAndConstructionBisectorRemainParametric()) {
        std::cerr << "Midpoint or construction bisector relation failed\n";
        return 1;
    }
    return 0;
}
