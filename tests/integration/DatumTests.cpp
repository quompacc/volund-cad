// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CadSessionTestSupport.hpp"

#include <voelund/application/Actions.hpp>
#include <voelund/features/SketchRegionIdentity.hpp>

#include <BRepAdaptor_Curve.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>

// M19 (user decision 2026-10-14): construction planes and axes. A plane 25 above XY carries a
// sketch whose extrusion starts there and follows the plane; a plane on a box face follows the
// box; a plane turned 90 degrees about x; axes on base Z, a straight box edge and the circular
// edge of a cylinder; save/open (schema 19) and undo.
namespace {
using namespace voelund;
using application::CommandStatus;
constexpr double pi = std::numbers::pi;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-6 * std::max(1.0, std::abs(b)); }
std::array<double, 2> heightRange(const TopoDS_Shape& shape) {
    Bnd_Box box;
    BRepBndLib::AddOptimal(shape, box, false, false);
    double x0 = 0, y0 = 0, z0 = 0, x1 = 0, y1 = 0, z1 = 0;
    box.Get(x0, y0, z0, x1, y1, z1);
    return {z0, z1};
}
document::SketchPlane frame(const application::CadSession& session, document::FeatureId id) {
    const auto found = session.datumFrame(id);
    require(found.has_value(), "Construction frame");
    return *found;
}

void planeCarriesSketch() {
    application::CadSession session;
    session.newDocument();
    const auto created = application::executeAction(
        session,
        application::prepareRequest(session, application::CreateDatumPlaneAction{
                                                 document::DatumPlane::xy,
                                                 std::nullopt,
                                                 std::nullopt,
                                                 {25, application::QuantityUnit::millimetre}}));
    require(created.command.status() == CommandStatus::committed &&
                session.features().back().name == "Bezugsebene 1" &&
                session.features().back().kind == document::FeatureKind::datumPlane &&
                near(frame(session, session.features().back().id).originZ, 25) &&
                session.validBodies().empty() && session.modelOutline().bodies.empty(),
            "A plane 25 above XY belongs to no body");
    const auto plane = session.features().back().id;

    document::SketchParameters sketch{frame(session, plane), {}};
    sketch.constructionPlane = plane;
    sketch.elements.values.push_back(
        {document::SketchElementId{1}, document::SketchCircle{{0, 0}, 5}});
    sketch.elements.nextId = 2;
    require(session.createDatumSketchFeature(sketch).status() == CommandStatus::committed,
            "Sketch on the plane");
    const auto sketchId = session.features().back().id;
    const auto regions = features::identifySketchRegions(sketch.elements.values).ids;
    const auto extruded = session.createSketchProfileExtrusion(sketchId, regions, 10);
    if (extruded.status() != CommandStatus::committed) {
        std::cerr << extruded.diagnostic() << std::endl;
        for (const auto& issue : extruded.evaluation().features)
            std::cerr << issue.feature.value() << ' ' << issue.message << std::endl;
    }
    require(extruded.status() == CommandStatus::committed && session.validBodies().size() == 1 &&
                near(test::volumeOf(session.validBodies().front().shape), 250 * pi),
            "The sketch extrudes into a new body");
    auto range = heightRange(session.validBodies().front().shape);
    require(near(range[0], 25) && near(range[1], 35), "The extrusion starts on the plane");

    auto parameters = session.identifiedSnapshot().snapshot().datumPlaneParameters(plane);
    parameters.offset = 40;
    require(session.editDatumPlaneParameters(plane, parameters).status() ==
                CommandStatus::committed,
            "Move the plane");
    range = heightRange(session.validBodies().front().shape);
    require(near(range[0], 40) && near(range[1], 50), "The body follows its plane");

    // Turned 90 degrees about x: the normal points along -y.
    parameters.rotationAxis = document::DatumRotationAxis::x;
    parameters.angle = kernel::Radians{pi / 2};
    parameters.offset = 0;
    require(session.editDatumPlaneParameters(plane, parameters).status() ==
                    CommandStatus::committed &&
                near(frame(session, plane).normalY, -1) &&
                std::abs(frame(session, plane).normalZ) < 1e-9,
            "A turned plane");
    require(session.undo().status() == CommandStatus::committed &&
                near(frame(session, plane).originZ, 40),
            "Undo returns to offset 40");

    const auto directory = std::filesystem::temp_directory_path() / "voelund-m19-datum";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const auto path = directory / "datum.volund";
    session.saveDocument(path);
    application::CadSession reopened;
    reopened.loadDocument(path);
    require(reopened.identifiedSnapshot().snapshot().datumPlaneParameters(plane) ==
                    session.identifiedSnapshot().snapshot().datumPlaneParameters(plane) &&
                reopened.sketchParameters(sketchId).constructionPlane == plane &&
                near(heightRange(reopened.validBodies().front().shape)[0], 40),
            "Open restores the plane and its sketch");
    std::filesystem::remove_all(directory);
}

