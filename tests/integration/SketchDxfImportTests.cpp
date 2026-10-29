// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>
#include <voelund/document/SketchDxf.hpp>

#include <cmath>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>

// N15b (packet acceptance): a reference DXF with known entities and measures comes into the
// sketch in one step: a line, a closed polyline with a bulged semicircle (joined at its
// corners), a circle on the layer "Konstruktion" (construction), an arc, an ellipse and a cubic
// spline; a TEXT is skipped and counted. Inches are scaled to millimetres, and a sketch the
// export writes reads back the same.
namespace {
using namespace voelund;
using document::SketchPoint;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-9; }
bool near(SketchPoint a, SketchPoint b) { return near(a.x, b.x) && near(a.y, b.y); }

std::string entity(const std::vector<std::pair<int, std::string>>& groups) {
    std::string text;
    for (const auto& [code, value] : groups)
        text += std::to_string(code) + "\n" + value + "\n";
    return text;
}
std::string drawing(int units, const std::string& entities) {
    return entity({{0, "SECTION"},
                   {2, "HEADER"},
                   {9, "$INSUNITS"},
                   {70, std::to_string(units)},
                   {0, "ENDSEC"},
                   {0, "SECTION"},
                   {2, "ENTITIES"}}) +
           entities + entity({{0, "ENDSEC"}, {0, "EOF"}});
}

const std::string reference = drawing(
    4, entity({{0, "LINE"}, {8, "0"}, {10, "0"}, {20, "0"}, {11, "40"}, {21, "0"}}) +
           entity({{0, "LWPOLYLINE"},
                   {8, "0"},
                   {90, "4"},
                   {70, "1"},
                   {10, "50"},
                   {20, "0"},
                   {10, "70"},
                   {20, "0"},
                   {42, "1"},
                   {10, "70"},
                   {20, "20"},
                   {10, "50"},
                   {20, "20"}}) +
           entity({{0, "CIRCLE"}, {8, "Konstruktion"}, {10, "100"}, {20, "0"}, {40, "5"}}) +
           entity({{0, "ARC"}, {10, "0"}, {20, "50"}, {40, "10"}, {50, "0"}, {51, "90"}}) +
           entity({{0, "ELLIPSE"},
                   {10, "0"},
                   {20, "100"},
                   {11, "20"},
                   {21, "0"},
                   {40, "0.5"},
                   {41, "0"},
                   {42, "6.283185307179586"}}) +
           entity({{0, "SPLINE"}, {70, "8"},  {71, "3"},   {72, "8"},  {73, "4"},   {40, "0"},
                   {40, "0"},     {40, "0"},  {40, "0"},   {40, "1"},  {40, "1"},   {40, "1"},
                   {40, "1"},     {10, "0"},  {20, "150"}, {10, "10"}, {20, "170"}, {10, "30"},
                   {20, "170"},   {10, "40"}, {20, "150"}}) +
           entity({{0, "TEXT"}, {10, "0"}, {20, "0"}, {40, "5"}, {1, "Hallo"}}));

template <typename Geometry>
const Geometry& nth(const application::SketchEditorSession& editor, std::size_t index) {
    return std::get<Geometry>(editor.elements()[index].geometry);
}

// The polyline's joints, the construction circle, the arc, the ellipse and the spline.
void referenceCurves(const application::SketchEditorSession& editor) {
    std::size_t joints = 0;
    for (const auto& constraint : editor.constraints())
        joints += constraint.kind == document::SketchConstraintKind::coincident;
    require(joints == 4, "The closed polyline is joined at its four corners");
    require(editor.elements()[5].construction &&
                near(nth<document::SketchCircle>(editor, 5).radius, 5),
            "The circle on the layer Konstruktion is construction geometry");
    require(near(nth<document::SketchArc>(editor, 6).endAngle, std::numbers::pi / 2),
            "The arc from 0 to 90 degrees");
    const auto& ellipse = nth<document::SketchEllipse>(editor, 7);
    require(near(ellipse.majorRadius, 20) && near(ellipse.minorRadius, 10), "The ellipse 20 x 10");
    const auto& spline = nth<document::SketchBSpline>(editor, 8);
    require(spline.poles.size() == 4 && near(spline.poles[1], {10, 170}), "The cubic spline");
}

void referenceFile() {
    application::CadSession session;
    session.newDocument();
    application::SketchEditorSession editor(session, document::DatumPlane::xy);
    const auto result = editor.importDxf(reference);
    require(result.added == 9 && result.skipped.size() == 1 && result.skipped.at("TEXT") == 1,
            "Nine curves, the text skipped");
    require(near(nth<document::SketchLine>(editor, 0).end, {40, 0}), "The line, 40 long");
    const auto& bulge = nth<document::SketchArc>(editor, 2);
    require(near(bulge.center, {70, 10}) && near(bulge.radius, 10) &&
                near(bulge.startAngle, -std::numbers::pi / 2) &&
                near(bulge.endAngle, std::numbers::pi / 2),
            "The bulge 1 is the semicircle to the right of (70; 0)-(70; 20)");
    referenceCurves(editor);
    require(editor.undo() && editor.elements().empty(), "One undo step");
}

void unitsAndRoundTrip() {
    application::CadSession session;
    session.newDocument();
    application::SketchEditorSession inches(session, document::DatumPlane::xy);
    static_cast<void>(inches.importDxf(
        drawing(1, entity({{0, "LINE"}, {10, "0"}, {20, "0"}, {11, "1"}, {21, "0"}}))));
    require(near(nth<document::SketchLine>(inches, 0).end, {25.4, 0}), "One inch is 25.4 mm");

    application::SketchEditorSession source(session, document::DatumPlane::xy);
    static_cast<void>(source.importDxf(reference));
    const auto text = document::sketchDxf(source.draftParameters().elements);
    application::SketchEditorSession again(session, document::DatumPlane::xy);
    const auto result = again.importDxf(text);
    require(result.added == source.elements().size() && result.skipped.empty(),
            "The export reads back completely");
    for (std::size_t index = 0; index < result.added; ++index)
        require(again.elements()[index].geometry.index() ==
                    source.elements()[index].geometry.index(),
                "Element " + std::to_string(index) + " keeps its kind");

    try {
        static_cast<void>(again.importDxf("not a drawing"));
    } catch (const application::SketchEditRefusal& refusal) {
        require(std::string(refusal.what()).find("DXF") != std::string::npos, "Named refusal");
        return;
    }
    throw std::runtime_error("A broken file is not refused");
}

} // namespace

int main() {
    try {
        referenceFile();
        unitsAndRoundTrip();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
