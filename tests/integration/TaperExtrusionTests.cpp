// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CadSessionTestSupport.hpp"

#include <voelund/features/SketchRegionIdentity.hpp>

#include <cmath>
#include <filesystem>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>

// W16 (user request 2026-10-25, FreeCAD's "Schrägungswinkel"): an extrusion's sides lean by
// the taper angle - narrower with a negative angle - on a new body and in a cut; it is kept by
// Save and Open (schema 40), edited like the distance, and refused where it has no meaning.
namespace {
using namespace voelund;
using application::CommandStatus;
using application::ExtrusionMode;
using document::ExtrusionOperation;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
document::SketchParameters onXy(double x0, double y0, double x1, double y1) {
    document::SketchParameters sketch{
        document::datumSketchPlane(document::DatumPlane::xy),
        {{{document::SketchElementId{1}, document::SketchRectangle{x0, y0, x1, y1}}}, 2}};
    sketch.datum = document::DatumPlane::xy;
    return sketch;
}
double volume(const application::CadSession& session) {
    return test::volumeOf(session.currentShape());
}
// A frustum of square ends: h/3 (A1 + A2 + sqrt(A1 A2)).
double frustum(double side, double top, double height) {
    return height / 3 * (side * side + top * top + side * top);
}
ExtrusionMode tapered(double degrees) {
    ExtrusionMode mode;
    mode.taperAngle = degrees;
    return mode;
}

void narrowsAndKeeps(const std::filesystem::path& work) {
    application::CadSession session;
    session.newDocument();
    const auto result =
        session.createDatumSketchExtrusion(onXy(-10, -10, 10, 10), {}, 10, tapered(-10));
    require(result.status() == CommandStatus::committed,
            "Tapered (status " + std::to_string(static_cast<int>(result.status())) +
                "): " + result.diagnostic());
    const double top = 20 - 2 * 10 * std::tan(10 * std::numbers::pi / 180);
    require(std::abs(volume(session) - frustum(20, top, 10)) < 1e-3,
            "Narrower towards the end: " + std::to_string(volume(session)));
    const auto path = work / "taper.volund";
    session.saveDocument(path);
    session.newDocument();
    session.loadDocument(path);
    require(std::abs(volume(session) - frustum(20, top, 10)) < 1e-3 &&
                session.identifiedSnapshot()
                        .snapshot()
                        .extrusionParameters(session.features().back().id)
                        .taperAngle == -10.0,
            "Save and Open keep the taper");
    auto values =
        session.identifiedSnapshot().snapshot().extrusionParameters(session.features().back().id);
    values.taperAngle = 0.0;
    require(session.editExtrusionParameters(session.features().back().id, values).status() ==
                    CommandStatus::committed &&
                std::abs(volume(session) - 4000) < 1e-6,
            "Set back to 0 it is a straight prism");
}

void refused() {
    application::CadSession session;
    session.newDocument();
    auto symmetric = tapered(5);
    symmetric.symmetric = true;
    require(
        session.createDatumSketchExtrusion(onXy(-10, -10, 10, 10), {}, 10, symmetric).status() !=
            CommandStatus::committed,
        "A symmetric extrusion does not taper");
    require(
        session.createDatumSketchExtrusion(onXy(-10, -10, 10, 10), {}, 10, tapered(89)).status() !=
            CommandStatus::committed,
        "89 degrees and more are refused");
    require(
        session.createDatumSketchExtrusion(onXy(-10, -10, 10, 10), {}, 10, tapered(-60)).status() !=
                CommandStatus::committed &&
            session.features().empty(),
        "A taper that closes the profile is refused without change");
}
} // namespace

int main() {
    try {
        const auto work = std::filesystem::temp_directory_path() / "voelund-w16";
        std::filesystem::create_directories(work);
        narrowsAndKeeps(work);
        refused();
        std::filesystem::remove_all(work);
        std::cout << "PASS W16 tapered extrusion\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
