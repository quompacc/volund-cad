// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CadSessionTestSupport.hpp"

#include <voelund/application/CadSession.hpp>

#include <cmath>
#include <filesystem>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>

// M27 (user decision 2026-10-14): sphere, cone, torus and wedge. Each new body has its
// analytic volume; a sphere is cut from a box (a cap at first, moved down a whole ball);
// a size change, undo, save and open; invalid sizes are refused.
namespace {
using namespace voelund;
using application::CommandStatus;
using kernel::PrimitiveShape;
constexpr double pi = std::numbers::pi;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-4 * std::max(1.0, std::abs(b)); }
double total(const application::CadSession& session) {
    double sum = 0;
    for (const auto& body : session.validBodies())
        sum += test::volumeOf(body.shape);
    return sum;
}

void volumes() {
    struct Case final {
        PrimitiveShape shape;
        double volume;
        const char* name;
    };
    const Case cases[]{{PrimitiveShape::sphere, 4.0 / 3.0 * pi * 1000, "Kugel 1"},
                       {PrimitiveShape::cone, pi * 20 / 3 * (100 + 40 + 16), "Kegel 1"},
                       {PrimitiveShape::torus, 2 * pi * pi * 15 * 16, "Torus 1"},
                       {PrimitiveShape::wedge, 20 * (20 + 10) / 2.0 * 20, "Keil 1"}};
    for (const auto& item : cases) {
        application::CadSession session;
        session.newDocument();
        const auto created = session.createPrimitive(item.shape, std::nullopt,
                                                     document::CylinderResultMode::separate);
        require(created.status() == CommandStatus::committed &&
                    session.features().back().name == item.name &&
                    session.validBodies().size() == 1,
                std::string("A new body: ") + item.name + " " + created.diagnostic());
        require(near(total(session), item.volume), std::string("Analytic volume of ") + item.name +
                                                       ": " + std::to_string(total(session)));
    }
}

void sphereCutFromBox() {
    application::CadSession session;
    session.newDocument();
    const auto box = *session.createBoxFeature({40, 30, 20}).feature();
    require(session.createPrimitive(PrimitiveShape::sphere, box, document::CylinderResultMode::cut)
                    .status() == CommandStatus::committed,
            "Sphere cut from the box");
    const auto sphere = session.features().back().id;
    require(session.validBodies().size() == 1, "The cut stays in the box body");
    // The sphere sits on the box top, sunk 2 into it: a cap of height 2 is removed.
    const double cap = pi * 4 * (3 * 10 - 2) / 3;
    require(near(total(session), 40 * 30 * 20 - cap), "A cap is cut away");

    auto parameters = session.identifiedSnapshot().snapshot().primitiveParameters(sphere);
    parameters.sizes[0] = 5;
    require(session.editPrimitiveParameters(sphere, parameters).status() ==
                CommandStatus::committed,
            "Radius 5");
    const double smallCap = pi * 4 * (3 * 5 - 2) / 3;
    require(near(total(session), 40 * 30 * 20 - smallCap), "The smaller cap");
    parameters.placement.translationZ = -10;
    require(session.editPrimitiveParameters(sphere, parameters).status() ==
                    CommandStatus::committed &&
                near(total(session), 40 * 30 * 20 - 4.0 / 3.0 * pi * 125),
            "Moved down 10: a whole ball is cut out");
    require(session.undo().status() == CommandStatus::committed &&
                near(total(session), 40 * 30 * 20 - smallCap),
            "Undo returns the cap");

    auto invalid = parameters;
    invalid.sizes[0] = 0;
    require(session.editPrimitiveParameters(sphere, invalid).status() == CommandStatus::rejected,
            "Radius 0 is refused");
    auto cone = parameters;
    cone.shape = PrimitiveShape::cone;
    require(session.editPrimitiveParameters(sphere, cone).status() == CommandStatus::rejected,
            "A primitive keeps its shape");

    const auto directory = std::filesystem::temp_directory_path() / "voelund-m27-primitives";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const auto path = directory / "primitives.volund";
    session.saveDocument(path);
    application::CadSession reopened;
    reopened.loadDocument(path);
    require(reopened.identifiedSnapshot().snapshot().primitiveParameters(sphere) ==
                    session.identifiedSnapshot().snapshot().primitiveParameters(sphere) &&
                reopened.features().back().inputFeature == box &&
                near(total(reopened), total(session)),
            "Open restores the cut sphere");
    std::filesystem::remove_all(directory);
}

} // namespace

int main() {
    try {
        volumes();
        sphereCutFromBox();
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
    return 0;
}
