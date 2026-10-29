// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>
#include <voelund/features/SketchRegionIdentity.hpp>

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

// N15a (packet acceptance): section curves. A cylinder (radius 8, height 25) cut at height 10
// gives its circle; cut through its axis, four joined lines bound one region; without a body
// the command is refused and nothing changes.
namespace {
using namespace voelund;
using application::CommandStatus;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-7; }

void cylinder() {
    application::CadSession session;
    session.newDocument();
    require(session.createNewBodyCylinder().status() == CommandStatus::committed, "A cylinder");
    const auto body = session.features().back().id;

    document::SketchPlane level{};
    level.originZ = 10;
    level.normalZ = 1;
    level.xDirectionX = 1;
    application::SketchEditorSession across(session, body, level);
    require(across.addSectionCurves() == 1, "One curve at height 10");
    const auto* circle = std::get_if<document::SketchCircle>(&across.elements().back().geometry);
    require(circle != nullptr && near(circle->center.x, 0) && near(circle->center.y, 0) &&
                near(circle->radius, 8),
            "The cylinder's circle, radius 8 about the axis");

    // Tilted by 30 degrees about x: an ellipse with radii 8 and 8 / cos 30.
    document::SketchPlane tilted = level;
    tilted.normalY = -0.5;
    tilted.normalZ = std::sqrt(3.0) / 2;
    application::SketchEditorSession slant(session, body, tilted);
    require(slant.addSectionCurves() == 1, "One curve on the tilted plane");
    const auto* ellipse = std::get_if<document::SketchEllipse>(&slant.elements().back().geometry);
    require(ellipse != nullptr && near(ellipse->minorRadius, 8) &&
                near(ellipse->majorRadius, 16 / std::sqrt(3.0)),
            "An ellipse with radii 8 and 8 / cos 30");

    application::SketchEditorSession along(session, document::DatumPlane::xz);
    const auto count = along.addSectionCurves();
    require(count == 4, "Four lines through the axis, got " + std::to_string(count));
    std::size_t joints = 0;
    for (const auto& constraint : along.constraints())
        joints += constraint.kind == document::SketchConstraintKind::coincident;
    require(joints == 4 && features::identifySketchRegions(along.elements()).ids.size() == 1,
            "Joined at their ends, they bound one region");
    require(along.undo() && along.elements().empty(), "One undo step removes them");
}

void nothingToCut() {
    application::CadSession session;
    session.newDocument();
    application::SketchEditorSession editor(session, document::DatumPlane::xy);
    const auto before = editor.draftParameters();
    try {
        static_cast<void>(editor.addSectionCurves());
    } catch (const application::SketchEditRefusal& refusal) {
        require(std::string(refusal.what()).find("Kein sichtbarer Körper") != std::string::npos &&
                    editor.draftParameters() == before,
                "Refused without a body, nothing changed");
        return;
    }
    throw std::runtime_error("Not refused without a body");
}

} // namespace

int main() {
    try {
        cylinder();
        nothingToCut();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
