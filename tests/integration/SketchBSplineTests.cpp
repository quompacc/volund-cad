// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CadSessionTestSupport.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>
#include <voelund/features/SketchRegionIdentity.hpp>

#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

// S21 (user decision 2026-10-14): an open cubic B-spline over control points. With a line back
// to its start it bounds one region, which extrudes to the volume of its area; a moved control
// point changes the curve and the body; splitting it is refused; undo, save and open.
namespace {
using namespace voelund;
using application::CommandStatus;
using document::SketchPoint;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
double total(const application::CadSession& session) {
    double sum = 0;
    for (const auto& body : session.validBodies())
        sum += test::volumeOf(body.shape);
    return sum;
}
// Four control points make a cubic Bezier curve; its area with the chord back to the start.
double area(const std::array<SketchPoint, 4>& p) {
    double sum = 0;
    SketchPoint previous = p[0];
    for (int step = 1; step <= 4000; ++step) {
        const double t = step / 4000.0, s = 1 - t;
        const SketchPoint q{s * s * s * p[0].x + 3 * s * s * t * p[1].x + 3 * s * t * t * p[2].x +
                                t * t * t * p[3].x,
                            s * s * s * p[0].y + 3 * s * s * t * p[1].y + 3 * s * t * t * p[2].y +
                                t * t * t * p[3].y};
        sum += previous.x * q.y - q.x * previous.y;
        previous = q;
    }
    sum += previous.x * p[0].y - p[0].x * previous.y;
    return std::abs(sum) / 2;
}
bool near(double a, double b) { return std::abs(a - b) < 1e-3 * std::max(1.0, std::abs(b)); }

void bspline() {
    application::CadSession session;
    session.newDocument();
    application::SketchEditorSession editor(session, document::DatumPlane::xy);
    std::array<SketchPoint, 4> poles{SketchPoint{0, 0}, {10, 20}, {30, 20}, {40, 0}};
    const auto spline = editor.addBSpline(poles);
    require(spline.has_value(), "A B-spline over four control points");
    editor.setTool(application::SketchDrawingTool::line);
    require(editor.setPreview({40, 0}, {0, 0}) && editor.confirmPreview().has_value(),
            "A line back to the start");
    const auto line = editor.elements().back().id;
    require(editor.addCoincidentReferences({*spline, document::SketchPointPosition::end},
                                           {line, document::SketchPointPosition::start}) &&
                editor.addCoincidentReferences({line, document::SketchPointPosition::end},
                                               {*spline, document::SketchPointPosition::start}),
            "Both ends joined");
    editor.commitHistory();
    require(features::identifySketchRegions(editor.elements()).ids.size() == 1, "One region");

    require(editor.selectNearestReference(*spline, {20, 15}, false), "Pick the curve");
    bool refused = false;
    try {
        static_cast<void>(editor.editCurve(application::SketchCurveEdit::split));
    } catch (const std::invalid_argument& refusal) {
        refused = std::string(refusal.what()).find("B-Spline") != std::string::npos;
    }
    require(refused, "Splitting a B-spline is refused");

    require(editor.tryAccept().status() == CommandStatus::committed, "Accepted");
    const auto sketch = session.features().back().id;
    const auto regions =
        features::identifySketchRegions(session.sketchParameters(sketch).elements.values).ids;
    require(session.createSketchProfileExtrusion(sketch, regions, 10).status() ==
                    CommandStatus::committed &&
                near(total(session), area(poles) * 10),
            "The region extrudes to its area: " + std::to_string(total(session)) + " for " +
                std::to_string(area(poles) * 10));

    application::SketchEditorSession again(session, sketch, session.sketchParameters(sketch));
    require(again.moveBSplinePole(*spline, 1, {10, 30}), "Move a control point");
    require(again.undo() && again.redo(), "Undo and redo the move");
    require(again.tryAccept().status() == CommandStatus::committed, "Accepted again");
    poles[1] = {10, 30};
    require(near(total(session), area(poles) * 10), "The body follows the curve");

    const auto directory = std::filesystem::temp_directory_path() / "voelund-s21-bspline";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const auto file = directory / "bspline.volund";
    session.saveDocument(file);
    application::CadSession reopened;
    reopened.loadDocument(file);
    require(reopened.sketchParameters(sketch) == session.sketchParameters(sketch) &&
                near(total(reopened), total(session)),
            "Open restores the B-spline");
    std::filesystem::remove_all(directory);
}

} // namespace

int main() {
    try {
        bspline();
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
    return 0;
}
