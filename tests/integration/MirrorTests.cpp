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

// M09 (user decision 2026-10-15): an asymmetric part, a box 40 x 30 x 10 at the origin with a
// hole r 5 at x = 30, is mirrored, not moved: about the YZ plane its image has the hole at
// x = -30. The hole radius drives the image; joined about the box's +X face the part is one
// symmetric body; a join about a plane apart from the body is refused at the mirror; undo; a
// fillet on an edge of the image (faces named as copies) survives save/open (schema 31).
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
// Whether some body holds the point.
bool solidAt(const application::CadSession& session, double x, double y, double z) {
    for (const auto& body : session.validBodies())
        if (BRepClass3d_SolidClassifier(body.shape, gp_Pnt(x, y, z), 1e-7).State() == TopAbs_IN)
            return true;
    return false;
}
document::SubshapeOrigin face(const application::CadSession& session, document::FeatureId support,
                              double nx, double ny, double nz) {
    for (const auto& candidate : session.supportFaces(support))
        if (candidate.plane.normalX * nx + candidate.plane.normalY * ny +
                candidate.plane.normalZ * nz >
            0.99)
            return candidate.origin;
    throw std::runtime_error("Face missing");
}

void mirrorPart() {
    application::CadSession session;
    session.newDocument();
    const auto box = *session.createBoxFeature({40, 30, 10}).feature();
    const auto cylinder = *session.createNewBodyCylinder().feature();
    auto hole = session.cylinderParameters(cylinder);
    hole.radius = 5;
    hole.height = 20;
    hole.placement.translationX = 30;
    hole.placement.translationY = 15;
    hole.placement.translationZ = -5;
    require(session.editCylinderParameters(cylinder, hole).status() == CommandStatus::committed,
            "Hole cylinder");
    require(application::executeAction(
                session, application::prepareRequest(
                             session, application::CreateCombineAction{
                                          box, {cylinder}, document::CombineOperation::cut,
                                          false}))
                    .command.status() == CommandStatus::committed,
            "Box with a hole at x = 30");
    const auto part = session.features().back().id;
    const double single = 12000 - 250 * pi;
    const auto outer = face(session, part, 1, 0, 0);
    // A construction plane apart from the part, earlier than the mirror that uses it.
    require(session.createDatumPlane(document::DatumPlane::yz, std::nullopt, std::nullopt, -10,
                                     document::DatumRotationAxis::none, 0)
                    .status() == CommandStatus::committed,
            "Construction plane at x = -10");
    const auto apart = session.features().back().id;

    const auto created = application::executeAction(
        session, application::prepareRequest(
                     session, application::CreateMirrorAction{
                                  part, document::DatumPlane::yz, std::nullopt, std::nullopt,
                                  std::nullopt, document::MirrorMode::newBody}));
    require(created.command.status() == CommandStatus::committed &&
                session.features().back().name == "Spiegeln 1" &&
                session.validBodies().size() == 2 && near(volume(session), 2 * single) &&
                valid(session),
            "A new body mirrored about YZ");
    const auto mirror = session.features().back().id;
    require(!solidAt(session, -30, 15, 5) && solidAt(session, -10, 15, 5) &&
                !solidAt(session, 30, 15, 5) && solidAt(session, 10, 15, 5),
            "The image has its hole at x = -30: mirrored, not moved");

    hole.radius = 4;
    require(session.editCylinderParameters(cylinder, hole).status() == CommandStatus::committed &&
                near(volume(session), 2 * (12000 - 160 * pi)),
            "The hole radius drives the image as well");
    hole.radius = 5;
    require(session.editCylinderParameters(cylinder, hole).status() == CommandStatus::committed,
            "Hole radius 5 again");

    auto joined = session.mirrorParameters(mirror);
    joined.plane = {std::nullopt, std::nullopt, document::BodyFaceReference{joined.body, outer}};
    joined.mode = document::MirrorMode::join;
    require(session.editMirrorParameters(mirror, joined).status() == CommandStatus::committed &&
                session.validBodies().size() == 1 && near(volume(session), 2 * single) &&
                valid(session) && !solidAt(session, 50, 15, 5) && solidAt(session, 70, 15, 5),
            "Joined about the +X face: one symmetric body with its holes at 30 and 50");

    auto disjoint = joined;
    disjoint.plane = {std::nullopt, apart, std::nullopt};
    const auto refused = session.editMirrorParameters(mirror, disjoint);
    const auto issue = std::ranges::find(refused.evaluation().features, mirror,
                                         &features::EvaluationIssue::feature);
    require(refused.status() == CommandStatus::rejected &&
                issue != refused.evaluation().features.end() && issue->code == "join-disjoint" &&
                near(volume(session), 2 * single),
            "A join with an image apart from the body is refused at the mirror");

    disjoint.mode = document::MirrorMode::newBody;
    require(session.editMirrorParameters(mirror, disjoint).status() == CommandStatus::committed &&
                session.validBodies().size() == 2 && solidAt(session, -30, 15, 5) &&
                !solidAt(session, -50, 15, 5),
            "As a new body the image stands apart, its hole at x = -50");
    require(session.undo().status() == CommandStatus::committed &&
                session.validBodies().size() == 1 && near(volume(session), 2 * single),
            "Undo returns to the joined part");

    // An edge of the image: its top and its end at x = 80 are copies of the box's faces.
    const auto copy = [&](kernel::PrimitiveRole role) {
        document::SubshapeOrigin origin{box, role};
        origin.instances.push_back({mirror, 1});
        return origin;
    };
    const document::FilletEdge edge{copy(kernel::PrimitiveRole::boxPositiveZ),
                                    copy(kernel::PrimitiveRole::boxNegativeX), 1};
    require(session.createFillet(mirror, std::span{&edge, 1}, 2).status() ==
                    CommandStatus::committed &&
                near(volume(session), 2 * single - 30 * 4 * (1 - pi / 4)) && valid(session),
            "A fillet on the image's outer top edge");

    const auto directory = std::filesystem::temp_directory_path() / "voelund-m09-mirror";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const auto path = directory / "mirror.volund";
    session.saveDocument(path);
    application::CadSession reopened;
    reopened.loadDocument(path);
    require(reopened.mirrorParameters(mirror) == session.mirrorParameters(mirror) &&
                reopened.validBodies().size() == 1 && near(volume(reopened), volume(session)),
            "Open restores the mirror and the fillet on its copied faces");
    std::filesystem::remove_all(directory);
}

} // namespace

int main() {
    try {
        mirrorPart();
        std::cout << "PASS M09 mirror: new body about YZ, hole drives the image, joined about a "
                     "face, disjoint join refused, undo, fillet on the image, save/open\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
