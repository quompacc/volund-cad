// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "../CommandResultAssertions.hpp"
#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <variant>

// W13 (user report 2026-10-24, as in FreeCAD): the rectangle tool's "Abgerundete Ecken" and
// "Rahmen" build the shape with all its relations: drawn from the origin, nothing is left free.
namespace {
using namespace voelund;
using application::SketchDrawingTool;
using application::SketchEditorSession;
using application::SketchSnap;
using application::SketchSnapKind;
using document::SketchConstraintKind;

constexpr document::SketchPlane plane{0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0};

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}

struct Shape final {
    std::size_t lines{0}, construction{0}, arcs{0};
    std::vector<double> radii;
    std::size_t radiusDimensions{0}, equals{0}, frameDistances{0};
    int freedom{-1};
};

Shape draw(SketchEditorSession::RectangleOptions options, document::SketchPoint corner = {40, 20}) {
    application::CadSession session;
    session.newDocument();
    const auto support = test::requireCommittedFeature(session.createBoxFeature());
    SketchEditorSession editor(session, support, plane);
    editor.setTool(SketchDrawingTool::rectangle);
    application::SketchCreationInput input{{0, 0}, corner};
    input.firstSnap = SketchSnap{SketchSnapKind::origin};
    const auto created = editor.create(input);
    require(created.has_value(), "rectangle");
    editor.finishRectangle(*created, options);
    Shape shape;
    for (const auto& element : editor.elements()) {
        if (const auto* arc = std::get_if<document::SketchArc>(&element.geometry)) {
            ++shape.arcs;
            shape.radii.push_back(arc->radius);
        } else if (std::holds_alternative<document::SketchLine>(element.geometry)) {
            ++(element.construction ? shape.construction : shape.lines);
        }
    }
    for (const auto& constraint : editor.constraints()) {
        shape.radiusDimensions += constraint.kind == SketchConstraintKind::radius;
        shape.equals += constraint.kind == SketchConstraintKind::equal;
        shape.frameDistances += constraint.kind == SketchConstraintKind::pointLineDistance;
    }
    shape.freedom = editor.lastSolveResult().degreesOfFreedom;
    return shape;
}

bool allNear(const std::vector<double>& values, double wanted, std::size_t count) {
    return std::ranges::count_if(values, [&](double value) {
               return std::abs(value - wanted) < 1e-6;
           }) == static_cast<long>(count);
}

void both();
void outwards();

void run() {
    const auto rounded = draw({true, false});
    require(rounded.lines == 4 && rounded.arcs == 4 && allNear(rounded.radii, 3.0, 4),
            "Rounded: four sides and four corners of R 3 (15 % of the 20 mm side)");
    require(rounded.radiusDimensions == 1 && rounded.equals == 3,
            "Rounded: one radius dimension, the other corners equal to it");
    require(rounded.freedom == 0, "Rounded from the origin: nothing left free, " +
                                      std::to_string(rounded.freedom) + " left");

    const auto framed = draw({false, true});
    require(framed.lines == 8 && framed.arcs == 0 && framed.frameDistances == 4,
            "Frame: an inner rectangle 2 mm (10 %) off every side");
    require(framed.freedom == 0,
            "Frame from the origin: nothing left free, " + std::to_string(framed.freedom));

    both();
    outwards();
    bool refused = false;
    try {
        static_cast<void>(draw({true, true}, {1, 1}));
    } catch (const std::invalid_argument&) {
        refused = true;
    }
    require(refused, "A rectangle too small for its frame is refused");
    std::cout << "PASS W13 rectangle options: rounded corners, frame, fully constrained\n";
}

// W13b: a frame outwards; the drawn rectangle is the inner one, the outer corners concentric.
void outwards() {
    const auto shape = draw({true, true, 2.0, -1.5});
    require(shape.arcs == 8 && allNear(shape.radii, 2.0, 4) && allNear(shape.radii, 3.5, 4),
            "Outwards: drawn corners R 2, frame corners R 3.5");
    require(shape.freedom == 0,
            "Outwards from the origin: nothing left free, " + std::to_string(shape.freedom));
}

void both() {
    const auto both = draw({true, true});
    require(both.lines == 8 && both.arcs == 8 && allNear(both.radii, 3.0, 4) &&
                allNear(both.radii, 1.0, 4),
            "Both: outer corners R 3, inner R 1 (less the frame width)");
    require(both.radiusDimensions == 2 && both.equals == 6,
            "Both: one radius dimension per rectangle");
    require(both.freedom == 0,
            "Both from the origin: nothing left free, " + std::to_string(both.freedom));
}
} // namespace

int main() {
    try {
        run();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
