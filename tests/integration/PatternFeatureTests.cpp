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

// N11b (schema 48, packet acceptance): a plate 120 x 100 x 10 with a through hole of 6 at its
// top face's centre (60; 50). The hole repeated in a grid 3 along X (20) by 4 along Y (15) is
// 12 holes: 120000 - 12 * 9 pi * 10, still one body. The source hole widened to 8 widens every
// copy; copy 5 (at (100; 65)) skipped is solid again; Save/Open keeps it all. A feature pattern
// as new bodies, or of the box itself, is refused. A hole 30 left of the centre mirrored about a
// construction plane x = 60 drills its twin at x = 90; widened, both widen; Save/Open.
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

struct Plate final {
    application::CadSession session;
    document::FeatureId box{1};
    document::FeatureId hole{1};
};
void make(Plate& plate, double dx = 0) {
    plate.session.newDocument();
    plate.box = *plate.session.createBoxFeature({120, 100, 10}).feature();
    for (const auto& face : plate.session.supportFaces(plate.box))
        if (face.plane.normalZ > 0.5) {
            const double x = (face.minX + face.maxX) / 2, y = (face.minY + face.maxY) / 2;
            const auto made =
                plate.session.createHole(plate.box, face.origin, x + dx, y, 6.0, std::nullopt);
            require(made.feature().has_value(), "Hole: " + made.diagnostic());
            plate.hole = *made.feature();
            return;
        }
    throw std::runtime_error("Top face missing");
}
application::PatternInput holes(const Plate& plate) {
    application::PatternInput input;
    input.baseAxis = document::BaseAxis::x;
    input.count = 3;
    input.spacing = 20;
    document::PatternDirection rows;
    rows.axis.baseAxis = document::BaseAxis::y;
    rows.count = 4;
    rows.spacing = 15;
    input.second = rows;
    input.mode = document::PatternMode::join;
    input.features = {plate.hole};
    return input;
}
application::CommandResult create(application::CadSession& session, document::FeatureId support,
                                  const application::PatternInput& input) {
    return application::executeAction(
               session, application::prepareRequest(
                            session, application::CreatePatternAction{support, input}))
        .command;
}

void reopened(application::CadSession& session, document::FeatureId pattern) {
    const auto path = test::temporaryFilePath("voelund-n11b-holes.volund");
    session.saveDocument(path);
    application::CadSession loaded;
    loaded.loadDocument(path);
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    require(loaded.patternParameters(pattern) == session.patternParameters(pattern) &&
                near(volume(loaded), volume(session)) && solidAt(loaded, 100, 65, 5),
            "Save/Open keeps the repeated holes and the gap");
}

void holeGrid() {
    Plate plate;
    make(plate);
    auto& session = plate.session;
    require(create(session, plate.hole, holes(plate)).status() == CommandStatus::committed &&
                session.validBodies().size() == 1 && near(volume(session), 120000 - 1080 * pi) &&
                !solidAt(session, 100, 95, 5) && solidAt(session, 104, 95, 5),
            "12 holes of 6 in a 3 x 4 grid, one body");
    const auto pattern = session.features().back().id;
    auto source = session.holeParameters(plate.hole);
    source.diameter = 8;
    require(session.editHoleParameters(plate.hole, source).status() == CommandStatus::committed &&
                near(volume(session), 120000 - 1920 * pi) && !solidAt(session, 103.5, 95, 5),
            "The source widened to 8 widens every copy");
    auto parameters = session.patternParameters(pattern);
    parameters.skipped = {5};
    require(session.editPatternParameters(pattern, parameters).status() ==
                    CommandStatus::committed &&
                near(volume(session), 120000 - 11 * 160 * pi) && solidAt(session, 100, 65, 5),
            "Copy 5 skipped: (100; 65) is solid");
    reopened(session, pattern);
}

void mirroredHole() {
    Plate plate;
    make(plate, -30);
    auto& session = plate.session;
    const auto plane =
        session.createDatumPlane(document::DatumPlane::yz, std::nullopt, std::nullopt, 60,
                                 document::DatumRotationAxis::none, 0);
    require(plane.feature().has_value() && !solidAt(session, 30, 50, 5), "Hole at x = 30");
    const auto made =
        session.createMirror(application::CreateMirrorAction{plate.hole,
                                                             std::nullopt,
                                                             plane.feature(),
                                                             std::nullopt,
                                                             std::nullopt,
                                                             document::MirrorMode::join,
                                                             {plate.hole}});
    require(made.status() == CommandStatus::committed && session.validBodies().size() == 1 &&
                near(volume(session), 120000 - 180 * pi) && !solidAt(session, 90, 50, 5),
            "The mirrored hole at x = 90: " + made.diagnostic());
    auto source = session.holeParameters(plate.hole);
    source.diameter = 8;
    require(session.editHoleParameters(plate.hole, source).status() == CommandStatus::committed &&
                near(volume(session), 120000 - 320 * pi),
            "Both holes widen with the source");
    const auto path = test::temporaryFilePath("voelund-n11b-mirror.volund");
    session.saveDocument(path);
    application::CadSession loaded;
    loaded.loadDocument(path);
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    const auto mirror = session.features().back().id;
    require(loaded.mirrorParameters(mirror) == session.mirrorParameters(mirror) &&
                near(volume(loaded), volume(session)),
            "Save/Open keeps the mirrored hole");
}

void refused() {
    Plate plate;
    make(plate);
    auto separate = holes(plate);
    separate.mode = document::PatternMode::newBodies;
    auto box = holes(plate);
    box.features = {plate.box};
    for (const auto& input : {separate, box})
        require(create(plate.session, plate.hole, input).status() != CommandStatus::committed &&
                    plate.session.features().size() == 2,
                "A feature pattern as new bodies or of the box is refused");
}
} // namespace

int main() {
    try {
        holeGrid();
        mirroredHole();
        refused();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "PASS N11b feature pattern and mirror: hole grid and mirrored hole follow their "
                 "source, skipped copy, open\n";
    return 0;
}
