// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CadSessionTestSupport.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>
#include <voelund/features/SketchRegionIdentity.hpp>
#include <voelund/kernel/SketchShapes.hpp>

#include <gp_Pnt.hxx>

#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

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

    require(editor.selectNearestReference(*spline, {38, 2}, false), "Pick the curve");
    bool refused = false;
    try {
        static_cast<void>(editor.editCurve(application::SketchCurveEdit::extend));
    } catch (const std::invalid_argument& refusal) {
        refused = std::string(refusal.what()).find("nicht verlängert") != std::string::npos;
    }
    require(refused, "Extending a B-spline is refused (N14c2)");
    editor.commitHistory();

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

// N14b: a spline through points passes through each at parameter 0, 1, ... (checked on the
// kernel's own curve of the stored poles), runs out straight at both ends, joins a line end it
// starts on; one point and repeated neighbours are refused.
void throughPoints() {
    application::CadSession session;
    session.newDocument();
    application::SketchEditorSession editor(session, document::DatumPlane::xy);
    editor.setTool(application::SketchDrawingTool::line);
    require(editor.setPreview({-10, 0}, {0, 0}) && editor.confirmPreview().has_value(), "A line");
    const std::array<SketchPoint, 5> fit{SketchPoint{0, 0}, {10, 10}, {20, 0}, {30, 15}, {45, 5}};
    const auto spline = editor.addSplineThroughPoints(fit);
    require(spline.has_value(), "A spline through five points");
    const auto& element = editor.elements().back();
    const auto& poles = std::get<document::SketchBSpline>(element.geometry).poles;
    require(poles.size() == 7, "Seven poles");
    std::vector<gp_Pnt> points;
    for (const auto& pole : poles)
        points.emplace_back(pole.x, pole.y, 0);
    const auto curve = kernel::SpaceCurve::bspline(points);
    for (std::size_t index = 0; index < fit.size(); ++index) {
        const auto at = curve.value(static_cast<double>(index));
        require(std::abs(at.X() - fit[index].x) < 1e-9 && std::abs(at.Y() - fit[index].y) < 1e-9,
                "Through point " + std::to_string(index));
    }
    for (const auto& [end, step] : {std::pair{0.0, 1e-3}, std::pair{4.0, -1e-3}}) {
        const auto a = curve.value(end), b = curve.value(end + step),
                   c = curve.value(end + 2 * step);
        require(std::abs(a.X() - 2 * b.X() + c.X()) < 1e-7 &&
                    std::abs(a.Y() - 2 * b.Y() + c.Y()) < 1e-7,
                "No curvature at the ends");
    }
    int joints = 0;
    for (const auto& constraint : editor.constraints())
        joints += constraint.kind == document::SketchConstraintKind::coincident &&
                  constraint.first.element == *spline;
    require(joints == 1, "Joined to the line's end");

    const std::array<SketchPoint, 1> one{SketchPoint{0, 0}};
    const std::array<SketchPoint, 3> repeated{SketchPoint{0, 0}, {5, 5}, {5, 5}};
    require(!editor.addSplineThroughPoints(one) && !editor.addSplineThroughPoints(repeated),
            "One point and repeated neighbours are refused");
    const std::array<SketchPoint, 2> two{SketchPoint{0, 20}, {10, 30}};
    require(editor.addSplineThroughPoints(two).has_value(), "Two points make a straight spline");
}

// N14c2: split exactly at its middle (t = 0.5 of the Bezier curve): two joined splines that
// still bound the region with the line back to the start.
void split() {
    application::CadSession session;
    session.newDocument();
    application::SketchEditorSession editor(session, document::DatumPlane::xy);
    std::array<SketchPoint, 4> poles{SketchPoint{0, 0}, {10, 20}, {30, 20}, {40, 0}};
    const auto spline = editor.addBSpline(poles);
    editor.setTool(application::SketchDrawingTool::line);
    require(spline && editor.setPreview({40, 0}, {0, 0}) && editor.confirmPreview().has_value(),
            "A spline and the line back");
    editor.commitHistory();
    require(editor.selectNearestReference(*spline, {20, 15}, false), "Pick the middle");
    const auto split = editor.editCurve(application::SketchCurveEdit::split);
    require(split.created.has_value() &&
                editor.lastSolveResult().status == features::SketchSolveStatus::success,
            "Split into two B-splines");
    editor.commitHistory();
    const auto& halves = editor.elements();
    const auto& first = std::get<document::SketchBSpline>(halves.front().geometry);
    require(!first.knots.empty() && std::abs(first.poles.back().x - 20) < 1e-9 &&
                std::abs(first.poles.back().y - 15) < 1e-9,
            "The first half ends at (20; 15) with its own knots");
    require(features::identifySketchRegions(editor.elements()).ids.size() == 1, "Still one region");
}

// N14c2: a B-spline trimmed at a line keeps the exact curve up to the cut.
void trimmed() {
    application::CadSession session;
    session.newDocument();
    application::SketchEditorSession editor(session, document::DatumPlane::xy);
    std::array<SketchPoint, 4> poles{SketchPoint{0, 0}, {10, 20}, {30, 20}, {40, 0}};
    const auto spline = editor.addBSpline(poles);
    editor.setTool(application::SketchDrawingTool::line);
    require(spline && editor.setPreview({20, -5}, {20, 30}) && editor.confirmPreview().has_value(),
            "A spline and a line across it");
    editor.commitHistory();
    require(editor.selectNearestReference(*spline, {35, 9}, false), "Pick the right part");
    const auto result = editor.editCurve(application::SketchCurveEdit::trim);
    require(editor.lastSolveResult().status == features::SketchSolveStatus::success &&
                result.added.size() == 1,
            "Trimmed; the new end lies on the line");
    const auto& kept = std::get<document::SketchBSpline>(editor.elements().front().geometry);
    std::vector<gp_Pnt> points;
    for (const auto& pole : kept.poles)
        points.emplace_back(pole.x, pole.y, 0);
    const auto curve = kernel::SpaceCurve::bspline(points, kept.knots);
    const auto end = curve.value(kept.knots.back());
    const auto quarter = curve.value(0.25);
    require(std::abs(end.X() - 20) < 1e-7 && std::abs(end.Y() - 15) < 1e-7,
            "The spline ends at the line, (20; 15)");
    require(std::abs(quarter.X() - 9.0625) < 1e-9 && std::abs(quarter.Y() - 11.25) < 1e-9,
            "The kept part is the same curve: B(0.25) = (9.0625; 11.25)");

    require(editor.tryAccept().status() == CommandStatus::committed, "Accepted");
    const auto sketch = session.features().back().id;
    const auto directory = std::filesystem::temp_directory_path() / "voelund-n14c2-bspline";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    session.saveDocument(directory / "trimmed.volund");
    application::CadSession reopened;
    reopened.loadDocument(directory / "trimmed.volund");
    require(reopened.sketchParameters(sketch) == session.sketchParameters(sketch),
            "Open restores the trimmed spline with its knots");
    std::filesystem::remove_all(directory);
}

int main() {
    try {
        bspline();
        split();
        trimmed();
        throughPoints();
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
    return 0;
}
