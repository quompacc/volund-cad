// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CadSessionTestSupport.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/features/SketchRegionIdentity.hpp>

#include <cmath>
#include <filesystem>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>

// M26 (user decision 2026-10-14): a helix ("Wendel"). A circle of diameter 2 on XZ, 10 from the
// Z axis, with pitch 5 and height 20 (4 turns) makes a spring whose volume is within 1 % of
// 2 pi^2 r^2 R n (Pappus for a screw motion); left-handed as well; cut from a cylinder of radius
// 10 it makes a thread groove; undo, save and open.
namespace {
using namespace voelund;
using application::CommandStatus;
constexpr double pi = std::numbers::pi;

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

document::FeatureId sketch(application::CadSession& session, document::DatumPlane plane,
                           document::SketchGeometry geometry) {
    document::SketchParameters parameters{document::datumSketchPlane(plane), {}};
    parameters.datum = plane;
    parameters.elements.values.push_back({document::SketchElementId{1}, std::move(geometry)});
    parameters.elements.nextId = 2;
    require(session.createDatumSketchFeature(parameters).status() == CommandStatus::committed,
            "Sketch");
    return session.features().back().id;
}

application::CommandResult helix(application::CadSession& session, document::FeatureId profile,
                                 bool left,
                                 std::optional<document::ExtrusionOperation> operation = {},
                                 std::optional<document::BodyId> target = {}) {
    const auto regions =
        features::identifySketchRegions(session.sketchParameters(profile).elements.values).ids;
    application::ExtrusionMode mode;
    mode.reversed = left;
    mode.operation = operation;
    mode.target = target;
    mode.revolve = document::RevolveParameters{
        {std::nullopt, document::BaseAxis::z, std::nullopt}, kernel::Radians{8 * pi}, 5.0};
    return session.createSketchProfileExtrusion(profile, regions, 1, mode);
}

void spring() {
    const double expected = 2 * pi * pi * 1 * 10 * 4;
    for (const bool left : {false, true}) {
        application::CadSession session;
        session.newDocument();
        const auto profile =
            sketch(session, document::DatumPlane::xz, document::SketchCircle{{10, 0}, 1});
        const auto created = helix(session, profile, left);
        if (created.status() != CommandStatus::committed)
            for (const auto& issue : created.evaluation().features)
                std::cerr << issue.code << ' ' << issue.message << std::endl;
        require(created.status() == CommandStatus::committed &&
                    session.features().back().name == "Wendel 1",
                "Wendel 1");
        require(std::abs(total(session) - expected) < 0.01 * expected,
                std::string(left ? "Left" : "Right") +
                    "-handed spring within 1 %: " + std::to_string(total(session)));
        if (left)
            continue;
        const auto feature = session.features().back().id;
        const auto directory = std::filesystem::temp_directory_path() / "voelund-m26-helix";
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(directory);
        const auto file = directory / "helix.volund";
        session.saveDocument(file);
        application::CadSession reopened;
        reopened.loadDocument(file);
        require(
            reopened.identifiedSnapshot().snapshot().extrusionParameters(feature).revolve ==
                    session.identifiedSnapshot().snapshot().extrusionParameters(feature).revolve &&
                std::abs(total(reopened) - total(session)) < 1e-6 * expected,
            "Open restores the helix");
        std::filesystem::remove_all(directory);
        require(session.undo().status() == CommandStatus::committed &&
                    session.validBodies().empty(),
                "Undo removes the helix");
    }
}

void threadGroove() {
    application::CadSession session;
    session.newDocument();
    const auto base = sketch(session, document::DatumPlane::xy, document::SketchCircle{{0, 0}, 10});
    const auto regions =
        features::identifySketchRegions(session.sketchParameters(base).elements.values).ids;
    require(session.createSketchProfileExtrusion(base, regions, 30).status() ==
                CommandStatus::committed,
            "A cylinder of radius 10");
    const auto body = document::BodyId{base.value()};
    const double before = total(session);
    const auto profile =
        sketch(session, document::DatumPlane::xz, document::SketchCircle{{10, 5}, 1});
    require(helix(session, profile, false, document::ExtrusionOperation::cut, body).status() ==
                CommandStatus::committed,
            "Thread groove");
    const double removed = before - total(session);
    const double spring = 2 * pi * pi * 1 * 10 * 4;
    require(removed > 0.3 * spring && removed < 0.7 * spring,
            "About half the spring is cut away: " + std::to_string(removed));
}

} // namespace

int main() {
    try {
        spring();
        threadGroove();
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
    return 0;
}
