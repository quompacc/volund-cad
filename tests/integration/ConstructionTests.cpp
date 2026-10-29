// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CadSessionTestSupport.hpp"

#include <voelund/application/Actions.hpp>

#include <BRepAdaptor_Curve.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

// N13b (schema 53, packet acceptance): a box 20 x 20 x 10. The midplane between its bottom and
// top lies exactly at z = 5 and follows the box when it grows to 16 high (z = 8); top and front
// are not parallel and are refused. Construction points at (0; 0; 0), (10; 0; 0) and (0; 10; 0)
// span a plane through the origin with the Z normal; three points on a line are refused. A point
// in the middle of the top front edge lies at (10; 0; 10). Save/Open keeps them all.
namespace {
using namespace voelund;
using application::CommandStatus;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-9 * std::max(1.0, std::abs(b)); }

document::SubshapeOrigin faceAlong(const application::CadSession& session,
                                   document::FeatureId support, double nz, double ny) {
    for (const auto& face : session.supportFaces(support))
        if (face.plane.normalZ * nz + face.plane.normalY * ny > 0.99)
            return face.origin;
    throw std::runtime_error("Face missing");
}
std::optional<document::FeatureId> point(application::CadSession& session, double x, double y,
                                         double z) {
    return session
        .createDatumPoint(application::CreateDatumPointAction{std::array{x, y, z}, {}, {}})
        .feature();
}
std::optional<document::FeatureId> planeThrough(application::CadSession& session,
                                                std::vector<document::FeatureId> points) {
    application::CreateDatumPlaneAction plane{
        std::nullopt, std::nullopt, std::nullopt, {0.0, application::QuantityUnit::millimetre}};
    plane.points = std::move(points);
    return session.createDatumPlane(plane).feature();
}
document::SketchPlane frame(const application::CadSession& session, document::FeatureId id) {
    const auto found = session.datumFrame(id);
    require(found.has_value(), "Construction frame");
    return *found;
}

document::FeatureId midplane(application::CadSession& session, document::FeatureId box) {
    application::CreateDatumPlaneAction plane{std::nullopt,
                                              box,
                                              faceAlong(session, box, -1, 0),
                                              {0.0, application::QuantityUnit::millimetre}};
    plane.secondSupport = box;
    plane.secondFace = faceAlong(session, box, 1, 0);
    const auto made = session.createDatumPlane(plane);
    require(made.feature().has_value() && near(frame(session, *made.feature()).originZ, 5),
            "The midplane of bottom and top at z = 5: " + made.diagnostic());
    plane.secondFace = faceAlong(session, box, 0, -1);
    require(session.createDatumPlane(plane).status() != CommandStatus::committed,
            "Bottom and front are not parallel");
    return *made.feature();
}

void throughPoints(application::CadSession& session) {
    const auto a = point(session, 0, 0, 0);
    const auto b = point(session, 10, 0, 0);
    const auto c = point(session, 0, 10, 0);
    const auto d = point(session, 20, 0, 0);
    require(a && b && c && d, "Four construction points");
    const auto plane = planeThrough(session, {*a, *b, *c});
    require(plane.has_value() && near(std::abs(frame(session, *plane).normalZ), 1) &&
                near(frame(session, *plane).originX, 0),
            "Through three points: the XY plane");
    require(!planeThrough(session, {*a, *b, *d}), "Three points on a line are refused");
}

std::optional<document::FeatureId> edgeMiddle(application::CadSession& session,
                                              document::FeatureId box) {
    for (TopExp_Explorer explorer(session.validBodies().front().shape, TopAbs_EDGE);
         explorer.More(); explorer.Next()) {
        const BRepAdaptor_Curve curve(TopoDS::Edge(explorer.Current()));
        const gp_Pnt middle = curve.Value((curve.FirstParameter() + curve.LastParameter()) / 2);
        if (near(middle.Y(), 0) && near(middle.Z(), 10) && near(middle.X(), 10))
            return session
                .createDatumPoint(application::CreateDatumPointAction{
                    std::nullopt, box, session.filletEdge(box, explorer.Current()),
                    document::EdgeSpot::middle})
                .feature();
    }
    return std::nullopt;
}

void construction() {
    application::CadSession session;
    session.newDocument();
    const auto box = *session.createBoxFeature({20, 20, 10}).feature();
    const auto onEdge = edgeMiddle(session, box);
    require(onEdge && near(frame(session, *onEdge).originX, 10) &&
                near(frame(session, *onEdge).originZ, 10),
            "The middle of the top front edge at (10; 0; 10)");
    const auto middle = midplane(session, box);
    throughPoints(session);
    auto taller = session.identifiedSnapshot().snapshot().boxParameters(box);
    taller.dimensions.height = 16;
    require(session.editBoxParameters(box, taller).status() == CommandStatus::committed &&
                near(frame(session, middle).originZ, 8) &&
                near(frame(session, *onEdge).originZ, 16),
            "The midplane and the point follow the taller box");
    const auto path = test::temporaryFilePath("voelund-n13b-construction.volund");
    session.saveDocument(path);
    application::CadSession loaded;
    loaded.loadDocument(path);
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    require(loaded.features().size() == session.features().size() &&
                near(frame(loaded, middle).originZ, 8) &&
                loaded.identifiedSnapshot().snapshot().datumPointParameters(*onEdge) ==
                    session.identifiedSnapshot().snapshot().datumPointParameters(*onEdge),
            "Save/Open keeps the midplane and the points");
}
} // namespace

int main() {
    try {
        construction();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "PASS N13b midplane, plane through points, points on edges, follow, open\n";
    return 0;
}
