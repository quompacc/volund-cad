// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CadSessionTestSupport.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// M01 (A-DAG-01, A-BODY-03 step 6): bodies A and B, each a base-plane sketch and an extrusion,
// A also with a joined cylinder. An error only in A's extrusion leaves B valid and editable,
// blocks only A's successor, and the repair brings A back with unchanged body IDs. Diamond,
// merge, cycle refusal and reordered roots: features.dependency_dag and features.body_identity.
namespace {
using namespace voelund;
using application::CommandStatus;
using application::FeatureRuntimeStatus;
using document::DatumPlane;
using document::FeatureId;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
FeatureId committed(const application::CommandResult& result) {
    require(result.status() == CommandStatus::committed && result.feature().has_value(),
            "Not committed: " + result.diagnostic());
    return *result.feature();
}
document::SketchParameters onDatum(DatumPlane datum, document::SketchRectangle rectangle) {
    document::SketchParameters sketch{
        document::datumSketchPlane(datum),
        document::SketchElements{{{document::SketchElementId{1}, rectangle}}, 2}};
    sketch.datum = datum;
    return sketch;
}
FeatureId extrude(application::CadSession& session, FeatureId sketch, double distance) {
    require(session.selectFeature(sketch) &&
                session.selectSketchProfileByElement(document::SketchElementId{1}, false),
            "Select the region");
    const std::vector<document::SketchRegionId> regions(session.selectedSketchProfiles().begin(),
                                                        session.selectedSketchProfiles().end());
    return committed(session.createSketchProfileExtrusion(sketch, regions, distance));
}
FeatureRuntimeStatus status(const application::CadSession& session, FeatureId feature) {
    for (const auto& state : session.featureStates())
        if (state.id == feature)
            return state.status;
    throw std::runtime_error("No state");
}
std::vector<std::uint64_t> bodies(const application::CadSession& session) {
    std::vector<std::uint64_t> ids;
    for (const auto& body : session.modelOutline().bodies)
        ids.push_back(body.id.value());
    return ids;
}

void run() {
    application::CadSession session;
    session.newDocument();
    const auto sketchA =
        committed(session.createDatumSketchFeature(onDatum(DatumPlane::xy, {0, 0, 20, 10})));
    const auto prismA = extrude(session, sketchA, 10);
    const auto cylinderA =
        committed(session.createCylinderFeature(prismA, document::CylinderResultMode::join));
    const auto sketchB =
        committed(session.createDatumSketchFeature(onDatum(DatumPlane::xz, {40, 0, 50, 5})));
    const auto prismB = extrude(session, sketchB, 4);
    const auto ids = bodies(session);
    require(ids.size() == 2 && session.validBodies().size() == 2, "Two bodies A and B");

    // Error only in A's branch: A's sketch becomes an open line, its region is gone.
    auto open = session.identifiedSnapshot().snapshot().sketchParameters(sketchA);
    open.elements.values.front().geometry = document::SketchLine{{0, 0}, {20, 0}};
    require(session.editSketchParameters(sketchA, open,
                                         application::ErrorDisposition::keepEditableErrors)
                    .status() == CommandStatus::committedWithErrors,
            "A's sketch opened on purpose");
    require(status(session, prismA) == FeatureRuntimeStatus::error &&
                status(session, cylinderA) == FeatureRuntimeStatus::blocked &&
                status(session, prismB) == FeatureRuntimeStatus::current &&
                session.validBodies().size() == 1 &&
                session.validBodies().front().bodyId.value() == ids[1],
            "Only A's successor is blocked; B stays valid");

    // B stays editable while A is broken.
    require(session.selectFeature(prismB), "Select B's extrusion");
    auto longer = session.selectedExtrusionParameters();
    longer.distance = 6;
    // Committed; the document as a whole still reports A's error.
    const auto edit = session.editSelectedExtrusionParameters(longer);
    require(edit.status() == CommandStatus::committedWithErrors &&
                session.selectedExtrusionParameters().distance == 6 &&
                status(session, prismB) == FeatureRuntimeStatus::current &&
                session.validBodies().size() == 1,
            "B is edited despite the error in A");

    // Repair A: everything current again, body IDs unchanged.
    // The first undo still leaves A broken (committed with errors), the second repairs it.
    require(session.undo().status() == CommandStatus::committedWithErrors &&
                session.undo().status() == CommandStatus::committed,
            "Undo B's edit and A's break");
    require(status(session, prismA) == FeatureRuntimeStatus::current &&
                status(session, cylinderA) == FeatureRuntimeStatus::current &&
                session.validBodies().size() == 2 && bodies(session) == ids,
            "After the repair both bodies are valid with their IDs");
}
} // namespace

int main() {
    try {
        run();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "PASS M01 body DAG (A-DAG-01): isolated branch error, editable B, repair\n";
    return 0;
}
