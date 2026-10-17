#include "../TestPaths.hpp"
#include "CadSessionTestSupport.hpp"

#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>

// M02 (A-EXT-01, user decisions 2026-10-11): without any existing body, a 20 x 10 rectangle
// extruded 8 mm is 1600 mm^3 on XY, XZ and YZ; blind runs [0, 8] along the plane normal,
// reversed [-8, 0], symmetric (the distance is the total length) [-4, 4]. The ring R20/r10
// x 8 is 2400 pi. An open profile, distance 0 or NaN and a lost region are refused, as is a
// symmetric extrusion with a direction or offset. Intersect keeps the common volume and
// refuses a disjoint tool; symmetric and intersect survive Save/Open.
namespace {
using namespace voelund;
using application::CommandStatus;
using application::ExtrusionMode;
using document::DatumPlane;
using document::FeatureId;
using document::SketchElementId;
constexpr double pi = std::numbers::pi;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-6 * std::max(1.0, std::abs(b)); }
document::SketchParameters onDatum(DatumPlane datum, std::vector<document::SketchElement> e) {
    std::uint64_t next = 1;
    for (const auto& element : e)
        next = std::max(next, element.id.value() + 1);
    document::SketchParameters sketch{document::datumSketchPlane(datum),
                                      document::SketchElements{std::move(e), next}};
    sketch.datum = datum;
    return sketch;
}
document::SketchElement rectangle() {
    return {SketchElementId{1}, document::SketchRectangle{0, 0, 20, 10}};
}
FeatureId sketchOn(application::CadSession& session, DatumPlane datum,
                   std::vector<document::SketchElement> elements) {
    const auto result = session.createDatumSketchFeature(onDatum(datum, std::move(elements)));
    require(result.status() == CommandStatus::committed, "Sketch: " + result.diagnostic());
    return *result.feature();
}
std::vector<document::SketchRegionId> regionOf(application::CadSession& session, FeatureId sketch,
                                               SketchElementId element) {
    require(session.selectFeature(sketch) && session.selectSketchProfileByElement(element, false),
            "Select a region");
    return {session.selectedSketchProfiles().begin(), session.selectedSketchProfiles().end()};
}
// [min, max] of the body along the axis 0 = x, 1 = y, 2 = z.
std::pair<double, double> range(const TopoDS_Shape& shape, int axis) {
    Bnd_Box box;
    BRepBndLib::Add(shape, box);
    box.SetGap(0);
    double low[3]{}, high[3]{};
    box.Get(low[0], low[1], low[2], high[0], high[1], high[2]);
    return {low[axis], high[axis]};
}

struct Plane {
    DatumPlane datum;
    int axis;    // the axis of the plane normal
    double sign; // +1 when the normal points along +axis
};

void directions() {
    for (const Plane plane : {Plane{DatumPlane::xy, 2, 1.0}, Plane{DatumPlane::xz, 1, -1.0},
                              Plane{DatumPlane::yz, 0, 1.0}}) {
        struct Case {
            const char* name;
            ExtrusionMode mode;
            double from, to; // along the normal
        };
        ExtrusionMode reversed;
        reversed.reversed = true;
        ExtrusionMode symmetric;
        symmetric.symmetric = true;
        for (const Case& c : {Case{"blind", {}, 0, 8}, Case{"reversed", reversed, -8, 0},
                              Case{"symmetric", symmetric, -4, 4}}) {
            application::CadSession session;
            session.newDocument();
            const auto sketch = sketchOn(session, plane.datum, {rectangle()});
            const auto regions = regionOf(session, sketch, SketchElementId{1});
            const auto result = session.createSketchProfileExtrusion(sketch, regions, 8, c.mode);
            require(result.status() == CommandStatus::committed,
                    std::string(c.name) + ": " + result.diagnostic());
            const auto& body = session.validBodies().front().shape;
            const auto [low, high] = range(body, plane.axis);
            const double from = plane.sign > 0 ? c.from : -c.to;
            const double to = plane.sign > 0 ? c.to : -c.from;
            require(session.validBodies().size() == 1 && near(test::volumeOf(body), 1600) &&
                        near(low, from) && near(high, to),
                    std::string(c.name) + " on plane " + std::to_string(plane.axis) + ": range " +
                        std::to_string(low) + ".." + std::to_string(high));
        }
    }
}

