// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "../TestPaths.hpp"

#include <voelund/application/Actions.hpp>
#include <voelund/application/CadSession.hpp>

#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

// M01 (user decision 2026-10-10): a cylinder joins the selected body; without one it is a new,
// independent body at the origin, also in an empty document. It has no input (schema 12),
// survives save/open, and cannot be switched to join/cut/intersect, which need a body.
namespace {
using namespace voelund;
using application::CommandStatus;
using document::CylinderResultMode;
using document::FeatureId;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
application::ActionResult act(application::CadSession& session, application::Action action) {
    return application::executeAction(
        session, {"m01", session.documentId(), session.sceneStatus().document, std::move(action)});
}
Bnd_Box bounds(const TopoDS_Shape& shape) {
    Bnd_Box box;
    BRepBndLib::Add(shape, box);
    return box;
}

void emptyDocumentCylinder() {
    application::CadSession session;
    session.newDocument();
    require(session.canAddCylinder(), "An empty document offers a cylinder");
    const auto result =
        act(session, application::CreateCylinderAction{std::nullopt, CylinderResultMode::separate});
    require(result.command.status() == CommandStatus::committed,
            "New-body cylinder: " + result.command.diagnostic());
    require(session.features().size() == 1 && !session.features().front().inputFeature &&
                session.validBodies().size() == 1,
            "One cylinder without input, one body");
    double x0 = 0, y0 = 0, z0 = 0, x1 = 0, y1 = 0, z1 = 0;
    bounds(session.validBodies().front().shape).Get(x0, y0, z0, x1, y1, z1);
    require(std::abs((x0 + x1) / 2) < 1e-6 && std::abs((y0 + y1) / 2) < 1e-6 && std::abs(z0) < 1e-3,
            "It stands on the origin");

    // A join without input is refused, the document unchanged.
    const auto join =
        act(session, application::CreateCylinderAction{std::nullopt, CylinderResultMode::join});
    require(join.command.status() != CommandStatus::committed && session.features().size() == 1,
            "A join needs a body");
    const auto cylinder = session.features().front().id;
    auto parameters = session.cylinderParameters(cylinder);
    parameters.resultMode = CylinderResultMode::join;
    require(
        session.editCylinderParameters(cylinder, parameters, application::ErrorDisposition::reject)
                .status() != CommandStatus::committed,
        "A new-body cylinder cannot be switched to join");

    const auto path = test::temporaryFilePath("voelund-m01-cylinder.volund");
    session.saveDocument(path);
    application::CadSession loaded;
    loaded.loadDocument(path);
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    require(loaded.features().size() == 1 && !loaded.features().front().inputFeature &&
                loaded.validBodies().size() == 1,
            "Save/open keeps the cylinder without input");
}

void selectedBodyIsTheTarget() {
    application::CadSession session;
    session.newDocument();
    require(session.createBoxFeature({10, 10, 10}).status() == CommandStatus::committed, "A");
    const FeatureId a = session.features().back().id;
    require(session.createBoxFeature({10, 10, 10}).status() == CommandStatus::committed, "B");
    require(session.selectFeature(a), "Select A");
    const auto producer = session.selectedBodyProducer();
    require(producer == a, "The selected body's producer is A, not the last feature B");
    require(act(session, application::CreateCylinderAction{producer, CylinderResultMode::join})
                    .command.status() == CommandStatus::committed,
            "Join to A");
    require(session.validBodies().size() == 2 && session.features().back().inputFeature == a,
            "The cylinder joined A; B untouched");
}
} // namespace

int main() {
    try {
        emptyDocumentCylinder();
        selectedBodyIsTheTarget();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "PASS M01 cylinder: new body without input, selected body as target\n";
    return 0;
}
