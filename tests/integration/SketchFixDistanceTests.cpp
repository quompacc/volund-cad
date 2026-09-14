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

double pointLineDistance(const voelund::document::SketchPoint& point,
                         const voelund::document::SketchLine& line) {
    const double x = line.end.x - line.start.x;
    const double y = line.end.y - line.start.y;
    return std::abs(x * (point.y - line.start.y) - y * (point.x - line.start.x)) / std::hypot(x, y);
}

bool fixedGeometryAndPointLineDistanceRemainParametric() {
    using namespace voelund;
    application::CadSession session;
    session.newDocument();
    application::SketchEditorSession editor(
        session, voelund::test::requireCommittedFeature(session.createBoxFeature()), plane);
    if (!addLine(editor, {1.0, 2.0}, {5.0, 3.0}, 1) ||
        !editor.selectNearestReference(document::SketchElementId{1}, {1.0, 2.0}, false) ||
        !editor.addSelectedRelation(document::SketchConstraintKind::fixed))
        return false;
    const auto fixed = std::get<document::SketchLine>(editor.elements()[0].geometry);
    static_cast<void>(editor.translateSelected(4.0, -3.0));
    if (std::get<document::SketchLine>(editor.elements()[0].geometry) != fixed ||
        !addLine(editor, {9.0, 7.0}, {10.0, 7.0}, 2) ||
        !addLine(editor, {0.0, 0.0}, {12.0, 0.0}, 3))
        return false;
    auto source = std::get<document::SketchLine>(editor.elements()[1].geometry).start;
    auto target = std::get<document::SketchLine>(editor.elements()[2].geometry);
    if (!editor.selectNearestReference(document::SketchElementId{2}, source, false) ||
        !editor.selectNearestReference(document::SketchElementId{3}, target.start, true) ||
        !editor.addSelectedRelation(document::SketchConstraintKind::pointLineDistance))
        return false;
    const auto distanceId = editor.constraints().back().id;
    if (!editor.setConstraintValue(distanceId, 4.0))
        return false;
    source = std::get<document::SketchLine>(editor.elements()[1].geometry).start;
    target = std::get<document::SketchLine>(editor.elements()[2].geometry);
    return std::abs(pointLineDistance(source, target) - 4.0) <= 1.0e-7;
}

} // namespace

int main() {
    if (!fixedGeometryAndPointLineDistanceRemainParametric()) {
        std::cerr << "Fixed geometry or point-line distance editor flow failed\n";
        return 1;
    }
    return 0;
}
