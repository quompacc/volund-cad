// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CadSessionTestSupport.hpp"

#include <voelund/features/SketchRegionIdentity.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>

// M18 (A-PART-02): two concentric circles R20/r10 on XY, the ring region chosen explicitly and
// extruded 8 (2400 pi); a box 10 x 10 x 10 placed well apart in X. The outer radius 20 -> 24
// gives 3808 pi, the box stays. Making both radii 10 lets the ring fail while the box stays
// valid, and no filled circle appears. The state with its error survives save/open; setting the
// radius back repairs it. Undo walks back through the whole chain.
namespace {
using namespace voelund;
using application::CommandStatus;
using application::ErrorDisposition;
constexpr double pi = std::numbers::pi;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-6 * std::max(1.0, std::abs(b)); }

double bodyVolume(const application::CadSession& session, document::BodyId body) {
    for (const auto& item : session.validBodies())
        if (item.bodyId == body)
            return test::volumeOf(item.shape);
    return -1;
}
application::FeatureRuntimeStatus status(const application::CadSession& session,
                                         document::FeatureId feature) {
    const auto states = session.featureStates();
    const auto found = std::ranges::find(states, feature, &application::FeatureRuntimeState::id);
    require(found != states.end(), "Feature state");
    return found->status;
}
document::SketchParameters ringSketch(double outer, double inner) {
    document::SketchParameters ring{document::datumSketchPlane(document::DatumPlane::xy), {}};
    ring.datum = document::DatumPlane::xy;
    ring.elements.values.push_back(
        {document::SketchElementId{1}, document::SketchCircle{{0, 0}, outer}});
    ring.elements.values.push_back(
        {document::SketchElementId{2}, document::SketchCircle{{0, 0}, inner}});
    ring.elements.nextId = 3;
    return ring;
}

void ringAndBox(const std::filesystem::path& directory) {
    application::CadSession session;
    session.newDocument();
    // The ring region, chosen explicitly: the region with a hole.
    auto sketch = ringSketch(20, 10);
    const auto identities = features::identifySketchRegions(sketch.elements.values);
    std::optional<document::SketchRegionId> ringRegion;
    for (std::size_t index = 0; index < identities.regions.regions.size(); ++index)
        if (!identities.regions.regions[index].holes.empty())
            ringRegion = identities.ids[index];
    require(ringRegion.has_value(), "The ring region");
    const auto ring =
        session.createDatumSketchExtrusion(sketch, std::span{&*ringRegion, 1}, 8).feature();
    require(ring.has_value(), "Ring extruded");
    const document::BodyId ringBody = *session.resultBody(*ring);
    const auto descriptors = session.features();
    const auto sketchId =
        *std::ranges::find(descriptors, *ring, &document::FeatureDescriptor::id)->inputFeature;
    require(near(bodyVolume(session, ringBody), 2400 * pi), "Ring of 2400 pi");

    const auto box = session.createBoxFeature({10, 10, 10}).feature();
    require(box.has_value(), "Box");
    document::BoxParameters parameters{
        {10, 10, 10}, {60, 0, 0, kernel::Radians{0}, kernel::Radians{0}, kernel::Radians{0}}};
    require(session.editBoxParameters(*box, parameters).status() == CommandStatus::committed,
            "Box placed at x 60");
    const document::BodyId boxBody{box->value()};
    require(near(bodyVolume(session, boxBody), 1000) && session.validBodies().size() == 2,
            "Box 1000 apart");

    require(session.editSketchParameters(sketchId, ringSketch(24, 10)).status() ==
                CommandStatus::committed,
            "Outer radius 24");
    require(near(bodyVolume(session, ringBody), 3808 * pi) &&
                near(bodyVolume(session, boxBody), 1000),
            "3808 pi, the box unchanged");

    // Both radii 10: the ring fails, the box stays, no filled circle appears.
    require(session.editSketchParameters(sketchId, ringSketch(10, 10),
                                         ErrorDisposition::keepEditableErrors)
                    .status() == CommandStatus::committedWithErrors,
            "Coinciding radii kept with their error");
    require(status(session, *ring) == application::FeatureRuntimeStatus::error &&
                status(session, *box) == application::FeatureRuntimeStatus::current &&
                session.validBodies().size() == 1 && near(bodyVolume(session, boxBody), 1000),
            "Ring failed, box valid, no filled circle");

    // The error survives save/open; the radius set back repairs it.
    const auto file = directory / "ring.volund";
    session.saveDocument(file);
    application::CadSession reopened;
    reopened.loadDocument(file);
    require(status(reopened, *ring) == application::FeatureRuntimeStatus::error &&
                reopened.validBodies().size() == 1,
            "The error is still there after opening");
    require(reopened.editSketchParameters(sketchId, ringSketch(20, 10)).status() ==
                CommandStatus::committed,
            "Repaired with radius 20");
    require(near(bodyVolume(reopened, ringBody), 2400 * pi) &&
                near(bodyVolume(reopened, boxBody), 1000) &&
                status(reopened, *ring) == application::FeatureRuntimeStatus::current,
            "Ring 2400 pi again, box unchanged");

    // Undo walks back: error, 3808 pi.
    require(session.undo().status() == CommandStatus::committed &&
                near(bodyVolume(session, ringBody), 3808 * pi),
            "Undo returns to 3808 pi");
    require(session.undo().status() == CommandStatus::committed &&
                near(bodyVolume(session, ringBody), 2400 * pi),
            "Undo returns to 2400 pi");
}

} // namespace

int main() {
    try {
        const auto directory = std::filesystem::temp_directory_path() / "voelund-m18-ring";
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(directory);
        ringAndBox(directory);
        std::filesystem::remove_all(directory);
        std::cout << "PASS M18 A-PART-02: ring region 2400 pi, box apart, radius 24 gives 3808 "
                     "pi, coinciding radii fail the ring only, error kept over save/open and "
                     "repaired, undo\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
