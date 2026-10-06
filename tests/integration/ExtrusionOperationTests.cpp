#include "CadSessionTestSupport.hpp"

#include <voelund/features/SketchRegionIdentity.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>

// T02: the result of an extrusion. A pocket and a through cut from the plate's top face, a
// rib as its own body, join and cut of base-plane sketches into a named body; a join that
// does not touch and an unknown body are rejected without change; save/open keeps it all.
namespace {
using namespace voelund;
using application::CommandStatus;
using application::ExtrusionMode;
using document::ExtrusionOperation;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-6 * std::max(1.0, std::abs(b)); }
document::FeatureId committed(const application::CommandResult& result) {
    for (const auto& issue : result.evaluation().features)
        if (!issue.message.empty())
            std::cerr << "feature " << issue.feature.value() << ": " << issue.message
                      << std::endl;
    require(result.status() == CommandStatus::committed && result.feature().has_value(),
            "Command was not committed (status " +
                std::to_string(static_cast<int>(result.status())) + ", reason " +
                std::to_string(static_cast<int>(result.reason())) + "): " + result.diagnostic());
    return *result.feature();
}
void applied(const application::CommandResult& result) {
    require(result.status() == CommandStatus::committed, "Edit was not applied: " + result.diagnostic());
}
document::SketchParameters onXy(double x0, double y0, double x1, double y1) {
    document::SketchParameters sketch{
        document::datumSketchPlane(document::DatumPlane::xy),
        {{{document::SketchElementId{1}, document::SketchRectangle{x0, y0, x1, y1}}}, 2}};
    sketch.datum = document::DatumPlane::xy;
    return sketch;
}
std::vector<document::SketchRegionId> regionsOf(const document::SketchParameters& sketch) {
    return features::identifySketchRegions(sketch.elements.values).ids;
}
double volume(const application::CadSession& session) {
    return test::volumeOf(session.currentShape());
}

// 80 x 40 x 10 around the origin: 32 000 mm³. Returns the plate's extrusion.
struct Plate final {
    document::FeatureId sketch;
    document::FeatureId extrusion;
};
Plate plate(application::CadSession& session) {
    session.newDocument();
    const auto extrusion =
        committed(session.createDatumSketchExtrusion(onXy(-40, -20, 40, 20), {}, 10));
    return {session.features().front().id, extrusion};
}
// A 20 x 10 rectangle centered at (cx, cy) on the plate's top face, in the face's frame.
document::FeatureId topSketch(application::CadSession& session, document::FeatureId support,
                              double cx, double cy) {
    const auto faces = session.supportFaces(support);
    const auto top = std::ranges::find_if(faces, [](const auto& face) {
        return near(face.plane.normalZ, 1.0) && near(face.plane.originZ, 10.0);
    });
    require(top != faces.end(), "The plate offers its top face");
    auto sketch = session.sketchOnFace(support, top->origin);
    const auto local = [&](double x, double y) {
        const double dx = x - sketch.plane.originX;
        const double dy = y - sketch.plane.originY;
        const double yX = sketch.plane.normalY * sketch.plane.xDirectionZ -
                          sketch.plane.normalZ * sketch.plane.xDirectionY;
        const double yY = sketch.plane.normalZ * sketch.plane.xDirectionX -
                          sketch.plane.normalX * sketch.plane.xDirectionZ;
        return std::pair{dx * sketch.plane.xDirectionX + dy * sketch.plane.xDirectionY,
                         dx * yX + dy * yY};
    };
    const auto [x0, y0] = local(cx - 10, cy - 5);
    const auto [x1, y1] = local(cx + 10, cy + 5);
    sketch.elements = {{{document::SketchElementId{1},
                         document::SketchRectangle{std::min(x0, x1), std::min(y0, y1),
                                                   std::max(x0, x1), std::max(y0, y1)}}},
                       2};
    return committed(session.createSketchFeature(support, sketch));
}
application::CommandResult extrude(application::CadSession& session, document::FeatureId sketch,
                                   double distance, ExtrusionMode mode) {
    return session.createSketchProfileExtrusion(
        sketch, regionsOf(session.sketchParameters(sketch)), distance, mode);
}

void faceOperations() {
    application::CadSession session;
    auto base = plate(session);
    auto sketch = topSketch(session, base.extrusion, 0, 10);
    committed(extrude(session, sketch, 4, {ExtrusionOperation::cut, true}));
    require(session.validBodies().size() == 1 && near(volume(session), 32000 - 800) &&
                near(test::maximumZOf(session.currentShape()), 10),
            "A reversed cut from the top face is a 4 mm pocket in the plate");
    committed(session.undo());
    require(near(volume(session), 32000), "Undo removes the pocket");

    committed(extrude(session, sketch, 10, {ExtrusionOperation::cut, true}));
    require(session.validBodies().size() == 1 && near(volume(session), 32000 - 2000),
            "A cut through the plate");

    base = plate(session);
    sketch = topSketch(session, base.extrusion, 0, 10);
    committed(extrude(session, sketch, 8, {ExtrusionOperation::newBody, false}));
    require(session.validBodies().size() == 2 && session.modelOutline().bodies.size() == 2 &&
                near(test::maximumZOf(session.currentShape()), 18),
            "A new body on the face stands on its own");

    // An unset operation keeps the earlier rule: a face sketch joins its body.
    base = plate(session);
    sketch = topSketch(session, base.extrusion, 0, 10);
    committed(extrude(session, sketch, 8, {}));
    require(session.validBodies().size() == 1 && near(volume(session), 32000 + 1600),
            "The default joins the face's body");
}

