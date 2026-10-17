#include "CadSessionTestSupport.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/features/SketchRegionIdentity.hpp>

#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

// M24 (user decision 2026-10-14): a loft ("Ausformung"). A square of 20 on XY lofted ruled to a
// square of 10 on a construction plane 30 above has the frustum volume 7000; the upper square
// made 8, the body follows; a section in the same plane is refused with its reason; undo,
// save and open.
namespace {
using namespace voelund;
using application::CommandStatus;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-3 * std::max(1.0, std::abs(b)); }
double total(const application::CadSession& session) {
    double sum = 0;
    for (const auto& body : session.validBodies())
        sum += test::volumeOf(body.shape);
    return sum;
}

std::vector<document::SketchGeometry> square(double half) {
    const document::SketchPoint corners[]{
        {-half, -half}, {half, -half}, {half, half}, {-half, half}};
    std::vector<document::SketchGeometry> lines;
    for (int index = 0; index < 4; ++index)
        lines.push_back(document::SketchLine{corners[index], corners[(index + 1) % 4]});
    return lines;
}

document::FeatureId addSketch(application::CadSession& session, document::SketchParameters sketch,
                              std::vector<document::SketchGeometry> geometry) {
    for (auto& item : geometry)
        sketch.elements.values.push_back(
            {document::SketchElementId{sketch.elements.nextId++}, std::move(item)});
    require(session.createDatumSketchFeature(sketch).status() == CommandStatus::committed,
            "Sketch");
    return session.features().back().id;
}

document::SketchParameters basePlane(document::DatumPlane plane) {
    document::SketchParameters sketch{document::datumSketchPlane(plane), {}};
    sketch.datum = plane;
    return sketch;
}

application::CommandResult loft(application::CadSession& session, document::FeatureId profile,
                                document::FeatureId section) {
    const auto regions =
        features::identifySketchRegions(session.sketchParameters(profile).elements.values).ids;
    const auto sectionRegions =
        features::identifySketchRegions(session.sketchParameters(section).elements.values).ids;
    application::ExtrusionMode mode;
    mode.loft = document::LoftParameters{{{section, sectionRegions.front()}}, true};
    return session.createSketchProfileExtrusion(profile, regions, 1, mode);
}

void frustum() {
    application::CadSession session;
    session.newDocument();
    require(session.createDatumPlane(document::DatumPlane::xy, std::nullopt, std::nullopt, 30,
                                     document::DatumRotationAxis::none, 0)
                    .status() == CommandStatus::committed,
            "Plane 30 above XY");
    const auto plane = session.features().back().id;
    document::SketchParameters upper{*session.datumFrame(plane), {}};
    upper.constructionPlane = plane;
    const auto section = addSketch(session, upper, square(5));
    const auto profile = addSketch(session, basePlane(document::DatumPlane::xy), square(10));
    const auto created = loft(session, profile, section);
    if (created.status() != CommandStatus::committed)
        for (const auto& issue : created.evaluation().features)
            std::cerr << issue.code << ' ' << issue.message << std::endl;
    require(created.status() == CommandStatus::committed &&
                session.features().back().name == "Ausformung 1",
            "Ausformung 1");
    const auto feature = session.features().back().id;
    require(near(total(session), 30.0 / 3 * (400 + 100 + 200)),
            "Frustum 7000: " + std::to_string(total(session)));

    auto smaller = session.sketchParameters(section);
    smaller.elements.values.clear();
    smaller.elements.nextId = 1;
    for (auto& item : square(4))
        smaller.elements.values.push_back(
            {document::SketchElementId{smaller.elements.nextId++}, std::move(item)});
    // The region keeps its ID (same elements), so the loft follows the sketch by itself.
    require(session.editSketchParameters(section, smaller).status() == CommandStatus::committed &&
                near(total(session), 10.0 * (400 + 64 + 160)),
            "The upper square made 8: the loft follows, " + std::to_string(total(session)));
    require(session.undo().status() == CommandStatus::committed, "Undo");

    const auto directory = std::filesystem::temp_directory_path() / "voelund-m24-loft";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const auto file = directory / "loft.volund";
    session.saveDocument(file);
    application::CadSession reopened;
    reopened.loadDocument(file);
    require(reopened.identifiedSnapshot().snapshot().extrusionParameters(feature).loft ==
                    session.identifiedSnapshot().snapshot().extrusionParameters(feature).loft &&
                near(total(reopened), total(session)),
            "Open restores the loft");
    std::filesystem::remove_all(directory);
}

void samePlane() {
    application::CadSession session;
    session.newDocument();
    const auto section = addSketch(session, basePlane(document::DatumPlane::xy), square(5));
    const auto profile = addSketch(session, basePlane(document::DatumPlane::xy), square(10));
    const auto refused = loft(session, profile, section);
    bool localized = false;
    for (const auto& issue : refused.evaluation().features)
        localized = localized || issue.code == "loft-coplanar";
    require(refused.status() == CommandStatus::rejected && localized,
            "Sections in one plane are refused with their reason");
}

} // namespace

int main() {
    try {
        frustum();
        samePlane();
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
    return 0;
}
