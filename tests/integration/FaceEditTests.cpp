// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CadSessionTestSupport.hpp"

#include <voelund/application/Actions.hpp>

#include <BRepAdaptor_Curve.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

// N13a (schema 52, packet acceptance): a box 20 x 20 x 10. Its top face moved 5 up is 6000,
// 3 down 2800, and the edit follows the box. A 2 mm chamfer on the top front edge (3960)
// deleted heals back to the sharp edge: 4000 and the corner material is back. Undo and
// Save/Open keep the edits.
namespace {
using namespace voelund;
using application::CommandStatus;

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
bool solidAt(const application::CadSession& session, double x, double y, double z) {
    for (const auto& body : session.validBodies())
        if (BRepClass3d_SolidClassifier(body.shape, gp_Pnt(x, y, z), 1e-7).State() == TopAbs_IN)
            return true;
    return false;
}
// The planar face of `support`'s result whose normal points along (nx, ny, nz).
document::SubshapeOrigin faceAlong(const application::CadSession& session,
                                   document::FeatureId support, double nx, double ny, double nz) {
    for (const auto& face : session.supportFaces(support))
        if (face.plane.normalX * nx + face.plane.normalY * ny + face.plane.normalZ * nz > 0.99)
            return face.origin;
    throw std::runtime_error("Face missing");
}
application::CommandResult editFaces(application::CadSession& session, document::FeatureId support,
                                     const document::SubshapeOrigin& face,
                                     document::FaceEditMode mode, double distance) {
    return session.createFaceEdit(
        application::CreateFaceEditAction{support, {face}, mode, distance});
}

void offset() {
    application::CadSession session;
    session.newDocument();
    const auto box = *session.createBoxFeature({20, 20, 10}).feature();
    const auto top = faceAlong(session, box, 0, 0, 1);
    require(editFaces(session, box, top, document::FaceEditMode::offset, 5).status() ==
                    CommandStatus::committed &&
                near(volume(session), 6000) && session.features().back().name == "Flächen 1",
            "The top face 5 up: 6000");
    const auto edit = session.features().back().id;
    auto parameters = session.identifiedSnapshot().snapshot().faceEditParameters(edit);
    parameters.distance = -3;
    require(session.editFaceEdit(application::EditFaceEditParametersAction{edit, parameters})
                        .status() == CommandStatus::committed &&
                near(volume(session), 2800),
            "3 down: 2800");
    auto bigger = session.identifiedSnapshot().snapshot().boxParameters(box);
    bigger.dimensions.width = 30;
    require(session.editBoxParameters(box, bigger).status() == CommandStatus::committed &&
                near(volume(session), 30 * 20 * 7),
            "The edit follows the wider box");
    const auto path = test::temporaryFilePath("voelund-n13a-offset.volund");
    session.saveDocument(path);
    application::CadSession loaded;
    loaded.loadDocument(path);
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    require(near(volume(loaded), 30 * 20 * 7) &&
                loaded.identifiedSnapshot().snapshot().faceEditParameters(edit) ==
                    session.identifiedSnapshot().snapshot().faceEditParameters(edit),
            "Save/Open keeps the face edit");
}

// The box's top front edge, named by its faces.
std::optional<document::FilletEdge> topFrontEdge(const application::CadSession& session,
                                                 document::FeatureId box) {
    for (TopExp_Explorer explorer(session.validBodies().front().shape, TopAbs_EDGE);
         explorer.More(); explorer.Next()) {
        const BRepAdaptor_Curve curve(TopoDS::Edge(explorer.Current()));
        const gp_Pnt middle = curve.Value((curve.FirstParameter() + curve.LastParameter()) / 2);
        if (near(middle.Y(), 0) && near(middle.Z(), 10) && near(middle.X(), 10))
            return session.filletEdge(box, explorer.Current());
    }
    return std::nullopt;
}

void healChamfer() {
    application::CadSession session;
    session.newDocument();
    const auto box = *session.createBoxFeature({20, 20, 10}).feature();
    const auto edge = topFrontEdge(session, box);
    require(edge.has_value() &&
                session.createChamfer(box, std::span(&*edge, 1), 2).status() ==
                    CommandStatus::committed &&
                near(volume(session), 3960) && !solidAt(session, 10, 0.3, 9.7),
            "A 2 mm chamfer: 3960");
    const auto chamfer = session.features().back().id;
    const double diagonal = std::sqrt(0.5);
    require(editFaces(session, chamfer, faceAlong(session, chamfer, 0, -diagonal, diagonal),
                      document::FaceEditMode::remove, 0)
                        .status() == CommandStatus::committed &&
                near(volume(session), 4000) && solidAt(session, 10, 0.3, 9.7),
            "The chamfer face deleted heals to the edge");
    require(session.undo().status() == CommandStatus::committed && near(volume(session), 3960),
            "Undo brings the chamfer back");
    require(editFaces(session, chamfer, faceAlong(session, chamfer, 0, 0, -1),
                      document::FaceEditMode::remove, 0)
                    .status() != CommandStatus::committed,
            "The bottom face cannot be deleted: nothing closes the gap");
}
} // namespace

int main() {
    try {
        offset();
        healChamfer();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "PASS N13a face edits: offset 5 and -3, follows, open, chamfer healed, undo\n";
    return 0;
}
