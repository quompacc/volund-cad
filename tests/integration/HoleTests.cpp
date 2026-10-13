#include "CadSessionTestSupport.hpp"

#include <voelund/application/Actions.hpp>

#include <BRepCheck_Analyzer.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>

// M08 (A-HOLE-01, user decision 2026-10-13): a hole into a planar face at (x, y) of the
// face's frame. Box 40 x 30 x 10, diameter 6 through all removes 90 pi, blind 4 removes 36 pi.
// The box grows to 20 thick: through all stays through (180 pi), blind stays 4 deep. The box
// moves: the hole moves with its face. Save/Open keeps the hole (schema 16).
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
// Material at this point of the body.
bool solidAt(const application::CadSession& session, double x, double y, double z) {
    BRepClass3d_SolidClassifier classifier(session.validBodies().front().shape, gp_Pnt(x, y, z),
                                           1e-6);
    return classifier.State() == TopAbs_IN;
}

struct Top final {
    document::SubshapeOrigin origin;
    double x, y;          // the face centre in the face's frame
    double cx, cy;        // the face centre in model coordinates
};
Top top(const application::CadSession& session, document::FeatureId support) {
    for (const auto& face : session.supportFaces(support))
        if (face.plane.normalZ > 0.5) {
            const double fx = (face.minX + face.maxX) / 2, fy = (face.minY + face.maxY) / 2;
            const double yx = face.plane.normalY * face.plane.xDirectionZ -
                              face.plane.normalZ * face.plane.xDirectionY;
            const double yy = face.plane.normalZ * face.plane.xDirectionX -
                              face.plane.normalX * face.plane.xDirectionZ;
            return {face.origin, fx, fy,
                    face.plane.originX + fx * face.plane.xDirectionX + fy * yx,
                    face.plane.originY + fx * face.plane.xDirectionY + fy * yy};
        }
    throw std::runtime_error("Top face missing");
}

void throughAndBlind() {
    application::CadSession session;
    session.newDocument();
    const auto created = session.createBoxFeature({40, 30, 10});
    require(created.status() == CommandStatus::committed, "Box");
    const auto box = *created.feature();
    const auto face = top(session, box);

    const auto through = application::executeAction(
        session, application::prepareRequest(
                     session, application::CreateHoleAction{
                                  box, face.origin, {face.x, application::QuantityUnit::millimetre},
                                  {face.y, application::QuantityUnit::millimetre},
                                  {6, application::QuantityUnit::millimetre}, std::nullopt}));
    require(through.command.status() == CommandStatus::committed &&
                session.features().back().name == "Bohrung 1" &&
                near(volume(session), 12000 - 90 * pi) && valid(session),
            "Diameter 6 through all removes 90 pi");
    const auto hole = session.features().back().id;
    require(!solidAt(session, face.cx, face.cy, 1) && solidAt(session, face.cx + 5, face.cy, 1),
            "The hole sits at the face centre and goes through to the bottom");

    auto parameters = session.holeParameters(hole);
    parameters.depth = 4;
    require(session.editHoleParameters(hole, parameters).status() == CommandStatus::committed &&
                near(volume(session), 12000 - 36 * pi) && solidAt(session, face.cx, face.cy, 5) &&
                !solidAt(session, face.cx, face.cy, 7),
            "Blind 4 removes 36 pi and ends 4 below the top face");

    // The box grows to 20: blind stays 4 deep, through all goes through again.
    auto thicker = session.identifiedSnapshot().snapshot().boxParameters(box);
    thicker.dimensions.height = 20;
    require(session.editBoxParameters(box, thicker).status() == CommandStatus::committed &&
                near(volume(session), 24000 - 36 * pi) && !solidAt(session, face.cx, face.cy, 17) &&
                solidAt(session, face.cx, face.cy, 15),
            "Blind stays 4 deep below the raised top face");
    parameters.depth.reset();
    require(session.editHoleParameters(hole, parameters).status() == CommandStatus::committed &&
                near(volume(session), 24000 - 180 * pi) && !solidAt(session, face.cx, face.cy, 1),
            "Through all stays through the thicker box");

    // The box moves 10 along x: the hole moves with its face.
    auto moved = session.identifiedSnapshot().snapshot().boxParameters(box);
    moved.placement.translationX += 10;
    require(session.editBoxParameters(box, moved).status() == CommandStatus::committed &&
                near(volume(session), 24000 - 180 * pi) &&
                !solidAt(session, face.cx + 10, face.cy, 10) &&
                solidAt(session, face.cx, face.cy, 10),
            "The hole follows its face");

    // A diameter that leaves no material at the face is refused; the hole stays.
    parameters.diameter = 100;
    require(session.editHoleParameters(hole, parameters).status() == CommandStatus::rejected &&
                near(volume(session), 24000 - 180 * pi),
            "A hole that would remove the whole body is refused");

    require(session.undo().status() == CommandStatus::committed &&
                near(volume(session), 24000 - 180 * pi) && solidAt(session, face.cx + 10, face.cy, 10),
            "Undo moves the box back");

    const auto directory = std::filesystem::temp_directory_path() / "voelund-m08-hole";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const auto path = directory / "hole.volund";
    session.saveDocument(path);
    application::CadSession reopened;
    reopened.loadDocument(path);
    require(reopened.holeParameters(hole) == session.holeParameters(hole) &&
                near(volume(reopened), volume(session)),
            "Open restores the hole");
    std::filesystem::remove_all(directory);
}

} // namespace

int main() {
    try {
        throughAndBlind();
        std::cout << "PASS M08 hole: through 90 pi, blind 36 pi, thicker box, follows its face, "
                     "refused, undo, save/open\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
