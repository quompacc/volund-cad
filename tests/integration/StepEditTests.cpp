// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CadSessionTestSupport.hpp"

#include <voelund/application/Actions.hpp>
#include <voelund/kernel/StepExchange.hpp>

#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRep_Tool.hxx>
#include <NCollection_IndexedMap.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS.hxx>
#include <gp_Ax2.hxx>
#include <gp_Pln.hxx>

#include <cmath>
#include <filesystem>
#include <functional>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>

// I05 (packet acceptance): a foreign part arrives as STEP: a plate 40 x 30 x 10 with a through
// hole of diameter 8 in its middle and a rounded top front edge (radius 2). The volumes are worked
// out by hand. The hole wall moved along its normal changes the diameter, the top moved up takes
// the rounding along. Too far is refused and changes nothing. Save/Open keep the edits.
namespace {
using namespace voelund;
using application::CommandStatus;
using document::FaceEditMode;

constexpr double pi = std::numbers::pi;
constexpr double plate = 40.0 * 30.0 * 10.0;
constexpr double hole = pi * 4.0 * 4.0 * 10.0;
// A rounded corner of radius 2 along 40 mm takes away the square minus the quarter circle.
constexpr double rounding = (4.0 - pi) * 40.0;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-3 * std::max(1.0, std::abs(b)); }

TopoDS_Shape part() {
    const TopoDS_Shape box = BRepPrimAPI_MakeBox(40, 30, 10).Shape();
    BRepFilletAPI_MakeFillet fillet(box);
    for (TopExp_Explorer edges(box, TopAbs_EDGE); edges.More(); edges.Next()) {
        const auto& edge = TopoDS::Edge(edges.Current());
        TopoDS_Vertex first;
        TopoDS_Vertex last;
        TopExp::Vertices(edge, first, last);
        const auto a = BRep_Tool::Pnt(first);
        const auto b = BRep_Tool::Pnt(last);
        if (a.Y() < 1e-9 && b.Y() < 1e-9 && a.Z() > 9.99 && b.Z() > 9.99)
            fillet.Add(2, edge);
    }
    const gp_Ax2 axis(gp_Pnt(20, 15, -1), gp_Dir(0, 0, 1));
    const TopoDS_Shape cut =
        BRepAlgoAPI_Cut(fillet.Shape(), BRepPrimAPI_MakeCylinder(axis, 4, 12).Shape()).Shape();
    return TopExp_Explorer(cut, TopAbs_SOLID).Current();
}

double volume(const application::CadSession& session) {
    double total = 0;
    for (const auto& body : session.validBodies())
        total += test::volumeOf(body.shape);
    return total;
}

// The numbers of the faces that `chosen` accepts, in the stored solid's order.
std::vector<document::SubshapeOrigin>
facesWhere(const application::CadSession& session, document::FeatureId feature,
           const std::function<bool(const BRepAdaptor_Surface&)>& chosen) {
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> faces;
    TopExp::MapShapes(*session.featureShape(feature), TopAbs_FACE, faces);
    std::vector<document::SubshapeOrigin> result;
    for (int index = 1; index <= faces.Extent(); ++index)
        if (chosen(BRepAdaptor_Surface(TopoDS::Face(faces(index)))))
            result.push_back({.feature = feature,
                              .role = {},
                              .importedFace = static_cast<std::uint32_t>(index)});
    return result;
}

bool isHoleWall(const BRepAdaptor_Surface& surface) {
    return surface.GetType() == GeomAbs_Cylinder &&
           std::abs(surface.Cylinder().Radius() - 4) < 1e-6;
}
bool isTop(const BRepAdaptor_Surface& surface) {
    return surface.GetType() == GeomAbs_Plane &&
           std::abs(surface.Plane().Location().Z() - 10) < 1e-6 &&
           surface.Plane().Axis().Direction().IsParallel(gp_Dir(0, 0, 1), 1e-6);
}

application::CommandResult edit(application::CadSession& session, document::FeatureId feature,
                                std::vector<document::SubshapeOrigin> faces, double distance) {
    return session.createFaceEdit(application::CreateFaceEditAction{
        feature, std::move(faces), FaceEditMode::offset, distance});
}

void run(const std::filesystem::path& directory) {
    const auto file = directory / "teil.step";
    const std::vector<kernel::StepSolid> solids{{part(), "Teil"}};
    require(!kernel::writeStep(file, solids).error, "Part written");
    application::CadSession session;
    session.newDocument();
    require(session.importStep(file).status == application::StepImportStatus::imported,
            "Part imported");
    const auto feature = session.features().front().id;
    const double original = plate - hole - rounding;
    require(near(volume(session), original), "The imported part has the expected volume");

    // The wall moves along its normal, which points into the hole: the hole gets smaller.
    const auto wall = facesWhere(session, feature, isHoleWall);
    require(!wall.empty(), "The hole wall is found");
    auto result = edit(session, feature, wall, 1);
    require(result.status() == CommandStatus::committed,
            "Hole wall 1 inwards: " + result.diagnostic());
    require(near(volume(session), plate - rounding - pi * 9 * 10), "Diameter 6");
    require(session.undo().status() == CommandStatus::committed, "Undo");
    result = edit(session, feature, wall, -1);
    require(result.status() == CommandStatus::committed, "Hole wall 1 outwards");
    require(near(volume(session), plate - rounding - pi * 25 * 10), "Diameter 10");
    require(session.undo().status() == CommandStatus::committed, "Undo");

    // The top moves up and the rounding rolls along: the footprint gains 3 mm of height.
    const auto top = facesWhere(session, feature, isTop);
    require(top.size() == 1, "The top is found");
    result = edit(session, feature, top, 3);
    require(result.status() == CommandStatus::committed, "Top 3 up: " + result.diagnostic());
    require(near(volume(session), original + 3 * (1200 - pi * 16)), "Top 3 up: 13 high");

    // Save and open keep the edit, without the STEP file.
    const auto saved = directory / "teil.volund";
    session.saveDocument(saved);
    std::filesystem::remove(file);
    application::CadSession reopened;
    reopened.loadDocument(saved);
    require(near(volume(reopened), original + 3 * (1200 - pi * 16)), "Save and open keep the edit");
    require(session.undo().status() == CommandStatus::committed, "Undo");

    // Through the whole part: refused, the part stays as it was.
    result = edit(session, feature, top, -20);
    require(result.status() == CommandStatus::rejected, "Top 20 down refused");
    require(near(volume(session), original), "A refusal changes nothing");
    // A hole cannot grow past the part either.
    result = edit(session, feature, wall, -50);
    require(result.status() == CommandStatus::rejected, "Hole of diameter 108 refused");
    require(near(volume(session), original), "A refusal changes nothing");
}
} // namespace

int main() {
    const auto directory = std::filesystem::temp_directory_path() / "voelund-step-edit-tests";
    std::filesystem::create_directories(directory);
    int status = 0;
    try {
        run(directory);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        status = 1;
    }
    std::filesystem::remove_all(directory);
    return status;
}