void planeOnFaceAndAxes() {
    application::CadSession session;
    session.newDocument();
    const auto box = *session.createBoxFeature({40, 30, 10}).feature();
    std::optional<document::SubshapeOrigin> top;
    for (const auto& face : session.supportFaces(box))
        if (face.plane.normalZ > 0.5)
            top = face.origin;
    require(top.has_value(), "Top face");
    require(
        session.createDatumPlane(std::nullopt, box, top, 5, document::DatumRotationAxis::none, 0)
                .status() == CommandStatus::committed,
        "Plane on the top face");
    const auto plane = session.features().back().id;
    require(near(frame(session, plane).originZ, 15), "5 above the top face");
    auto taller = session.identifiedSnapshot().snapshot().boxParameters(box);
    taller.dimensions.height = 20;
    require(session.editBoxParameters(box, taller).status() == CommandStatus::committed &&
                near(frame(session, plane).originZ, 25),
            "The plane follows the box");

    require(session.createDatumAxis(document::BaseAxis::z, std::nullopt, std::nullopt).status() ==
                    CommandStatus::committed &&
                near(frame(session, session.features().back().id).normalZ, 1),
            "Base axis Z");

    // A vertical box edge.
    std::optional<document::FilletEdge> vertical;
    for (TopExp_Explorer edges(session.validBodies().front().shape, TopAbs_EDGE);
         edges.More() && !vertical; edges.Next()) {
        const BRepAdaptor_Curve curve(TopoDS::Edge(edges.Current()));
        if (curve.GetType() == GeomAbs_Line && std::abs(curve.Line().Direction().Z()) > 0.99)
            vertical = session.filletEdge(box, edges.Current());
    }
    require(vertical.has_value(), "A vertical named edge");
    require(session.createDatumAxis(std::nullopt, box, vertical).status() ==
                    CommandStatus::committed &&
                near(std::abs(frame(session, session.features().back().id).normalZ), 1),
            "Axis on a straight edge");

    // The circular top edge of a cylinder: its axis.
    const auto cylinder = *session.createNewBodyCylinder().feature();
    std::optional<document::FilletEdge> circle;
    for (const auto& body : session.validBodies())
        if (body.sourceFeature == cylinder)
            for (TopExp_Explorer edges(body.shape, TopAbs_EDGE); edges.More() && !circle;
                 edges.Next())
                if (BRepAdaptor_Curve(TopoDS::Edge(edges.Current())).GetType() == GeomAbs_Circle)
                    circle = session.filletEdge(cylinder, edges.Current());
    require(circle.has_value(), "A named circular edge");
    require(session.createDatumAxis(std::nullopt, cylinder, circle).status() ==
                CommandStatus::committed,
            "Axis on a circular edge");
    const auto axis = frame(session, session.features().back().id);
    require(near(std::abs(axis.normalZ), 1) && std::abs(axis.originX) < 1e-9 &&
                std::abs(axis.originY) < 1e-9,
            "The axis runs through the circle centre");

    // A base plane reference must not name a face as well.
    require(session.createDatumPlane(document::DatumPlane::xy, box, top, 0,
                                     document::DatumRotationAxis::none, 0)
                    .status() == CommandStatus::rejected,
            "One reference only");
}

} // namespace

int main() {
    try {
        planeCarriesSketch();
        planeOnFaceAndAxes();
        std::cout << "PASS M19 construction planes and axes\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
