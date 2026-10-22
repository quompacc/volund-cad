// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "../CommandResultAssertions.hpp"
#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>

#include <cmath>
#include <iostream>
#include <numbers>

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
    application::SketchEditorSession editor(
        session, voelund::test::requireCommittedFeature(session.createBoxFeature()), plane);
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

bool centerArcIsParametricAndDiameterRemainsEditable() {
    using namespace voelund;
    application::CadSession session;
    session.newDocument();
    application::SketchEditorSession editor(
        session, voelund::test::requireCommittedFeature(session.createBoxFeature()), plane);
    editor.setTool(application::SketchDrawingTool::arc);
    if (!editor.setPreview({0.0, 0.0}, {4.0, 0.0}, document::SketchPoint{0.0, 4.0})) {
        std::cerr << "arc preview failed\n";
        return false;
    }
    if (editor.confirmPreview() != document::SketchElementId{1}) {
        std::cerr << "arc confirm failed\n";
        return false;
    }
    const auto created = std::get<document::SketchArc>(editor.elements()[0].geometry);
    if (std::abs(created.radius - 4.0) > 1.0e-7 ||
        std::abs(created.endAngle - created.startAngle - std::numbers::pi / 2.0) > 1.0e-7 ||
        editor.evaluatedElements().size() != 1 ||
        !editor.selectNearestReference(document::SketchElementId{1}, created.center, false) ||
        !editor.canAddSelectedRelation(document::SketchConstraintKind::diameter) ||
        !editor.addSelectedRelation(document::SketchConstraintKind::diameter)) {
        std::cerr << "arc initial state or diameter action failed, solve="
                  << static_cast<int>(editor.lastSolveResult().status) << '\n';
        return false;
    }
    const auto diameterId = editor.constraints().back().id;
    if (!editor.setConstraintValue(diameterId, 12.0)) {
        std::cerr << "arc diameter edit failed, solve="
                  << static_cast<int>(editor.lastSolveResult().status) << '\n';
        return false;
    }
    const auto solved = std::get<document::SketchArc>(editor.elements()[0].geometry);
    return std::abs(solved.radius - 6.0) <= 1.0e-7 && solved.endAngle > solved.startAngle &&
           solved.endAngle - solved.startAngle < 2.0 * std::numbers::pi;
}

// S16: a radius dimension on an arc or circle; one per element, never on a line.
bool radiusDimensionDrivesArcAndCircle() {
    using namespace voelund;
    application::CadSession session;
    session.newDocument();
    application::SketchEditorSession editor(
        session, voelund::test::requireCommittedFeature(session.createBoxFeature()), plane);
    editor.setTool(application::SketchDrawingTool::arc);
    if (!editor.setPreview({0.0, 0.0}, {4.0, 0.0}, document::SketchPoint{0.0, 4.0}) ||
        editor.confirmPreview() != document::SketchElementId{1}) {
        std::cerr << "arc not created\n";
        return false;
    }
    editor.setTool(application::SketchDrawingTool::circle);
    if (!addCircle(editor, {20.0, 0.0}, {23.0, 0.0}, 2)) {
        std::cerr << "circle not created\n";
        return false;
    }
    editor.setTool(application::SketchDrawingTool::line);
    if (!editor.setPreview({0.0, -10.0}, {10.0, -10.0}) ||
        editor.confirmPreview() != document::SketchElementId{3}) {
        std::cerr << "line not created\n";
        return false;
    }
    if (!editor.selectNearestReference(document::SketchElementId{3}, {0.0, -10.0}, false) ||
        editor.canAddSelectedRelation(document::SketchConstraintKind::radius)) {
        std::cerr << "radius offered on a line\n";
        return false;
    }
    const auto arc = std::get<document::SketchArc>(editor.elements()[0].geometry);
    if (!editor.selectNearestReference(document::SketchElementId{1}, arc.center, false) ||
        !editor.addSelectedRelation(document::SketchConstraintKind::radius) ||
        editor.constraints().back().kind != document::SketchConstraintKind::radius ||
        std::abs(editor.constraints().back().value - 4.0) > 1.0e-9 ||
        editor.canAddSelectedRelation(document::SketchConstraintKind::radius)) {
        std::cerr << "arc radius not added once\n";
        return false;
    }
    const auto arcRadius = editor.constraints().back().id;
    if (!editor.setConstraintValue(arcRadius, 12.0) ||
        std::abs(std::get<document::SketchArc>(editor.elements()[0].geometry).radius - 12.0) >
            1.0e-7) {
        std::cerr << "arc radius edit failed\n";
        return false;
    }
    if (!editor.selectNearestReference(document::SketchElementId{2}, {20.0, 0.0}, false) ||
        !editor.addSelectedRelation(document::SketchConstraintKind::radius) ||
        !editor.setConstraintValue(editor.constraints().back().id, 20.0) ||
        std::abs(std::get<document::SketchCircle>(editor.elements()[1].geometry).radius - 20.0) >
            1.0e-7) {
        std::cerr << "circle radius failed\n";
        return false;
    }
    // The circle's radius leaves the arc's alone.
    if (std::abs(std::get<document::SketchArc>(editor.elements()[0].geometry).radius - 12.0) >
        1.0e-7) {
        std::cerr << "the arc lost its radius\n";
        return false;
    }
    return true;
}

} // namespace

int main() {
    if (!radiusDimensionDrivesArcAndCircle()) {
        std::cerr << "Radius dimension failed\n";
        return 1;
    }
    if (!circlesBecomeConcentricAndDiameterRemainsEditable()) {
        std::cerr << "Concentric or diameter editor relation failed\n";
        return 1;
    }
    if (!centerArcIsParametricAndDiameterRemainsEditable()) {
        std::cerr << "Parametric center arc failed\n";
        return 1;
    }
    return 0;
}
