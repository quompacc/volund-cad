// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CadSessionTestSupport.hpp"

#include <voelund/document/DocumentSnapshot.hpp>
#include <voelund/features/SketchRegionIdentity.hpp>

#include <BRepCheck_Analyzer.hxx>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>

// M20 (user decision 2026-10-14): a rectangle 10 x 20 on XZ, 5 from the Z axis, turned about
// Z: 360 degrees give a tube of pi (15^2 - 5^2) 20 = 4000 pi, 90 degrees a quarter. The same
// about a construction line of the sketch and about a construction axis. A groove (cut) takes
// a ring out of a cylinder; a profile across the axis is refused; a changed profile follows;
// save/open (schema 20).
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
bool valid(const application::CadSession& session) {
    for (const auto& body : session.validBodies())
        if (!BRepCheck_Analyzer(body.shape).IsValid())
            return false;
    return !session.validBodies().empty();
}

// A closed rectangle of four lines on XZ (sketch x = world X, sketch y = world Z), and an
// optional construction line on the sketch's y axis (world Z).
document::SketchParameters rectangle(double x0, double y0, double x1, double y1,
                                     bool withAxis = false) {
    document::SketchParameters sketch{document::datumSketchPlane(document::DatumPlane::xz), {}};
    sketch.datum = document::DatumPlane::xz;
    const document::SketchPoint corners[]{{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
    for (std::uint64_t index = 0; index < 4; ++index)
        sketch.elements.values.push_back(
            {document::SketchElementId{index + 1},
             document::SketchLine{corners[index], corners[(index + 1) % 4]}});
    sketch.elements.nextId = 5;
    if (withAxis) {
        sketch.elements.values.push_back(
            {document::SketchElementId{5}, document::SketchLine{{0, 0}, {0, 10}}, true});
        sketch.elements.nextId = 6;
    }
    return sketch;
}

document::RevolveParameters about(document::RevolveAxis axis, double degrees = 360) {
    return {axis, kernel::Radians{degrees * pi / 180}};
}

// The sketch and a revolve of all its regions; the revolve's feature.
std::optional<document::FeatureId> revolve(application::CadSession& session,
                                           const document::SketchParameters& sketch,
                                           application::ExtrusionMode mode) {
    if (session.createDatumSketchFeature(sketch).status() != CommandStatus::committed)
        return std::nullopt;
    const auto sketchId = session.features().back().id;
    const auto regions = features::identifySketchRegions(sketch.elements.values).ids;
    const auto result = session.createSketchProfileExtrusion(sketchId, regions, 10, mode);
    if (result.status() != CommandStatus::committed) {
        std::cerr << result.diagnostic() << std::endl;
        for (const auto& issue : result.evaluation().features)
            std::cerr << issue.feature.value() << ' ' << issue.message << std::endl;
        return std::nullopt;
    }
    return session.features().back().id;
}

void tube() {
    application::CadSession session;
    session.newDocument();
    application::ExtrusionMode mode;
    mode.revolve = about({std::nullopt, document::BaseAxis::z, std::nullopt});
    const auto turned = revolve(session, rectangle(5, 0, 15, 20), mode);
    require(turned && session.features().back().name == "Drehung 1" &&
                near(volume(session), 4000 * pi) && valid(session),
            "360 degrees about Z give the tube");
    const auto snapshot = session.identifiedSnapshot().snapshot();
    auto parameters = snapshot.extrusionParameters(*turned);
    parameters.revolve->angle = kernel::Radians{pi / 2};
    require(session.editExtrusionParameters(*turned, parameters).status() ==
                    CommandStatus::committed &&
                near(volume(session), 1000 * pi) && valid(session),
            "90 degrees give a quarter");
    parameters.symmetric = true;
    require(session.editExtrusionParameters(*turned, parameters).status() ==
                    CommandStatus::committed &&
                near(volume(session), 1000 * pi),
            "Symmetric keeps the volume");

    // The profile grows outward by 5: the result follows.
    const auto sketchId = session.features().front().id;
    auto sketch = session.sketchParameters(sketchId);
    sketch = rectangle(5, 0, 20, 20);
    require(session.editSketchParameters(sketchId, sketch).status() == CommandStatus::committed &&
                near(volume(session), pi * (400 - 25) * 20 / 4),
            "A wider profile follows");

    const auto directory = std::filesystem::temp_directory_path() / "voelund-m20-revolve";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const auto path = directory / "revolve.volund";
    session.saveDocument(path);
    application::CadSession reopened;
    reopened.loadDocument(path);
    require(reopened.identifiedSnapshot().snapshot().extrusionParameters(*turned) ==
                    session.identifiedSnapshot().snapshot().extrusionParameters(*turned) &&
                near(volume(reopened), volume(session)),
            "Open restores the revolve");
    std::filesystem::remove_all(directory);
}

void otherAxes() {
    {
        application::CadSession session;
        session.newDocument();
        application::ExtrusionMode mode;
        mode.revolve = about({document::SketchElementId{5}, std::nullopt, std::nullopt});
        require(revolve(session, rectangle(5, 0, 15, 20, true), mode) &&
                    near(volume(session), 4000 * pi),
                "About a construction line of the sketch");
    }
    {
        application::CadSession session;
        session.newDocument();
        require(
            session.createDatumAxis(document::BaseAxis::z, std::nullopt, std::nullopt).status() ==
                CommandStatus::committed,
            "Construction axis");
        application::ExtrusionMode mode;
        mode.revolve = about({std::nullopt, std::nullopt, session.features().back().id}, 180);
        require(revolve(session, rectangle(5, 0, 15, 20), mode) && near(volume(session), 2000 * pi),
                "Half a turn about a construction axis");
    }
    {
        application::CadSession session;
        session.newDocument();
        application::ExtrusionMode mode;
        mode.revolve = about({std::nullopt, document::BaseAxis::z, std::nullopt});
        require(!revolve(session, rectangle(-5, 0, 5, 20), mode) && session.validBodies().empty(),
                "A profile across the axis is refused");
    }
}

void groove() {
    application::CadSession session;
    session.newDocument();
    const auto cylinder = *session.createNewBodyCylinder().feature();
    auto parameters = session.cylinderParameters(cylinder);
    parameters.radius = 20;
    parameters.height = 30;
    require(session.editCylinderParameters(cylinder, parameters).status() ==
                CommandStatus::committed,
            "Cylinder r 20, h 30");
    const auto body = *session.resultBody(cylinder);
    application::ExtrusionMode mode;
    mode.operation = document::ExtrusionOperation::cut;
    mode.target = body;
    mode.revolve = about({std::nullopt, document::BaseAxis::z, std::nullopt});
    require(revolve(session, rectangle(15, 10, 25, 15), mode) &&
                session.validBodies().size() == 1 && near(volume(session), 12000 * pi - 875 * pi) &&
                valid(session),
            "The groove takes a ring out of the cylinder");
}

} // namespace

int main() {
    try {
        tube();
        otherAxes();
        groove();
        std::cout << "PASS M20 revolve: tube 4000 pi, quarter, symmetric, profile follows, "
                     "line/construction axis, across the axis refused, groove, save/open\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
