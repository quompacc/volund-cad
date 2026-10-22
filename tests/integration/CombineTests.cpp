// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CadSessionTestSupport.hpp"

#include <voelund/application/Actions.hpp>

#include <BRepCheck_Analyzer.hxx>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

// M21 (user decision 2026-10-14): a Boolean of whole bodies. Box 40 x 30 x 10 and a separate
// cylinder r 5, h 20 through its middle: cut removes 250 pi, the cylinder stays editable,
// kept tools stay separate bodies, join and intersect, a combination that leaves nothing is
// refused, undo, save/open (schema 18).
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

void combineBodies() {
    application::CadSession session;
    session.newDocument();
    const auto box = *session.createBoxFeature({40, 30, 10}).feature();
    const auto created = session.createNewBodyCylinder();
    require(created.status() == CommandStatus::committed, "Cylinder");
    const auto cylinder = *created.feature();
    auto parameters = session.cylinderParameters(cylinder);
    parameters.radius = 5;
    parameters.height = 20;
    parameters.placement.translationX = 20;
    parameters.placement.translationY = 15;
    parameters.placement.translationZ = -5;
    require(session.editCylinderParameters(cylinder, parameters).status() ==
                    CommandStatus::committed &&
                session.validBodies().size() == 2,
            "Two separate bodies");

    const auto cut = application::executeAction(
        session, application::prepareRequest(
                     session, application::CreateCombineAction{
                                  box, {cylinder}, document::CombineOperation::cut, false}));
    require(cut.command.status() == CommandStatus::committed &&
                session.features().back().name == "Kombinieren 1" &&
                session.validBodies().size() == 1 && near(volume(session), 12000 - 250 * pi) &&
                valid(session),
            "Cut removes 250 pi and consumes the cylinder");
    const auto combine = session.features().back().id;

    // The cylinder stays editable through its own feature.
    parameters.radius = 4;
    require(session.editCylinderParameters(cylinder, parameters).status() ==
                    CommandStatus::committed &&
                near(volume(session), 12000 - 160 * pi),
            "The cylinder radius drives the result");

    auto values = session.combineParameters(combine);
    values.keepTools = true;
    require(session.editCombineParameters(combine, values).status() == CommandStatus::committed &&
                session.validBodies().size() == 2 &&
                near(volume(session), 12000 - 160 * pi + 320 * pi),
            "Kept tools stay separate bodies");

    values.keepTools = false;
    values.operation = document::CombineOperation::join;
    require(session.editCombineParameters(combine, values).status() == CommandStatus::committed &&
                session.validBodies().size() == 1 && near(volume(session), 12000 + 160 * pi) &&
                valid(session),
            "Join adds the cylinder outside the box");
    values.operation = document::CombineOperation::intersect;
    require(session.editCombineParameters(combine, values).status() == CommandStatus::committed &&
                near(volume(session), 160 * pi) && valid(session),
            "Intersect keeps the common volume");

    // A cylinder moved away leaves no common volume: refused, nothing changes.
    auto away = parameters;
    away.placement.translationX = 200;
    require(session.editCylinderParameters(cylinder, away).status() == CommandStatus::rejected &&
                near(volume(session), 160 * pi),
            "An empty intersection is refused");

    require(session.undo().status() == CommandStatus::committed &&
                near(volume(session), 12000 + 160 * pi),
            "Undo returns to the join");

    const auto directory = std::filesystem::temp_directory_path() / "voelund-m21-combine";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const auto path = directory / "combine.volund";
    session.saveDocument(path);
    application::CadSession reopened;
    reopened.loadDocument(path);
    require(reopened.combineParameters(combine) == session.combineParameters(combine) &&
                reopened.validBodies().size() == 1 && near(volume(reopened), volume(session)),
            "Open restores the combination");
    std::filesystem::remove_all(directory);
}

// Two tools in one combination: both cylinders are cut out of the box.
void twoTools() {
    application::CadSession session;
    session.newDocument();
    const auto box = *session.createBoxFeature({40, 30, 10}).feature();
    std::vector<document::FeatureId> tools;
    for (const auto& [radius, x, y] : {std::tuple{5.0, 25.0, 15.0}, std::tuple{3.0, 8.0, 8.0}}) {
        const auto created = *session.createNewBodyCylinder().feature();
        auto parameters = session.cylinderParameters(created);
        parameters.radius = radius;
        parameters.height = 20;
        parameters.placement.translationX = x;
        parameters.placement.translationY = y;
        parameters.placement.translationZ = -5;
        require(session.editCylinderParameters(created, parameters).status() ==
                    CommandStatus::committed,
                "Cylinder placed");
        tools.push_back(created);
    }
    const auto result = session.createCombine(box, tools, document::CombineOperation::cut, false);
    if (result.status() != CommandStatus::committed) {
        std::cerr << result.diagnostic() << std::endl;
        for (const auto& issue : result.evaluation().features)
            std::cerr << issue.feature.value() << ' ' << issue.code << ' ' << issue.message
                      << std::endl;
    }
    require(result.status() == CommandStatus::committed && session.validBodies().size() == 1 &&
                near(volume(session), 12000 - 250 * pi - 90 * pi) && valid(session),
            "Two tools are cut out");
}

// The target and the tools must be different current bodies.
void refused() {
    application::CadSession session;
    session.newDocument();
    const auto box = *session.createBoxFeature({40, 30, 10}).feature();
    require(session.createCombine(box, std::vector{box}, document::CombineOperation::join, false)
                    .status() == CommandStatus::rejected,
            "A body is no tool of itself");
    require(session.createCombine(box, {}, document::CombineOperation::join, false).status() ==
                CommandStatus::rejected,
            "A combination needs a tool");
}

} // namespace

int main() {
    try {
        combineBodies();
        twoTools();
        refused();
        std::cout << "PASS M21 combine: cut 250 pi, tool editable, keep, join, intersect, "
                     "refused, undo, save/open\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