void ringAndRefusals() {
    application::CadSession session;
    session.newDocument();
    const auto ring = sketchOn(session, DatumPlane::xy,
                               {{SketchElementId{1}, document::SketchCircle{{0, 0}, 20}},
                                {SketchElementId{2}, document::SketchCircle{{0, 0}, 10}}});
    const auto ringRegion = regionOf(session, ring, SketchElementId{1});
    require(session.createSketchProfileExtrusion(ring, ringRegion, 8).status() ==
                    CommandStatus::committed &&
                near(test::volumeOf(session.validBodies().front().shape), 2400 * pi),
            "Ring R20/r10 x 8 = 2400 pi");

    const auto before = session.features().size();
    for (const double distance : {0.0, std::numeric_limits<double>::quiet_NaN()})
        require(session.createSketchProfileExtrusion(ring, ringRegion, distance).status() ==
                    CommandStatus::rejected,
                "Distance 0 or NaN refused");
    const auto open = sketchOn(session, DatumPlane::xy,
                               {{SketchElementId{1}, document::SketchLine{{50, 0}, {60, 0}}}});
    require(session.createSketchProfileExtrusion(open, {}, 8).status() == CommandStatus::rejected,
            "An open profile is refused");
    const auto other = sketchOn(session, DatumPlane::xz, {rectangle()});
    require(session.createSketchProfileExtrusion(other, ringRegion, 8).status() ==
                CommandStatus::rejected,
            "A region the sketch does not have is refused");
    ExtrusionMode bad;
    bad.symmetric = true;
    bad.offset = 2;
    const auto rect = sketchOn(session, DatumPlane::yz, {rectangle()});
    const auto rectRegion = regionOf(session, rect, SketchElementId{1});
    require(session.createSketchProfileExtrusion(rect, rectRegion, 8, bad).status() ==
                CommandStatus::rejected,
            "Symmetric with an offset is refused");
    bad.offset = 0;
    bad.reversed = true;
    require(session.createSketchProfileExtrusion(rect, rectRegion, 8, bad).status() ==
                CommandStatus::rejected,
            "Symmetric with a direction is refused");
    require(session.features().size() == before + 3,
            "Refusals added no extrusion (three sketches)");
}

void intersectAndRoundTrip() {
    application::CadSession session;
    session.newDocument();
    // Body A: 20 x 10 x 8 on XY.
    const auto a = sketchOn(session, DatumPlane::xy, {rectangle()});
    require(session.createSketchProfileExtrusion(a, regionOf(session, a, SketchElementId{1}), 8)
                    .status() == CommandStatus::committed,
            "Body A");
    const document::BodyId bodyA{a.value()};
    // A symmetric 10 x 10 x 20 tool on XY at x 15..25 intersects A: 5 x 10 x 8 = 400.
    const auto tool = sketchOn(session, DatumPlane::xy,
                               {{SketchElementId{1}, document::SketchRectangle{15, 0, 25, 10}}});
    ExtrusionMode intersect;
    intersect.operation = document::ExtrusionOperation::intersect;
    intersect.target = bodyA;
    intersect.symmetric = true;
    const auto result = session.createSketchProfileExtrusion(
        tool, regionOf(session, tool, SketchElementId{1}), 20, intersect);
    require(result.status() == CommandStatus::committed &&
                near(test::volumeOf(session.validBodies().front().shape), 400),
            "Intersect keeps the common 400 mm^3: " + result.diagnostic());
    // A disjoint tool is refused; the document stays as it was.
    const auto far = sketchOn(session, DatumPlane::xy,
                              {{SketchElementId{1}, document::SketchRectangle{60, 0, 70, 10}}});
    const auto count = session.features().size();
    require(session.createSketchProfileExtrusion(far, regionOf(session, far, SketchElementId{1}), 8,
                                                 intersect)
                        .status() == CommandStatus::rejected &&
                session.features().size() == count &&
                near(test::volumeOf(session.validBodies().front().shape), 400),
            "A disjoint intersect is refused and A stays");

    const auto path = test::temporaryFilePath("voelund-m02-extrusion.volund");
    session.saveDocument(path);
    application::CadSession loaded;
    loaded.loadDocument(path);
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    const auto saved =
        loaded.identifiedSnapshot().snapshot().extrusionParameters(*result.feature());
    require(saved.symmetric && saved.operation == document::ExtrusionOperation::intersect &&
                near(test::volumeOf(loaded.validBodies().front().shape), 400),
            "Symmetric intersect survives Save/Open");
}
} // namespace

int main() {
    try {
        directions();
        ringAndRefusals();
        intersectAndRoundTrip();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "PASS M02 extrusion directions (A-EXT-01), symmetric, intersect\n";
    return 0;
}
