#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>

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
    return tangent.constraints().size() == 1;
}

} // namespace

int main() {
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