// T03: a start offset shifts the prism; "up to face" ends on a parallel face and follows it.
void offsetAndEndFace() {
    application::CadSession session;
    auto base = plate(session);
    auto sketch = topSketch(session, base.extrusion, 0, 10);
    committed(extrude(session, sketch, 8, {ExtrusionOperation::newBody, false, std::nullopt, 5}));
    require(session.validBodies().size() == 2 && near(test::maximumZOf(session.currentShape()), 23),
            "A rib with offset 5 floats from z = 15 to 23");

    // A column from the plate's top face up to the underside of a box placed above.
    base = plate(session);
    const auto box = committed(session.createBoxFeature());
    auto placed = session.identifiedSnapshot().snapshot().boxParameters(box);
    placed.dimensions = {20, 20, 5};
    placed.placement.translationZ = 30;
    applied(session.editBoxParameters(box, placed));
    const auto faces = session.supportFaces(box);
    const auto underside = std::ranges::find_if(faces, [](const auto& face) {
        return face.origin.role == kernel::PrimitiveRole::boxNegativeZ;
    });
    require(underside != faces.end(), "The box offers its underside");
    const auto end = session.faceReference(box, underside->origin);
    sketch = topSketch(session, base.extrusion, 0, 10);
    committed(extrude(session, sketch, 1, {ExtrusionOperation::join, false, std::nullopt, 0, end}));
    const double column = 20 * 10 * 20;
    require(session.validBodies().size() == 2 && near(volume(session), 32000 + column + 2000),
            "The column joins the plate and ends on the box: " + std::to_string(volume(session)));
    placed.placement.translationZ = 40;
    applied(session.editBoxParameters(box, placed));
    require(near(volume(session), 32000 + 1.5 * column + 2000), "The column follows the box");
    placed.placement.translationZ = 5;
    const auto behind = session.editBoxParameters(box, placed);
    require(behind.status() == CommandStatus::rejected && near(volume(session), 32000 + 1.5 * column + 2000),
            "A box below the start is rejected: " + behind.diagnostic());
}

void namedTargets(const std::filesystem::path& work) {
    application::CadSession session;
    const auto base = plate(session);
    const document::BodyId plateBody{base.sketch.value()};
    // Base-plane sketches join or cut the plate's body by name.
    committed(session.createDatumSketchExtrusion(onXy(30, -5, 60, 5), {}, 10,
                                                 {ExtrusionOperation::join, false, plateBody}));
    require(session.validBodies().size() == 1 && session.modelOutline().bodies.size() == 1 &&
                near(volume(session), 32000 + 2000),
            "A base-plane join extends the plate and its sketch belongs to the plate");
    committed(session.createDatumSketchExtrusion(onXy(-25, -5, -15, 5), {}, 10,
                                                 {ExtrusionOperation::cut, false, plateBody}));
    require(session.validBodies().size() == 1 && near(volume(session), 34000 - 1000),
            "A base-plane cut removes a block from the plate");

    const auto features = session.features().size();
    const auto before = volume(session);
    auto result = session.createDatumSketchExtrusion(onXy(100, 0, 110, 10), {}, 10,
                                                     {ExtrusionOperation::join, false, plateBody});
    require(result.status() == CommandStatus::rejected && session.features().size() == features &&
                near(volume(session), before),
            "A join that does not touch the body is rejected without change: " +
                result.diagnostic());
    result = session.createDatumSketchExtrusion(onXy(0, 0, 5, 5), {}, 10,
                                                {ExtrusionOperation::cut, false,
                                                 document::BodyId{999}});
    require(result.status() == CommandStatus::rejected && session.features().size() == features,
            "An unknown body is rejected");
    result = session.createDatumSketchExtrusion(onXy(0, 0, 5, 5), {}, 10,
                                                {ExtrusionOperation::cut, false, std::nullopt});
    require(result.status() == CommandStatus::rejected && session.features().size() == features,
            "A base-plane cut without a body is rejected");

    const auto path = work / "t02.volund";
    const auto snapshot = session.identifiedSnapshot().snapshot();
    session.saveDocument(path);
    session.newDocument();
    session.loadDocument(path);
    require(std::ranges::equal(session.identifiedSnapshot().snapshot().parameters(),
                               snapshot.parameters()) &&
                near(volume(session), 33000),
            "Save and open keep operations and targets");
}
} // namespace

int main() {
    try {
        const auto work = std::filesystem::temp_directory_path() / "voelund-t02";
        std::filesystem::create_directories(work);
        faceOperations();
        offsetAndEndFace();
        namedTargets(work);
        std::filesystem::remove_all(work);
        std::cout << "PASS T02/T03 extrusion operations\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
