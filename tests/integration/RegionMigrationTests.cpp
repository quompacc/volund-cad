// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/application/CadSession.hpp>
#include <voelund/features/SketchEvaluation.hpp>

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>

// R09b: Open migrates legacy minimum-element profile selections to regions with the
// documented mapping and keeps unmappable selections for repair.
namespace {
using namespace voelund;
namespace fs = std::filesystem;
void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
fs::path legacyFile(const fs::path& directory, const std::string& name,
                    const std::string& profiles) {
    const auto path = directory / name;
    std::ofstream output(path, std::ios::binary);
    output << "VOLUND 16\nNEXT_ID 5\nFEATURES 3\n"
              "BOX 1 \"Support\" 20 20 30 0 0 0 0 0 0\n"
              "SKETCH 3 \"Sketch\" 1 0 0 30 0 0 1 1 0 0 2 3 0 1\n"
              "ELEMENT RECTANGLE 1 2 2 10 8 0\n"
              "ELEMENT CIRCLE 2 6 5 1 0\n"
              "EXTRUSION 4 \"Extrusion\" 3 5 "
           << profiles << "\n";
    require(!output.fail(), "Fixture write failed");
    return path;
}
// Legacy sketches without a support binding await R06 repair, so the session shows no
// current body; the profile faces prove that migration kept the extruded area.
double area(const application::CadSession& session, bool legacy,
            std::vector<document::SketchProfileId> profiles = {}) {
    const auto snapshot = session.identifiedSnapshot().snapshot();
    const auto& sketch = snapshot.sketchParameters(document::FeatureId{3});
    const auto faces = legacy ? features::evaluateNamedProfileFaces(sketch, profiles)
                              : features::evaluateExtrusionFaces(
                                    sketch, snapshot.extrusionParameters(document::FeatureId{4}));
    double total = 0;
    for (const auto& face : faces) {
        GProp_GProps properties;
        BRepGProp::SurfaceProperties(face.face, properties);
        total += properties.Mass();
    }
    return total;
}
// By value: identifiedSnapshot() returns a temporary snapshot.
document::ExtrusionParameters extrusion(const application::CadSession& session) {
    return session.identifiedSnapshot().snapshot().extrusionParameters(document::FeatureId{4});
}
std::string texts(const document::ExtrusionParameters& parameters) {
    std::string result;
    for (const auto& region : parameters.regions)
        result += region.text() + " | ";
    return result;
}

void migrated(const fs::path& work) {
    application::CadSession session;
    session.loadDocument(legacyFile(work, "rectangle.volund", "1 1"));
    require(extrusion(session).profiles.empty() &&
                texts(extrusion(session)) == "o:1.0,1.1,1.2,1.3;h:2.0+ | o:2.0+ | ",
            "Rectangle profile maps to ring plus inner disc");
    require(std::abs(area(session, false) - 48) < 1e-9 &&
                std::abs(area(session, true, {document::SketchProfileId{1}}) - 48) < 1e-9,
            "Migration changed the legacy filled area");
    const auto native = work / "rectangle-native.volund";
    session.saveDocument(native);
    application::CadSession reopened;
    reopened.loadDocument(native);
    require(extrusion(reopened) == extrusion(session), "Migrated regions lost on save/open");

    application::CadSession circle;
    circle.loadDocument(legacyFile(work, "circle.volund", "1 2"));
    require(texts(extrusion(circle)) == "o:2.0+ | " &&
                std::abs(area(circle, false) - std::numbers::pi) < 1e-9,
            "Circle profile maps to the inner disc");
    std::cout << "legacy rectangle/circle profiles migrate to regions without geometry change\n";
}

void unmappable(const fs::path& work) {
    application::CadSession session;
    session.loadDocument(legacyFile(work, "unknown.volund", "1 9"));
    require(extrusion(session).regions.empty() && extrusion(session).profiles.size() == 1 &&
                extrusion(session).profiles.front() == document::SketchProfileId{9},
            "Unmappable profile was replaced instead of kept for repair");
    const auto states = session.featureStates();
    require(std::ranges::any_of(states,
                                [](const auto& state) {
                                    return state.id == document::FeatureId{4} &&
                                           state.status !=
                                               application::FeatureRuntimeStatus::current;
                                }),
            "Unmappable profile must not evaluate as current");
    std::cout << "unmappable legacy profile is kept, never guessed\n";
}
} // namespace

int main(int argc, char** argv) {
    try {
        require(argc == 2, "Usage: voelund_region_migration_tests WORK_DIRECTORY");
        const fs::path work = fs::path(argv[1]) / "region-migration";
        fs::create_directories(work);
        migrated(work);
        unmappable(work);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
