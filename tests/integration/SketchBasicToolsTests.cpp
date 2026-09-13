#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <variant>

namespace {

using voelund::application::CadSession;
using voelund::application::SketchDrawingTool;
using voelund::application::SketchEditorSession;
using voelund::document::SketchElementId;
using voelund::document::SketchLine;
using voelund::document::SketchPoint;

constexpr voelund::document::SketchPlane plane{0.0, 0.0, 40.0, 0.0, 0.0, 1.0,
                                                1.0, 0.0, 0.0};

bool addLine(SketchEditorSession& editor, SketchPoint first, SketchPoint second) {
    editor.setTool(SketchDrawingTool::line);
    return editor.setPreview(first, second) && editor.confirmPreview().has_value();
}

bool addCircle(SketchEditorSession& editor, SketchPoint center, SketchPoint radialPoint) {
    editor.setTool(SketchDrawingTool::circle);
    return editor.setPreview(center, radialPoint) && editor.confirmPreview().has_value();
}

bool compoundDrawingToolsCreateProfiles() {
    CadSession session;
    session.newDocument();
    const auto support = session.addBoxFeature();
    SketchEditorSession polygon(session, support, plane);
    polygon.setTool(SketchDrawingTool::polygon);
    polygon.setPolygonSides(6);
    if (!polygon.setPreview({0.0, 0.0}, {8.0, 0.0}) || !polygon.confirmPreview().has_value() ||
        polygon.elements().size() != 6 || polygon.closedProfileCount() != 1 ||
        !polygon.canAccept())
        return false;

    SketchEditorSession slot(session, support, plane);
    slot.setTool(SketchDrawingTool::slot);
    if (!slot.setPreview({0.0, 0.0}, {12.0, 0.0}, SketchPoint{0.0, 3.0}) ||
        !slot.confirmPreview().has_value() || slot.elements().size() != 4 ||
        slot.closedProfileCount() != 1 || !slot.canAccept())
        return false;
    const auto& rightCap =
        std::get<voelund::document::SketchArc>(slot.elements()[1].geometry);
    const auto& leftCap =
        std::get<voelund::document::SketchArc>(slot.elements()[3].geometry);
    const double rightMidpointX = rightCap.center.x + rightCap.radius *
        std::cos((rightCap.startAngle + rightCap.endAngle) * 0.5);
    const double leftMidpointX = leftCap.center.x + leftCap.radius *
        std::cos((leftCap.startAngle + leftCap.endAngle) * 0.5);
    if (rightMidpointX <= 12.0 || leftMidpointX >= 0.0)
        return false;

    SketchEditorSession arc(session, support, plane);
    arc.setTool(SketchDrawingTool::threePointArc);
    if (!arc.setPreview({-5.0, 0.0}, {0.0, 5.0}, SketchPoint{5.0, 0.0}) ||
        !arc.confirmPreview().has_value() ||
        !std::holds_alternative<voelund::document::SketchArc>(arc.elements().front().geometry))
        return false;

    SketchEditorSession ellipse(session, support, plane);
    ellipse.setTool(SketchDrawingTool::ellipse);
    if (!ellipse.setPreview({0.0, 0.0}, {8.0, 0.0}, SketchPoint{0.0, 3.0}) ||
        !ellipse.confirmPreview().has_value() || ellipse.closedProfileCount() != 1 ||
        ellipse.evaluatedElements().front().shape.IsNull())
        return false;
    const auto& value = std::get<voelund::document::SketchEllipse>(ellipse.elements().front().geometry);
    return std::abs(value.majorRadius - 8.0) < 1.0e-6 &&
           std::abs(value.minorRadius - 3.0) < 1.0e-6;
}

bool everyDrawingToolHasLivePreview() {
    CadSession session;
    session.newDocument();
    const auto support = session.addBoxFeature();
    SketchEditorSession editor(session, support, plane);

    const auto previewCount = [&](SketchDrawingTool tool, std::optional<SketchPoint> third,
                                  std::size_t expected) {
        editor.setTool(tool);
        if (!editor.setPreview({0.0, 0.0}, {8.0, 5.0}, third)) {
            std::cerr << "Preview tool " << static_cast<int>(tool) << " rejected geometry\n";
            return false;
        }
        const auto evaluated = editor.evaluatedElements();
        const bool valid = evaluated.size() == expected &&
                           std::ranges::none_of(evaluated, [](const auto& item) {
                               return item.shape.IsNull();
                           });
        if (!valid)
            std::cerr << "Preview tool " << static_cast<int>(tool) << " produced "
                      << evaluated.size() << " of " << expected << " shapes\n";
        return valid;
    };

    if (!previewCount(SketchDrawingTool::line, std::nullopt, 1) ||
        !previewCount(SketchDrawingTool::polyline, std::nullopt, 1) ||
        !previewCount(SketchDrawingTool::rectangle, std::nullopt, 1) ||
        !previewCount(SketchDrawingTool::circle, std::nullopt, 1) ||
        !previewCount(SketchDrawingTool::polygon, std::nullopt, 6))
        return false;

    for (const auto tool : {SketchDrawingTool::arc, SketchDrawingTool::threePointArc,
                            SketchDrawingTool::ellipse, SketchDrawingTool::slot}) {
        if (!previewCount(tool, std::nullopt, 1) || editor.confirmPreview().has_value() ||
            !editor.elements().empty()) {
            std::cerr << "Incomplete preview tool " << static_cast<int>(tool)
                      << " was accepted or persisted\n";
            return false;
        }
    }

    return previewCount(SketchDrawingTool::arc, SketchPoint{0.0, 8.0}, 1) &&
           previewCount(SketchDrawingTool::threePointArc, SketchPoint{8.0, 8.0}, 1) &&
           previewCount(SketchDrawingTool::ellipse, SketchPoint{0.0, 3.0}, 1) &&
           previewCount(SketchDrawingTool::slot, SketchPoint{0.0, 3.0}, 4);
}

bool lineEditingOperationsWork() {
    CadSession session;
    session.newDocument();
    const auto support = session.addBoxFeature();
    SketchEditorSession trim(session, support, plane);
    if (!addLine(trim, {0.0, 0.0}, {10.0, 0.0}) ||
        !addLine(trim, {5.0, -5.0}, {5.0, 5.0}) ||
        !trim.selectNearestReference(SketchElementId{1}, {8.0, 0.0}, false) ||
        !trim.trimSelected())
        return false;
    const auto& trimmed = std::get<SketchLine>(trim.elements().front().geometry);
    if (std::abs(trimmed.end.x - 5.0) > 1.0e-6)
        return false;

    SketchEditorSession extend(session, support, plane);
    if (!addLine(extend, {0.0, 0.0}, {4.0, 0.0}) ||
        !addLine(extend, {8.0, -2.0}, {8.0, 2.0}) ||
        !extend.selectNearestReference(SketchElementId{1}, {4.0, 0.0}, false) ||
        !extend.extendSelected())
        return false;
    const auto& extended = std::get<SketchLine>(extend.elements().front().geometry);
    if (std::abs(extended.end.x - 8.0) > 1.0e-6)
        return false;

    SketchEditorSession split(session, support, plane);
    if (!addLine(split, {0.0, 0.0}, {10.0, 0.0}) ||
        !split.selectNearestReference(SketchElementId{1}, {4.0, 0.0}, false) ||
        !split.splitSelected() || split.elements().size() != 2 || split.constraints().size() != 1)
        return false;
    return std::abs(std::get<SketchLine>(split.elements()[0].geometry).end.x - 4.0) < 1.0e-6;
}

bool transformationsAndLocalHistoryWork() {
    CadSession session;
    session.newDocument();
    const auto support = session.addBoxFeature();
    SketchEditorSession editor(session, support, plane);
    if (!addLine(editor, {0.0, 0.0}, {4.0, 0.0}))
        return false;
    editor.commitHistory();
    if (!editor.selectNearestReference(SketchElementId{1}, {4.0, 0.0}, false) ||
        !editor.offsetSelected(2.0))
        return false;
    editor.commitHistory();
    if (editor.elements().size() != 2 || !editor.canUndo() || !editor.undo() ||
        editor.elements().size() != 1 || !editor.canRedo() || !editor.redo() ||
        editor.elements().size() != 2)
        return false;
    if (!editor.selectNearestReference(SketchElementId{1}, {4.0, 0.0}, false) ||
        !editor.linearPatternSelected(3, 10.0))
        return false;
    return editor.elements().size() == 4;
}

bool remainingOperationsAndReferencesWork() {
    CadSession session;
    session.newDocument();
    const auto support = session.addBoxFeature();

    SketchEditorSession fillet(session, support, plane);
    if (!addLine(fillet, {0.0, 0.0}, {10.0, 0.0}) ||
        !addLine(fillet, {10.0, 0.0}, {10.0, 10.0}) ||
        !fillet.selectNearestReference(SketchElementId{1}, {9.0, 0.0}, false) ||
        !fillet.selectNearestReference(SketchElementId{2}, {10.0, 1.0}, true) ||
        !fillet.filletSelected(2.0) || fillet.elements().size() != 3)
        return false;

    SketchEditorSession mirror(session, support, plane);
    if (!addLine(mirror, {2.0, 1.0}, {4.0, 1.0}) ||
        !addLine(mirror, {0.0, -5.0}, {0.0, 5.0}) ||
        !mirror.selectNearestReference(SketchElementId{1}, {3.0, 1.0}, false) ||
        !mirror.selectNearestReference(SketchElementId{2}, {0.0, 0.0}, true) ||
        !mirror.mirrorSelected() || mirror.elements().size() != 3)
        return false;
    const auto& mirrored = std::get<SketchLine>(mirror.elements().back().geometry);
    if (std::abs(mirrored.start.x + 2.0) > 1.0e-6 ||
        std::abs(mirrored.end.x + 4.0) > 1.0e-6)
        return false;

    SketchEditorSession circular(session, support, plane);
    if (!addLine(circular, {2.0, 0.0}, {4.0, 0.0}) ||
        !circular.selectNearestReference(SketchElementId{1}, {3.0, 0.0}, false) ||
        !circular.circularPatternSelected(4, 360.0) || circular.elements().size() != 4)
        return false;
    const auto& quarterTurn = std::get<SketchLine>(circular.elements()[1].geometry);
    if (std::abs(quarterTurn.start.x) > 1.0e-6 ||
        std::abs(quarterTurn.start.y - 2.0) > 1.0e-6)
        return false;

    SketchEditorSession fixed(session, support, plane);
    if (!addLine(fixed, {0.0, 0.0}, {5.0, 0.0}) ||
        !fixed.addSupportReference({SketchElementId{1},
                                    voelund::document::SketchPointPosition::end},
                                   {5.0, 0.0}, 3, 0.5))
        return false;
    return fixed.constraints().size() == 2 &&
           fixed.constraints()[0].auxiliaryValues == std::vector<double>({3.0, 0.5}) &&
           fixed.constraints()[1].auxiliaryValues == std::vector<double>({3.0, 0.5});
}

bool automaticRelationsWork() {
    CadSession session;
    session.newDocument();
    const auto support = session.addBoxFeature();
    SketchEditorSession editor(session, support, plane);
    if (!addLine(editor, {0.0, 0.0}, {10.0, 10.0}) ||
        !addLine(editor, {0.0, 5.0}, {10.0, 15.5}))
        return false;
    const auto parallel = editor.addAutomaticLineRelation(SketchElementId{2});
    if (parallel != voelund::document::SketchConstraintKind::parallel)
        return false;
    if (!addLine(editor, {3.0, 2.0}, {-7.2, 12.0}))
        return false;
    const auto perpendicular = editor.addAutomaticLineRelation(SketchElementId{3});
    if (perpendicular != voelund::document::SketchConstraintKind::perpendicular)
        return false;
    if (!addLine(editor, {5.0, 5.0}, {8.0, 9.0}) ||
        !editor.addPointOnObjectReference(
            {SketchElementId{4}, voelund::document::SketchPointPosition::start},
            SketchElementId{1}))
        return false;
    if (editor.constraints().size() != 3)
        return false;

    SketchEditorSession tangent(session, support, plane);
    if (!addLine(tangent, {0.0, 0.0}, {10.0, 0.0}))
        return false;
    tangent.setTool(SketchDrawingTool::circle);
    if (!tangent.setPreview({5.0, 5.0}, {5.0, 0.0}) ||
        !tangent.confirmPreview().has_value() ||
        !tangent.addTangencyReference(SketchElementId{2}, SketchElementId{1}))
        return false;
    if (tangent.constraints().size() != 1)
        return false;

    SketchEditorSession equal(session, support, plane);
    if (!addCircle(equal, {0.0, 0.0}, {5.0, 0.0}) ||
        !addCircle(equal, {15.0, 0.0}, {20.1, 0.0}) ||
        !equal.addAutomaticRadialRelation(SketchElementId{2}, SketchElementId{1},
                                          voelund::document::SketchConstraintKind::equal)) {
        std::cerr << "Automatic equal-radius relation failed\n";
        return false;
    }
    const auto& firstEqualCircle =
        std::get<voelund::document::SketchCircle>(equal.elements()[0].geometry);
    const auto& equalCircle =
        std::get<voelund::document::SketchCircle>(equal.elements()[1].geometry);
    if (std::abs(equalCircle.radius - firstEqualCircle.radius) > 1.0e-6) {
        std::cerr << "Automatic equal-radius solve failed\n";
        return false;
    }

    SketchEditorSession concentric(session, support, plane);
    if (!addCircle(concentric, {0.0, 0.0}, {5.0, 0.0}) ||
        !addCircle(concentric, {0.2, -0.1}, {3.2, -0.1}) ||
        !concentric.addAutomaticRadialRelation(
            SketchElementId{2}, SketchElementId{1},
            voelund::document::SketchConstraintKind::concentric)) {
        std::cerr << "Automatic concentric relation failed\n";
        return false;
    }

    SketchEditorSession circleTangent(session, support, plane);
    const bool circleTangency =
        addCircle(circleTangent, {0.0, 0.0}, {5.0, 0.0}) &&
        addCircle(circleTangent, {10.0, 0.0}, {15.0, 0.0}) &&
        circleTangent.addAutomaticRadialRelation(
            SketchElementId{2}, SketchElementId{1},
            voelund::document::SketchConstraintKind::tangent);
    if (!circleTangency)
        std::cerr << "Automatic circle tangency failed\n";
    return circleTangency;
}

} // namespace

int main() {
    if (!everyDrawingToolHasLivePreview()) {
        std::cerr << "Live sketch tool previews failed\n";
        return 1;
    }
    if (!compoundDrawingToolsCreateProfiles()) {
        std::cerr << "Compound sketch drawing tools failed\n";
        return 1;
    }
    if (!lineEditingOperationsWork()) {
        std::cerr << "Sketch line editing operations failed\n";
        return 1;
    }
    if (!transformationsAndLocalHistoryWork()) {
        std::cerr << "Sketch transformations or local history failed\n";
        return 1;
    }
    if (!remainingOperationsAndReferencesWork()) {
        std::cerr << "Remaining sketch operations or references failed\n";
        return 1;
    }
    if (!automaticRelationsWork()) {
        std::cerr << "Automatic sketch relations failed\n";
        return 1;
    }
    return 0;
}
