#include "../CommandResultAssertions.hpp"
#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>

#include <cmath>
#include <iostream>

namespace {

constexpr voelund::document::SketchPlane plane{0.0, 0.0, 40.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0};

bool addLine(voelund::application::SketchEditorSession& editor,
             voelund::document::SketchPoint first, voelund::document::SketchPoint second,
             std::uint64_t id) {
    return editor.setPreview(first, second) &&
           editor.confirmPreview() == voelund::document::SketchElementId{id};
}

bool lineAndPointAlignmentsRemainParametric() {
    using namespace voelund;
    application::CadSession session;
    session.newDocument();
    application::SketchEditorSession editor(
        session, voelund::test::requireCommittedFeature(session.createBoxFeature()), plane);
    if (!addLine(editor, {0.0, 0.0}, {4.0, 1.0}, 1) ||
        !addLine(editor, {8.0, 3.0}, {12.0, 5.0}, 2) ||
        !addLine(editor, {0.0, 8.0}, {3.0, 10.0}, 3) ||
        !addLine(editor, {7.0, 12.0}, {11.0, 15.0}, 4))
        return false;
    auto first = std::get<document::SketchLine>(editor.elements()[0].geometry);
    auto second = std::get<document::SketchLine>(editor.elements()[1].geometry);
    if (!editor.selectNearestReference(document::SketchElementId{1}, first.start, false) ||
        !editor.selectNearestReference(document::SketchElementId{2}, second.start, true) ||
        !editor.addSelectedRelation(document::SketchConstraintKind::collinear))
        return false;
    first = std::get<document::SketchLine>(editor.elements()[0].geometry);
    second = std::get<document::SketchLine>(editor.elements()[1].geometry);
    const double directionX = second.end.x - second.start.x;
    const double directionY = second.end.y - second.start.y;
    const double firstCross = directionX * (first.start.y - second.start.y) -
                              directionY * (first.start.x - second.start.x);
    const double secondCross =
        directionX * (first.end.y - second.start.y) - directionY * (first.end.x - second.start.x);
    auto third = std::get<document::SketchLine>(editor.elements()[2].geometry);
    auto fourth = std::get<document::SketchLine>(editor.elements()[3].geometry);
    if (std::abs(firstCross) > 1.0e-7 || std::abs(secondCross) > 1.0e-7 ||
        !editor.selectNearestReference(document::SketchElementId{3}, third.start, false) ||
        !editor.selectNearestReference(document::SketchElementId{4}, fourth.start, true) ||
        !editor.addSelectedRelation(document::SketchConstraintKind::horizontalPoints))
        return false;
    third = std::get<document::SketchLine>(editor.elements()[2].geometry);
    fourth = std::get<document::SketchLine>(editor.elements()[3].geometry);
    if (!editor.selectNearestReference(document::SketchElementId{3}, third.end, false) ||
        !editor.selectNearestReference(document::SketchElementId{4}, fourth.end, true) ||
        !editor.addSelectedRelation(document::SketchConstraintKind::verticalPoints))
        return false;
    third = std::get<document::SketchLine>(editor.elements()[2].geometry);
    fourth = std::get<document::SketchLine>(editor.elements()[3].geometry);
    return std::abs(third.start.y - fourth.start.y) <= 1.0e-7 &&
           std::abs(third.end.x - fourth.end.x) <= 1.0e-7;
}

bool automaticCoincidenceUsesTheSolver() {
    using namespace voelund;
    application::CadSession session;
    session.newDocument();
    application::SketchEditorSession editor(
        session, voelund::test::requireCommittedFeature(session.createBoxFeature()), plane);
    if (!addLine(editor, {0.0, 0.0}, {4.0, 0.0}, 1) ||
        !addLine(editor, {7.0, 2.0}, {10.0, 2.0}, 2) ||
        !editor.addCoincidentReferences(
            {document::SketchElementId{2}, document::SketchPointPosition::start},
            {document::SketchElementId{1}, document::SketchPointPosition::end}))
        return false;
    const auto first = std::get<document::SketchLine>(editor.elements()[0].geometry);
    const auto second = std::get<document::SketchLine>(editor.elements()[1].geometry);
    return std::hypot(second.start.x - first.end.x, second.start.y - first.end.y) <= 1.0e-7 &&
           editor.constraints().back().kind == document::SketchConstraintKind::coincident;
}

} // namespace

int main() {
    if (!lineAndPointAlignmentsRemainParametric()) {
        std::cerr << "Collinear or point alignment relation failed\n";
        return 1;
    }
    if (!automaticCoincidenceUsesTheSolver()) {
        std::cerr << "Automatic coincidence relation failed\n";
        return 1;
    }
    return 0;
}
