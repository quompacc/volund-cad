// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CadSessionTestSupport.hpp"

#include <voelund/features/SketchRegionIdentity.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

// N02b (user decision 2026-10-21): three bodies of different kinds moved together by 5 in X -
// a box (its own placement), a body moved before (its move changes) and an extruded plate (a
// new "Bewegen N") - as one undo step; one undo puts all three back. Two boxes suppressed
// together, one undo reactivates both.
namespace {
using namespace voelund;
using application::CommandStatus;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-6 * std::max(1.0, std::abs(b)); }

double maximumX(const application::CadSession& session, document::BodyId body) {
    for (const auto& item : session.validBodies())
        if (item.bodyId == body)
            return test::maximumXOf(item.shape);
    throw std::runtime_error("Body missing");
}

document::FeatureId plate(application::CadSession& session) {
    document::SketchParameters sketch{document::datumSketchPlane(document::DatumPlane::xy), {}};
    sketch.datum = document::DatumPlane::xy;
    sketch.elements.values.push_back(
        {document::SketchElementId{1}, document::SketchRectangle{40, 0, 60, 10}});
    sketch.elements.nextId = 2;
    const auto regions = features::identifySketchRegions(sketch.elements.values).ids;
    const auto result = session.createDatumSketchExtrusion(sketch, regions, 5);
    require(result.feature().has_value(), "Plate");
    return *result.feature();
}

void movedTogether() {
    application::CadSession session;
    session.newDocument();
    const auto box = *session.createBoxFeature({10, 10, 10}).feature();
    const auto other = *session.createBoxFeature({4, 4, 4}).feature();
    require(session.moveBody(other, {20, 0, 0}, std::nullopt, 0).status() ==
                CommandStatus::committed,
            "Second box moved to x 20");
    const auto extruded = plate(session);
    const document::BodyId boxBody{box.value()};
    const document::BodyId otherBody{other.value()};
    const auto plateBody = *session.resultBody(extruded);
    const double before[] = {maximumX(session, boxBody), maximumX(session, otherBody),
                             maximumX(session, plateBody)};
    const std::size_t steps = session.features().size();
    const std::vector<document::FeatureId> group{box, other, extruded};
    require(session.moveBodies(group, {5, 0, 0}).status() == CommandStatus::committed,
            "Moved together");
    require(near(maximumX(session, boxBody), before[0] + 5) &&
                near(maximumX(session, otherBody), before[1] + 5) &&
                near(maximumX(session, plateBody), before[2] + 5),
            "All three 5 further in X");
    require(session.features().size() == steps + 1,
            "Only the plate got a new move; box and moved body changed their own");
    require(session.undo().status() == CommandStatus::committed &&
                near(maximumX(session, boxBody), before[0]) &&
                near(maximumX(session, otherBody), before[1]) &&
                near(maximumX(session, plateBody), before[2]) && session.features().size() == steps,
            "One undo puts all three back");
}

void suppressedTogether() {
    application::CadSession session;
    session.newDocument();
    const auto first = *session.createBoxFeature({10, 10, 10}).feature();
    const auto second = *session.createBoxFeature({5, 5, 5}).feature();
    const std::vector<document::FeatureId> group{first, second};
    require(session.setFeaturesSuppressed(group, true).status() == CommandStatus::committed &&
                session.validBodies().empty(),
            "Both suppressed in one step");
    require(session.undo().status() == CommandStatus::committed &&
                session.validBodies().size() == 2,
            "One undo reactivates both");
}

} // namespace

int main() {
    try {
        movedTogether();
        suppressedTogether();
        std::cout << "PASS N02b group edits: three kinds of bodies moved together, one undo; two "
                     "steps suppressed together, one undo\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
