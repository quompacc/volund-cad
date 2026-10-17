#include "CadSessionTestSupport.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/features/SketchRegionIdentity.hpp>

#include <cmath>
#include <filesystem>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>

// M23 (user decision 2026-10-14): a sweep ("Folgen"). A circle of diameter 6 on XY swept
// along a straight path of 50 on XZ has the volume of its cylinder; the path made longer, the
// body follows; along an L path with a quarter arc it is valid; as a cut it makes a groove in a
// box; a disconnected path is a localized error; undo, save and open.
namespace {
using namespace voelund;
using application::CommandStatus;
constexpr double pi = std::numbers::pi;

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

document::FeatureId datumSketch(application::CadSession& session, document::DatumPlane plane,
                                std::vector<document::SketchGeometry> geometry) {
    document::SketchParameters sketch{document::datumSketchPlane(plane), {}};
    sketch.datum = plane;
    for (auto& item : geometry)
        sketch.elements.values.push_back(
            {document::SketchElementId{sketch.elements.nextId++}, std::move(item)});
    require(session.createDatumSketchFeature(sketch).status() == CommandStatus::committed,
            "Sketch");
    return session.features().back().id;
}

document::FeatureId sweep(application::CadSession& session, document::FeatureId profile,
                          document::FeatureId path, std::size_t elements,
                          std::optional<document::ExtrusionOperation> operation = std::nullopt,
                          std::optional<document::BodyId> target = std::nullopt) {
    const auto regions =
        features::identifySketchRegions(session.sketchParameters(profile).elements.values).ids;
    application::ExtrusionMode mode;
    mode.operation = operation;
    mode.target = target;
    mode.sweep = document::SweepPath{path, {}};
    for (std::uint64_t id = 1; id <= elements; ++id)
        mode.sweep->elements.push_back(document::SketchElementId{id});
    const auto created = session.createSketchProfileExtrusion(profile, regions, 1, mode);
    if (created.status() != CommandStatus::committed) {
        std::cerr << created.diagnostic() << std::endl;
        for (const auto& issue : created.evaluation().features)
            std::cerr << issue.feature.value() << ' ' << issue.code << ' ' << issue.message
                      << std::endl;
    }
    require(created.status() == CommandStatus::committed, "Sweep created");
    return session.features().back().id;
}

void straight() {
    application::CadSession session;
    session.newDocument();
    const auto profile =
        datumSketch(session, document::DatumPlane::xy, {document::SketchCircle{{0, 0}, 3}});
    const auto path =
        datumSketch(session, document::DatumPlane::xz, {document::SketchLine{{0, 0}, {0, 50}}});
    const auto feature = sweep(session, profile, path, 1);
    require(session.features().back().name == "Folgen 1", "Named Folgen 1");
    require(near(total(session), pi * 9 * 50),
            "A cylinder of 50: " + std::to_string(total(session)));

    auto longer = session.sketchParameters(path);
    std::get<document::SketchLine>(longer.elements.values.front().geometry).end = {0, 80};
    require(session.editSketchParameters(path, longer).status() == CommandStatus::committed &&
                near(total(session), pi * 9 * 80),
            "The body follows a longer path");
    require(session.undo().status() == CommandStatus::committed &&
                near(total(session), pi * 9 * 50),
            "Undo");

    const auto directory = std::filesystem::temp_directory_path() / "voelund-m23-sweep";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const auto file = directory / "sweep.volund";
    session.saveDocument(file);
    application::CadSession reopened;
    reopened.loadDocument(file);
    require(reopened.identifiedSnapshot().snapshot().extrusionParameters(feature).sweep ==
                    session.identifiedSnapshot().snapshot().extrusionParameters(feature).sweep &&
                near(total(reopened), pi * 9 * 50),
            "Open restores the sweep");
    std::filesystem::remove_all(directory);
}

void lPath() {
    application::CadSession session;
    session.newDocument();
    const auto profile =
        datumSketch(session, document::DatumPlane::xy, {document::SketchCircle{{0, 0}, 3}});
    const auto path = datumSketch(session, document::DatumPlane::xz,
                                  {document::SketchLine{{0, 0}, {0, 30}},
                                   document::SketchArc{{10, 30}, 10, pi / 2, pi},
                                   document::SketchLine{{10, 40}, {40, 40}}});
    static_cast<void>(sweep(session, profile, path, 3));
    // Pappus: the tube along a centre line of 30 + quarter circle + 30.
    const double centre = 30 + pi * 10 / 2 + 30;
    require(near(total(session), pi * 9 * centre),
            "Along an L path with an arc: " + std::to_string(total(session)));
}

void groove() {
    application::CadSession session;
    session.newDocument();
    const auto box = *session.createBoxFeature({40, 20, 10}).feature();
    // A circle of radius 3 on YZ at the box top, swept along x through the box.
    const auto profile =
        datumSketch(session, document::DatumPlane::yz, {document::SketchCircle{{10, 10}, 3}});
    const auto path =
        datumSketch(session, document::DatumPlane::xy, {document::SketchLine{{0, 10}, {40, 10}}});
    const auto before = total(session);
    static_cast<void>(sweep(session, profile, path, 1, document::ExtrusionOperation::cut,
                            document::BodyId{box.value()}));
    require(near(total(session), before - pi * 9 / 2 * 40), "A half-round groove along the top");

    // A path of two lines that do not touch is a localized error.
    auto broken = session.sketchParameters(path);
    broken.elements.values.push_back({document::SketchElementId{broken.elements.nextId++},
                                      document::SketchLine{{50, 10}, {60, 10}}});
    auto parameters =
        session.identifiedSnapshot().snapshot().extrusionParameters(session.features().back().id);
    parameters.sweep->elements.push_back(document::SketchElementId{2});
    require(session.editSketchParameters(path, broken).status() == CommandStatus::committed,
            "A second, separate line");
    const auto changed = session.editExtrusionParameters(session.features().back().id, parameters);
    bool localized = false;
    for (const auto& issue : changed.evaluation().features)
        localized = localized || issue.code == "sweep-path-disconnected";
    require(changed.status() == CommandStatus::rejected && localized,
            "A disconnected path is refused with its reason");
}

} // namespace

int main() {
    try {
        straight();
        lPath();
        groove();
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
    return 0;
}
