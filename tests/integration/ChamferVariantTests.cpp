// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CadSessionTestSupport.hpp"

#include <BRepAdaptor_Curve.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <string>

// N10 (schema 46): the top front edge of a 20 mm cube chamfered with 4 mm on the top and 2 mm
// on the front removes 1/2 * 4 * 2 * 20 = 80 mm³, the material at (10, 3, 19.8) goes; flipped
// it stays. 3 mm on the top at 30 degrees removes 1/2 * 3 * 3 tan 30 * 20. An angle of 90 is
// refused with a reason; Save/Open and undo keep or restore the chamfer.
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
    return test::volumeOf(session.validBodies().front().shape);
}
bool solidAt(const application::CadSession& session, double x, double y, double z) {
    BRepClass3d_SolidClassifier classifier(session.validBodies().front().shape, gp_Pnt(x, y, z),
                                           1e-6);
    return classifier.State() == TopAbs_IN;
}

struct Cube final {
    application::CadSession session;
    document::FeatureId box{1};
    std::optional<document::FilletEdge> edge;
    bool topFirst{false}; // the edge's first face is the top
};
void make(Cube& cube) {
    cube.session.newDocument();
    cube.box = *cube.session.createBoxFeature({20, 20, 20}).feature();
    for (TopExp_Explorer explorer(cube.session.validBodies().front().shape, TopAbs_EDGE);
         explorer.More(); explorer.Next()) {
        const BRepAdaptor_Curve curve(TopoDS::Edge(explorer.Current()));
        const gp_Pnt middle = curve.Value((curve.FirstParameter() + curve.LastParameter()) / 2);
        if (near(middle.Y(), 0) && near(middle.Z(), 20) && near(middle.X(), 10)) {
            cube.edge = cube.session.filletEdge(cube.box, explorer.Current());
            cube.topFirst =
                cube.edge && cube.edge->first.role == kernel::PrimitiveRole::boxPositiveZ;
            return;
        }
    }
    throw std::runtime_error("Top front edge missing");
}
// 4 mm on the top (the first face unless flipped), 2 mm on the front.
document::ChamferShape fourOnTop(const Cube& cube, bool flip) {
    return {document::ChamferMode::twoDistances, 2, cube.topFirst == flip};
}

void twoDistances() {
    Cube cube;
    make(cube);
    auto& session = cube.session;
    require(session.createChamfer(cube.box, std::span(&*cube.edge, 1), 4, fourOnTop(cube, false))
                        .status() == CommandStatus::committed &&
                near(8000 - volume(session), 80) && !solidAt(session, 10, 3, 19.8),
            "4 on the top, 2 on the front: 80 mm³, (10, 3, 19.8) cut away");
    const auto chamfer = session.features().back().id;
    auto flipped = session.chamferParameters(chamfer);
    flipped.shape.flipped = !flipped.shape.flipped;
    require(session.editChamferParameters(chamfer, flipped).status() == CommandStatus::committed &&
                near(8000 - volume(session), 80) && solidAt(session, 10, 3, 19.8),
            "Flipped: 2 on the top, the point stays");
    require(session.undo().status() == CommandStatus::committed && !solidAt(session, 10, 3, 19.8),
            "Undo restores the first side");
    const auto path = test::temporaryFilePath("voelund-n10-chamfer.volund");
    session.saveDocument(path);
    application::CadSession loaded;
    loaded.loadDocument(path);
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    require(loaded.chamferParameters(chamfer) == session.chamferParameters(chamfer) &&
                near(volume(loaded), volume(session)),
            "Save/Open keeps the two distances and the side");
}

void distanceAngle() {
    Cube cube;
    make(cube);
    auto& session = cube.session;
    const document::ChamferShape thirty{document::ChamferMode::distanceAngle, 30, !cube.topFirst};
    require(session.createChamfer(cube.box, std::span(&*cube.edge, 1), 3, thirty).status() ==
                    CommandStatus::committed &&
                near(8000 - volume(session), 0.5 * 3 * 3 * std::tan(pi / 6) * 20),
            "3 mm at 30 degrees: 1/2 * 3 * 3 tan 30 * 20");
    const document::ChamferShape right{document::ChamferMode::distanceAngle, 90, false};
    const auto refused = session.createChamfer(cube.box, std::span(&*cube.edge, 1), 3, right);
    require(refused.status() != CommandStatus::committed &&
                refused.diagnostic().find("between 0 and 90") != std::string::npos,
            "90 degrees is refused with its reason: " + refused.diagnostic());
}
} // namespace

int main() {
    try {
        twoDistances();
        distanceAngle();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "PASS N10 chamfer with two distances and with distance and angle\n";
    return 0;
}
