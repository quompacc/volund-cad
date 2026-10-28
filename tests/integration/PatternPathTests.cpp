// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CadSessionTestSupport.hpp"

#include <voelund/application/Actions.hpp>

#include <BRepClass3d_SolidClassifier.hxx>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>

// N11c (schema 49): a cube 10 at the origin. Along a line of 60 on XY, 4 copies 20 apart stand
// at x = 0, 20, 40, 60 as 4 bodies; a fifth would run past the end and is refused. Along a
// circle of radius 30 about (35; 5), a quarter of its length apart: kept upright the copies are
// moved like the circle's points; following the path they turn about the circle's centre, as a
// circular pattern would. Save/Open keeps the path and the orientation.
namespace {
using namespace voelund;
using application::CommandStatus;
constexpr double pi = std::numbers::pi;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-6 * std::max(1.0, std::abs(b)); }
double volume(const application::CadSession& session) {
    double total = 0;
    for (const auto& body : session.validBodies())
        total += test::volumeOf(body.shape);
    return total;
}
bool solidAt(const application::CadSession& session, double x, double y, double z) {
    for (const auto& body : session.validBodies())
        if (BRepClass3d_SolidClassifier(body.shape, gp_Pnt(x, y, z), 1e-7).State() == TopAbs_IN)
            return true;
    return false;
}

document::FeatureId pathSketch(application::CadSession& session, document::SketchGeometry curve) {
    document::SketchParameters sketch{document::datumSketchPlane(document::DatumPlane::xy), {}};
    sketch.datum = document::DatumPlane::xy;
    sketch.elements.values.push_back(
        {document::SketchElementId{sketch.elements.nextId++}, std::move(curve)});
    require(session.createDatumSketchFeature(sketch).status() == CommandStatus::committed,
            "Path sketch");
    return session.features().back().id;
}
application::PatternInput upright(document::FeatureId path, std::uint32_t count, double spacing) {
    application::PatternInput input;
    input.kind = document::PatternKind::path;
    input.count = count;
    input.spacing = spacing;
    input.path = document::SweepPath{path, {document::SketchElementId{1}}};
    return input;
}
application::CommandResult along(application::CadSession& session, document::FeatureId box,
                                 const application::PatternInput& input) {
    return application::executeAction(
               session,
               application::prepareRequest(session, application::CreatePatternAction{box, input}))
        .command;
}

void line() {
    application::CadSession session;
    session.newDocument();
    const auto box = *session.createBoxFeature({10, 10, 10}).feature();
    const auto path = pathSketch(session, document::SketchLine{{0, 0}, {60, 0}});
    require(along(session, box, upright(path, 5, 20)).status() != CommandStatus::committed,
            "A fifth copy would run past the end of the line");
    require(along(session, box, upright(path, 4, 20)).status() == CommandStatus::committed &&
                session.validBodies().size() == 4 && near(volume(session), 4000) &&
                solidAt(session, 65, 5, 5) && solidAt(session, 25, 5, 5),
            "4 cubes 20 apart along the line");
}

// The cubes' middles: kept upright the circle's points minus its start (65; 5); turned, the
// original's middle (5; 5) turned about (35; 5).
bool cubesAt(const application::CadSession& session, const std::vector<gp_Pnt>& middles) {
    for (const auto& middle : middles)
        if (!solidAt(session, middle.X(), middle.Y(), 5))
            return false;
    return true;
}

void circle() {
    application::CadSession session;
    session.newDocument();
    const auto box = *session.createBoxFeature({10, 10, 10}).feature();
    const auto path = pathSketch(session, document::SketchCircle{{35, 5}, 30});
    const double quarter = 2 * pi * 30 / 4;
    require(along(session, box, upright(path, 4, quarter)).status() == CommandStatus::committed &&
                near(volume(session), 4000) &&
                cubesAt(session, {{-25, 35, 5}, {-55, 5, 5}, {-25, -25, 5}}),
            "Kept upright: moved like the circle's points");
    const auto pattern = session.features()[2].id; // box, path sketch, pattern, copies
    auto parameters = session.patternParameters(pattern);
    parameters.followPath = true;
    require(session.editPatternParameters(pattern, parameters).status() ==
                    CommandStatus::committed &&
                near(volume(session), 4000) &&
                cubesAt(session, {{35, -25, 5}, {65, 5, 5}, {35, 35, 5}}),
            "Following the circle: turned about its centre");
    const auto file = test::temporaryFilePath("voelund-n11c-path.volund");
    session.saveDocument(file);
    application::CadSession loaded;
    loaded.loadDocument(file);
    std::error_code ignored;
    std::filesystem::remove(file, ignored);
    require(loaded.patternParameters(pattern) == parameters &&
                cubesAt(loaded, {{35, -25, 5}, {65, 5, 5}, {35, 35, 5}}),
            "Save/Open keeps the path and the orientation");
}
} // namespace

int main() {
    try {
        line();
        circle();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "PASS N11c pattern along a line and a circle, upright and following, open\n";
    return 0;
}
