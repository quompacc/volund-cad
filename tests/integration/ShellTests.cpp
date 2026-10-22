// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CadSessionTestSupport.hpp"

#include <voelund/application/Actions.hpp>

#include <BRepCheck_Analyzer.hxx>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

// M22 (user decision 2026-10-14): a box 40 x 30 x 20 with its top opened and walls 2 thick
// keeps 24000 - 36 * 26 * 18; with the front opened as well 24000 - 36 * 28 * 18; walls of 20
// are refused; the box 30 high follows; outward walls grow the box; undo; save/open.
namespace {
using namespace voelund;
using application::CommandStatus;

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
document::SubshapeOrigin face(const application::CadSession& session, document::FeatureId box,
                              double nx, double ny, double nz) {
    for (const auto& candidate : session.supportFaces(box))
        if (candidate.plane.normalX * nx + candidate.plane.normalY * ny +
                candidate.plane.normalZ * nz >
            0.99)
            return candidate.origin;
    throw std::runtime_error("Face missing");
}

void hollowBox() {
    application::CadSession session;
    session.newDocument();
    const auto box = *session.createBoxFeature({40, 30, 20}).feature();
    const auto top = face(session, box, 0, 0, 1);
    const auto front = face(session, box, 0, -1, 0);
    const auto created = application::executeAction(
        session, application::prepareRequest(
                     session, application::CreateShellAction{
                                  box, {top}, {2, application::QuantityUnit::millimetre}, false}));
    require(created.command.status() == CommandStatus::committed &&
                session.features().back().name == "Wandstärke 1" &&
                near(volume(session), 24000 - 36 * 26 * 18) && valid(session),
            "Walls 2 thick with the top open");
    const auto shell = session.features().back().id;

    auto parameters = session.shellParameters(shell);
    parameters.openFaces.push_back(front);
    require(session.editShellParameters(shell, parameters).status() == CommandStatus::committed &&
                near(volume(session), 24000 - 36 * 28 * 18) && valid(session),
            "The front opens as well");

    auto thick = parameters;
    thick.thickness = 20;
    require(session.editShellParameters(shell, thick).status() == CommandStatus::rejected &&
                near(volume(session), 24000 - 36 * 28 * 18),
            "Walls of 20 are refused");

    parameters.openFaces.pop_back();
    require(session.editShellParameters(shell, parameters).status() == CommandStatus::committed,
            "Only the top open again");
    auto taller = session.identifiedSnapshot().snapshot().boxParameters(box);
    taller.dimensions.height = 30;
    require(session.editBoxParameters(box, taller).status() == CommandStatus::committed &&
                near(volume(session), 36000 - 36 * 26 * 28) && valid(session),
            "The shell follows the taller box");
    require(session.undo().status() == CommandStatus::committed &&
                near(volume(session), 24000 - 36 * 26 * 18),
            "Undo returns to the box 20 high");

    parameters.outward = true;
    require(session.editShellParameters(shell, parameters).status() == CommandStatus::committed &&
                near(volume(session), 44 * 34 * 22 - 24000) && valid(session),
            "Outward walls grow the box");

    const auto directory = std::filesystem::temp_directory_path() / "voelund-m22-shell";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const auto path = directory / "shell.volund";
    session.saveDocument(path);
    application::CadSession reopened;
    reopened.loadDocument(path);
    require(reopened.shellParameters(shell) == session.shellParameters(shell) &&
                near(volume(reopened), volume(session)),
            "Open restores the shell");
    std::filesystem::remove_all(directory);
}

} // namespace

int main() {
    try {
        hollowBox();
        std::cout << "PASS M22 shell: top open, front open, too thick refused, taller box, "
                     "undo, outward, save/open\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
