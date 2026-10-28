// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CadSessionTestSupport.hpp"

#include <voelund/application/Actions.hpp>

#include <BRepClass3d_SolidClassifier.hxx>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// N11a (schema 47): a cube 10 in a grid of 3 along X (20 apart) by 4 along Y (15 apart) is 12
// bodies; skipping copy 4 (the second along X in the second row, at (20; 15)) leaves 11, undo
// and Save/Open keep the grid and the gap. Joined, 3 x 2 overlapping cubes 8 apart without the
// corner copy 5 fill 404 mm² x 10. A circular grid, skipping the original, a skipped copy out
// of range and more than 100 instances are refused.
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
bool solidAt(const application::CadSession& session, double x, double y, double z) {
    for (const auto& body : session.validBodies())
        if (BRepClass3d_SolidClassifier(body.shape, gp_Pnt(x, y, z), 1e-7).State() == TopAbs_IN)
            return true;
    return false;
}
std::size_t copies(const application::CadSession& session) {
    return static_cast<std::size_t>(std::ranges::count(session.features(),
                                                       document::FeatureKind::patternCopy,
                                                       &document::FeatureDescriptor::kind));
}
application::CommandResult create(application::CadSession& session, document::FeatureId support,
                                  const application::PatternInput& input) {
    return application::executeAction(
               session, application::prepareRequest(
                            session, application::CreatePatternAction{support, input}))
        .command;
}
application::PatternInput grid(std::uint32_t columns, double spacing, std::uint32_t rows,
                               double rowSpacing) {
    application::PatternInput input;
    input.baseAxis = document::BaseAxis::x;
    input.count = columns;
    input.spacing = spacing;
    document::PatternDirection second;
    second.axis.baseAxis = document::BaseAxis::y;
    second.count = rows;
    second.spacing = rowSpacing;
    input.second = second;
    return input;
}

void skipAndKeep(application::CadSession& session);
void reopened(application::CadSession& session, const document::PatternParameters& parameters);

void newBodies() {
    application::CadSession session;
    session.newDocument();
    const auto box = *session.createBoxFeature({10, 10, 10}).feature();
    require(create(session, box, grid(3, 20, 4, 15)).status() == CommandStatus::committed &&
                session.validBodies().size() == 12 && copies(session) == 11 &&
                near(volume(session), 12000) && solidAt(session, 45, 50, 5),
            "3 x 4 cubes as 12 bodies, the last at (40; 45)");
    skipAndKeep(session);
}

void skipAndKeep(application::CadSession& session) {
    const auto pattern = session.features()[1].id;
    auto parameters = session.patternParameters(pattern);
    parameters.skipped = {4};
    require(session.editPatternParameters(pattern, parameters).status() ==
                    CommandStatus::committed &&
                session.validBodies().size() == 11 && copies(session) == 10 &&
                !solidAt(session, 25, 20, 5) && solidAt(session, 5, 20, 5),
            "Skipping copy 4 leaves (20; 15) empty");
    require(session.undo().status() == CommandStatus::committed &&
                session.validBodies().size() == 12 && solidAt(session, 25, 20, 5) &&
                session.redo().status() == CommandStatus::committed,
            "Undo brings copy 4 back");
    reopened(session, parameters);
}

void reopened(application::CadSession& session, const document::PatternParameters& parameters) {
    const auto pattern = session.features()[1].id;
    const auto path = test::temporaryFilePath("voelund-n11a-grid.volund");
    session.saveDocument(path);
    application::CadSession loaded;
    loaded.loadDocument(path);
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    require(loaded.patternParameters(pattern) == parameters && loaded.validBodies().size() == 11 &&
                near(volume(loaded), 11000) && !solidAt(loaded, 25, 20, 5),
            "Save/Open keeps the grid and the skipped copy");
}

void joined() {
    application::CadSession session;
    session.newDocument();
    const auto box = *session.createBoxFeature({10, 10, 10}).feature();
    auto input = grid(3, 8, 2, 8);
    input.mode = document::PatternMode::join;
    input.skipped = {5};
    require(create(session, box, input).status() == CommandStatus::committed &&
                session.validBodies().size() == 1 && near(volume(session), 4040) &&
                !solidAt(session, 22, 14, 5),
            "Joined 3 x 2 without the corner copy: 404 mm² x 10");
}

void refused() {
    application::CadSession session;
    session.newDocument();
    const auto box = *session.createBoxFeature({10, 10, 10}).feature();
    auto circular = grid(3, 20, 2, 20);
    circular.kind = document::PatternKind::circular;
    circular.angle = 1.0;
    auto original = grid(3, 20, 2, 20);
    original.skipped = {0};
    auto beyond = grid(3, 20, 2, 20);
    beyond.skipped = {6};
    for (const auto& input : {circular, original, beyond, grid(11, 20, 10, 20)})
        require(create(session, box, input).status() != CommandStatus::committed &&
                    session.features().size() == 1,
                "An invalid grid is refused");
}
} // namespace

int main() {
    try {
        newBodies();
        joined();
        refused();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "PASS N11a grid patterns with skipped copies\n";
    return 0;
}
