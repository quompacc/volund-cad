// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CadSessionTestSupport.hpp"

#include <voelund/application/Actions.hpp>

#include <BRepCheck_Analyzer.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>

// M10 (user decision 2026-10-15): a cube 10 at the origin. Three copies along X 20 apart are
// three bodies at 0, 20 and 40; counts of 0, 1 and 101 are refused; the cube 12 wide drives
// every copy; a fillet on copy 2 (faces named as copies) survives a fourth copy, refuses a count
// that would drop copy 2, undo and save/open; joined copies 20 apart are refused. Four copies
// about Z over 360 degrees stand at 0, 90, 180 and 270 degrees without a doubled first one and
// join into one square; three over 90 degrees include both ends.
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
bool solidAt(const application::CadSession& session, double x, double y, double z) {
    for (const auto& body : session.validBodies())
        if (BRepClass3d_SolidClassifier(body.shape, gp_Pnt(x, y, z), 1e-7).State() == TopAbs_IN)
            return true;
    return false;
}
std::vector<document::FeatureId> copies(const application::CadSession& session) {
    std::vector<document::FeatureId> result;
    for (const auto& feature : session.features())
        if (feature.kind == document::FeatureKind::patternCopy)
            result.push_back(feature.id);
    return result;
}
application::CommandResult create(application::CadSession& session, document::FeatureId support,
                                  const application::PatternInput& input) {
    return application::executeAction(
               session, application::prepareRequest(
                            session, application::CreatePatternAction{support, input}))
        .command;
}

void linearPattern() {
    application::CadSession session;
    session.newDocument();
    const auto box = *session.createBoxFeature({10, 10, 10}).feature();
    application::PatternInput input;
    input.baseAxis = document::BaseAxis::x;
    input.count = 3;
    input.spacing = 20;
    require(create(session, box, input).status() == CommandStatus::committed &&
                session.features()[1].name == "Muster 1" && copies(session).size() == 2 &&
                session.features()[2].name == "Muster 1 Kopie 1" &&
                session.validBodies().size() == 3 && near(volume(session), 3000) && valid(session),
            "Three cubes as three bodies");
    const auto pattern = session.features()[1].id;
    require(solidAt(session, 5, 5, 5) && solidAt(session, 25, 5, 5) && solidAt(session, 45, 5, 5) &&
                !solidAt(session, 15, 5, 5) && !solidAt(session, 65, 5, 5),
            "The cubes stand at 0, 20 and 40");

    auto values = session.patternParameters(pattern);
    for (const std::uint32_t count : {0U, 1U, 101U}) {
        auto wrong = values;
        wrong.count = count;
        require(session.editPatternParameters(pattern, wrong).status() == CommandStatus::rejected &&
                    copies(session).size() == 2,
                "Count " + std::to_string(count) + " is refused");
    }

    auto wider = session.identifiedSnapshot().snapshot().boxParameters(box);
    wider.dimensions.width = 12;
    require(session.editBoxParameters(box, wider).status() == CommandStatus::committed &&
                near(volume(session), 3 * 1200) && solidAt(session, 51, 5, 5),
            "The wider cube drives every copy");

    auto joined = session.patternParameters(pattern);
    joined.mode = document::PatternMode::join;
    const auto disjoint = session.editPatternParameters(pattern, joined);
    const auto issue = std::ranges::find(disjoint.evaluation().features, pattern,
                                         &features::EvaluationIssue::feature);
    require(disjoint.status() == CommandStatus::rejected &&
                issue != disjoint.evaluation().features.end() && issue->code == "join-disjoint" &&
                copies(session).size() == 2,
            "Joined cubes 20 apart are refused at the pattern");

    // An edge of copy 2: its top and its +X face are copies of the cube's faces.
    const auto copy = [&](kernel::PrimitiveRole role) {
        document::SubshapeOrigin origin{box, role};
        origin.instances.push_back({pattern, 2});
        return origin;
    };
    const document::FilletEdge edge{copy(kernel::PrimitiveRole::boxPositiveZ),
                                    copy(kernel::PrimitiveRole::boxPositiveX), 1};
    const auto second = copies(session)[1];
    const double rounded = 3 * 1200 - 10 * 4 * (1 - pi / 4);
    require(session.createFillet(second, std::span{&edge, 1}, 2).status() ==
                    CommandStatus::committed &&
                near(volume(session), rounded) && valid(session),
            "A fillet on copy 2");

    values = session.patternParameters(pattern);
    values.count = 4;
    require(session.editPatternParameters(pattern, values).status() == CommandStatus::committed &&
                copies(session).size() == 3 && copies(session)[1] == second &&
                near(volume(session), rounded + 1200) && solidAt(session, 65, 5, 5),
            "A fourth cube; copy 2 and its fillet stay");
    values.count = 2;
    require(session.editPatternParameters(pattern, values).status() == CommandStatus::rejected &&
                copies(session).size() == 3,
            "A count that drops the filleted copy is refused");
    require(session.undo().status() == CommandStatus::committed && copies(session).size() == 2 &&
                near(volume(session), rounded),
            "Undo removes the fourth cube");

    const auto directory = std::filesystem::temp_directory_path() / "voelund-m10-pattern";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const auto path = directory / "pattern.volund";
    session.saveDocument(path);
    application::CadSession reopened;
    reopened.loadDocument(path);
    require(reopened.patternParameters(pattern) == session.patternParameters(pattern) &&
                copies(reopened) == copies(session) && reopened.validBodies().size() == 3 &&
                near(volume(reopened), rounded),
            "Open restores the pattern, its copies and the fillet on copy 2");
    std::filesystem::remove_all(directory);
}

void circularPattern() {
    application::CadSession session;
    session.newDocument();
    const auto box = *session.createBoxFeature({10, 10, 10}).feature();
    application::PatternInput input;
    input.kind = document::PatternKind::circular;
    input.baseAxis = document::BaseAxis::z;
    input.count = 4;
    input.angle = 2 * pi;
    require(create(session, box, input).status() == CommandStatus::committed &&
                session.validBodies().size() == 4 && near(volume(session), 4000) &&
                solidAt(session, 5, 5, 5) && solidAt(session, -5, 5, 5) &&
                solidAt(session, -5, -5, 5) && solidAt(session, 5, -5, 5) && valid(session),
            "Four cubes at 0, 90, 180 and 270 degrees, none doubled");
    const auto pattern = session.features()[1].id;

    auto values = session.patternParameters(pattern);
    values.mode = document::PatternMode::join;
    require(session.editPatternParameters(pattern, values).status() == CommandStatus::committed &&
                copies(session).empty() && session.validBodies().size() == 1 &&
                near(volume(session), 4000) && valid(session),
            "Joined into one square 20 x 20");

    values.mode = document::PatternMode::newBodies;
    values.count = 3;
    values.angle = kernel::Radians{pi / 2};
    require(session.editPatternParameters(pattern, values).status() == CommandStatus::committed &&
                copies(session).size() == 2 && session.validBodies().size() == 3 &&
                solidAt(session, -5, 5, 5) && solidAt(session, 0, 9, 5) &&
                !solidAt(session, -5, -5, 5),
            "Three over 90 degrees include both ends");
}

} // namespace

int main() {
    try {
        linearPattern();
        circularPattern();
        std::cout << "PASS M10 pattern: linear 3 x 20, counts refused, master edit, fillet on a "
                     "copy, fourth copy, undo, save/open; circular 4 x 360 and joined, 3 x 90\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
